/**
 * @file test_timectl.c
 * @brief Common NTP client test binary
 *
 * Tests NTP client operations through a backend-agnostic function-pointer
 * table defined entirely in this file. No separate adapter files are needed.
 *
 * Adding a new NTP client backend
 * --------------------------------
 *   1. #include its header below (e.g. "libntpdctl.h").
 *   2. Fill in a new ntp_ops_t row in the backends[] table.
 *   3. Recompile — nothing else changes.
 *
 * Usage
 * -----
 *   ./test_timectl [--backend=<name>] <command> [args...]
 *
 * Commands:
 *   offset_check                            - Print current clock offset
 *   makestep                                - Force an immediate clock step
 *   server [host [minpoll [maxpoll]]]       - Add an NTP server
 *   delete_server [host]                    - Remove an NTP server
 *   burst [n_good [n_total [addr [mask]]]]  - Trigger a burst of polls
 *   set_poll <host> <minpoll> <maxpoll>     - Update server poll intervals
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- Backend headers: add a new #include here for each NTP client --- */
#include "libchronyctl.h"

/* ------------------------------------------------------------------ */
/* Generic NTP ops table                                              */
/* Each NTP client fills one row; all function signatures are common. */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *name;
    int  (*init)(void);
    int  (*cleanup)(void);
    int  (*get_offset)(double *offset_sec);
    int  (*makestep)(void);
    int  (*add_server)(const char *host, int minpoll, int maxpoll);
    int  (*delete_server)(const char *host);
    int  (*set_poll)(const char *host, int minpoll, int maxpoll);
    /* burst: addr/mask NULL means all sources */
    int  (*burst)(const char *addr, const char *mask, int n_good, int n_total);
    const char *(*strerror)(int err);
} ntp_ops_t;

/* ------------------------------------------------------------------ */
/* Chrony burst wrapper: converts string addr/mask to IPAddr          */
/* ------------------------------------------------------------------ */

#include "addressing.h"
#include <arpa/inet.h>
#include <netdb.h>

static int chrony_burst(const char *addr_str, const char *mask_str,
                        int n_good, int n_total)
{
    IPAddr addr_buf, mask_buf;
    IPAddr *paddr = NULL, *pmask = NULL;

    if (addr_str) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        memset(&addr_buf, 0, sizeof(addr_buf));
        if (getaddrinfo(addr_str, NULL, &hints, &res) == 0) {
            addr_buf.addr.in4 = ntohl(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
            addr_buf.family   = IPADDR_INET4;
            freeaddrinfo(res);
            paddr = &addr_buf;
        } else {
            return CHRONYCTL_ERROR_INVALID;
        }
    }
    if (mask_str) {
        struct addrinfo hints, *res;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        memset(&mask_buf, 0, sizeof(mask_buf));
        if (getaddrinfo(mask_str, NULL, &hints, &res) == 0) {
            mask_buf.addr.in4 = ntohl(((struct sockaddr_in *)res->ai_addr)->sin_addr.s_addr);
            mask_buf.family   = IPADDR_INET4;
            freeaddrinfo(res);
            pmask = &mask_buf;
        } else {
            return CHRONYCTL_ERROR_INVALID;
        }
    }
    return chronyctl_burst(paddr, pmask, n_good, n_total);
}

/* ------------------------------------------------------------------ */
/* Backend table                                                      */
/* Add a new row here for each NTP client.                            */
/* ------------------------------------------------------------------ */

static const ntp_ops_t backends[] = {
    {
        .name          = "chrony",
        .init          = chronyctl_init,
        .cleanup       = chronyctl_cleanup,
        .get_offset    = chronyctl_get_offset,
        .makestep      = chronyctl_makestep,
        .add_server    = chronyctl_add_server,
        .delete_server = chronyctl_delete_server,
        .set_poll      = chronyctl_set_poll,
        .burst         = chrony_burst,
        .strerror      = chronyctl_strerror,
    },
    /* Future example:
    {
        .name          = "ntpd",
        .init          = ntpdctl_init,
        .cleanup       = ntpdctl_cleanup,
        .get_offset    = ntpdctl_get_offset,
        .makestep      = ntpdctl_makestep,
        .add_server    = ntpdctl_add_server,
        .delete_server = ntpdctl_delete_server,
        .set_poll      = ntpdctl_set_poll,
        .burst         = ntpdctl_burst,
        .strerror      = ntpdctl_strerror,
    }, */
};

static const int NUM_BACKENDS = (int)(sizeof(backends) / sizeof(backends[0]));

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static const ntp_ops_t *find_backend(const char *name)
{
    for (int i = 0; i < NUM_BACKENDS; i++)
        if (strcmp(backends[i].name, name) == 0)
            return &backends[i];
    return NULL;
}

static void list_backends(void)
{
    printf("Available backends:\n");
    for (int i = 0; i < NUM_BACKENDS; i++)
        printf("  %s\n", backends[i].name);
}

static void report(const ntp_ops_t *ops, int err, const char *desc)
{
    if (err == 0)
        printf("  [OK]   %s\n", desc);
    else
        fprintf(stderr, "  [FAIL] %s: %s (code %d)\n",
                desc, ops->strerror(err), err);
}

static void print_usage(const char *prog)
{
    printf("Usage: %s [--backend=<name>] <command> [args...]\n\n", prog);
    printf("Commands:\n");
    printf("  offset_check\n");
    printf("  makestep\n");
    printf("  server [host [minpoll [maxpoll]]]        (defaults: time.xfinity.com 6 10)\n");
    printf("  delete_server [host]                     (default: pool.ntp.org)\n");
    printf("  burst [n_good [n_total [addr [mask]]]]   (defaults: 4 8 all any)\n");
    printf("  set_poll <host> <minpoll> <maxpoll>\n\n");
    list_backends();
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    const char *backend_name = backends[0].name;
    int         arg_offset   = 1;

    if (argc >= 2 && strncmp(argv[1], "--backend=", 10) == 0) {
        backend_name = argv[1] + 10;
        arg_offset   = 2;
    }

    if (argc - arg_offset < 1) {
        print_usage(argv[0]);
        return 1;
    }

    const ntp_ops_t *ops = find_backend(backend_name);
    if (!ops) {
        fprintf(stderr, "Unknown backend: '%s'\n", backend_name);
        list_backends();
        return 1;
    }

    const char *cmd = argv[arg_offset];
    printf("=== test_timectl  backend='%s'  cmd='%s' ===\n", backend_name, cmd);

    int ret = ops->init();
    report(ops, ret, "init");
    if (ret != 0)
        return 1;

    if (strcmp(cmd, "offset_check") == 0) {
        double offset = 0.0;
        ret = ops->get_offset(&offset);
        report(ops, ret, "get_offset");
        if (ret == 0)
            printf("  Offset: %.9f seconds\n", offset);

    } else if (strcmp(cmd, "makestep") == 0) {
        ret = ops->makestep();
        report(ops, ret, "makestep");

    } else if (strcmp(cmd, "server") == 0) {
        const char *host    = (argc - arg_offset > 1) ? argv[arg_offset + 1] : "time.xfinity.com";
        int         minpoll = (argc - arg_offset > 2) ? atoi(argv[arg_offset + 2]) : 6;
        int         maxpoll = (argc - arg_offset > 3) ? atoi(argv[arg_offset + 3]) : 10;
        printf("  host=%s  minpoll=%d  maxpoll=%d\n", host, minpoll, maxpoll);
        ret = ops->add_server(host, minpoll, maxpoll);
        report(ops, ret, "add_server");

    } else if (strcmp(cmd, "delete_server") == 0) {
        const char *host = (argc - arg_offset > 1) ? argv[arg_offset + 1] : "time.xfinity.com";
        printf("  host=%s\n", host);
        ret = ops->delete_server(host);
        report(ops, ret, "delete_server");

    } else if (strcmp(cmd, "burst") == 0) {
        int         n_good  = (argc - arg_offset > 1) ? atoi(argv[arg_offset + 1]) : 4;
        int         n_total = (argc - arg_offset > 2) ? atoi(argv[arg_offset + 2]) : 8;
        const char *addr    = (argc - arg_offset > 3) ? argv[arg_offset + 3] : NULL;
        const char *mask    = (argc - arg_offset > 4) ? argv[arg_offset + 4] : NULL;
        printf("  n_good=%d  n_total=%d  addr=%s  mask=%s\n",
               n_good, n_total, addr ? addr : "(all)", mask ? mask : "(any)");
        ret = ops->burst(addr, mask, n_good, n_total);
        report(ops, ret, "burst");

    } else if (strcmp(cmd, "set_poll") == 0) {
        if (argc - arg_offset < 4) {
            fprintf(stderr, "Usage: %s [--backend=...] set_poll <host> <minpoll> <maxpoll>\n",
                    argv[0]);
            ops->cleanup();
            return 1;
        }
        const char *host    = argv[arg_offset + 1];
        int         minpoll = atoi(argv[arg_offset + 2]);
        int         maxpoll = atoi(argv[arg_offset + 3]);
        printf("  host=%s  minpoll=%d  maxpoll=%d\n", host, minpoll, maxpoll);
        ret = ops->set_poll(host, minpoll, maxpoll);
        report(ops, ret, "set_poll");

    } else {
        fprintf(stderr, "Unknown command: '%s'\n\n", cmd);
        print_usage(argv[0]);
        ops->cleanup();
        return 1;
    }

    ops->cleanup();
    printf("=== done ===\n");
    return (ret == 0) ? 0 : 1;
}

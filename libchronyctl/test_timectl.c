/*
 * Copyright 2026 Comcast Cable Communications Management, LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

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
 *   online                                  - Mark sources as available
 *   offset_check                            - Print current clock offset
 *   makestep                                - Force an immediate clock step
 *   server [host [minpoll [maxpoll]]]       - Add an NTP server
 *   delete_server [host]                    - Remove an NTP server
 *   burst [n_good [n_total [addr [mask]]]]  - Trigger a burst of polls
 *   set_poll <host> <minpoll> <maxpoll>     - Update server poll intervals
 *   waitsync [max_tries [interval_sec]]     - Wait until clock is synchronised
 *   source_count                            - Print number of tracked sources
 *   selectable_check                        - Check if any selectable source exists
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
    int  (*burst)(const char *addr, const char *mask, int good_count, int total_count);
    /* online: addr/mask NULL means all sources */
    int  (*online)(const char *addr, const char *mask);
    /* has_selectable_source: 1 if any source is selected/selectable */
    int  (*has_selectable_source)(int *has_selectable);
    /* source_count: total number of sources chronyd is tracking */
    int  (*source_count)(int *count);
    /* waitsync: block until synchronized; returns CHRONYCTL_SUCCESS or error */
    int  (*waitsync)(int max_tries, int interval_sec);
    const char *(*strerror)(int err);
} ntp_ops_t;

/* ------------------------------------------------------------------ */
/* Chrony burst wrapper: converts string addr/mask to IPAddr          */
/* ------------------------------------------------------------------ */

#include "chrony_address.h"
#include <arpa/inet.h>
#include <netdb.h>

static int chrony_burst(const char *addr_str, const char *mask_str,
                        int good_count, int total_count)
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
    return chronyctl_burst(paddr, pmask, good_count, total_count);
}

/* ------------------------------------------------------------------ */
/* Chrony online wrapper: converts string addr/mask to IPAddr         */
/* ------------------------------------------------------------------ */

static int chrony_online(const char *addr_str, const char *mask_str)
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
    return chronyctl_online(paddr, pmask);
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
        .online                = chrony_online,
        .has_selectable_source = chronyctl_has_selectable_source,
        .source_count          = chronyctl_get_source_count,
        .waitsync              = chronyctl_waitsync,
        .strerror              = chronyctl_strerror,
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
    printf("  delete_server [host]                     (default: time.xfinity.com)\n");
    printf("  burst [n_good [n_total [addr [mask]]]]   (defaults: 4 8 all any)\n");
    printf("  online [addr [mask]]                     (defaults: all sources)\n");
    printf("  selectable_check                         check if any selectable source exists\n");
    printf("  source_count                             print number of tracked sources\n");
    printf("  waitsync [max_tries [interval_sec]]      wait until synchronised (defaults: 30 1)\n");
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
        int         good_count  = (argc - arg_offset > 1) ? atoi(argv[arg_offset + 1]) : 4;
        int         total_count = (argc - arg_offset > 2) ? atoi(argv[arg_offset + 2]) : 8;
        const char *addr    = (argc - arg_offset > 3) ? argv[arg_offset + 3] : NULL;
        const char *mask    = (argc - arg_offset > 4) ? argv[arg_offset + 4] : NULL;
        printf("  good_count=%d  total_count=%d  addr=%s  mask=%s\n",
               good_count, total_count, addr ? addr : "(all)", mask ? mask : "(any)");
        ret = ops->burst(addr, mask, good_count, total_count);
        report(ops, ret, "burst");

    } else if (strcmp(cmd, "online") == 0) {
        const char *addr = (argc - arg_offset > 1) ? argv[arg_offset + 1] : NULL;
        const char *mask = (argc - arg_offset > 2) ? argv[arg_offset + 2] : NULL;
        printf("  addr=%s  mask=%s\n",
               addr ? addr : "(all)", mask ? mask : "(any)");
        ret = ops->online(addr, mask);
        report(ops, ret, "online");

    } else if (strcmp(cmd, "selectable_check") == 0) {
        int has_sel = 0;
        ret = ops->has_selectable_source(&has_sel);
        report(ops, ret, "has_selectable_source");
        if (ret == 0)
            printf("  Selectable source available: %s\n", has_sel ? "yes" : "no");

    } else if (strcmp(cmd, "source_count") == 0) {
        int count = 0;
        ret = ops->source_count(&count);
        report(ops, ret, "source_count");
        if (ret == 0)
            printf("  Tracked sources: %d\n", count);

    } else if (strcmp(cmd, "waitsync") == 0) {
        int max_tries    = (argc - arg_offset > 1) ? atoi(argv[arg_offset + 1]) : 30;
        int interval_sec = (argc - arg_offset > 2) ? atoi(argv[arg_offset + 2]) : 1;
        printf("  max_tries=%d  interval_sec=%d\n", max_tries, interval_sec);
        ret = ops->waitsync(max_tries, interval_sec);
        report(ops, ret, "waitsync");
        if (ret == 0)
            printf("  Clock is synchronised\n");
        else
            printf("  Clock is NOT synchronised after %d tries\n", max_tries);

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

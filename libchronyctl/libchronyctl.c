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
 * @file libchronyctl.c
 * @brief Implementation of chronyd control library using direct protocol
 *
 * Each call opens a fresh Unix-domain socket, binds it to a path that
 * includes the **thread** ID (gettid) rather than the process ID (getpid).
 * Using the thread ID guarantees that concurrent threads never try to bind
 * to or unlink the same local socket file.
 *
 * Note, however, that this file still uses shared global state
 * (`chronyctl_initialized`, `chrony_sequence`). The unique per-thread socket
 * path only avoids bind/unlink collisions; it does not make the overall API
 * safe for concurrent use. `chrony_sequence` is declared `_Atomic` to prevent
 * a data race on the sequence counter, but `chronyctl_initialized` is not
 * protected. Callers must ensure `chronyctl_init()` and `chronyctl_cleanup()`
 * are not called concurrently with any other API.
 */

#include "libchronyctl.h"
#include "chrony_protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <math.h>
#include <netdb.h>
#include <sys/stat.h>
#include <stddef.h>
#include <sys/syscall.h>
#include <stdatomic.h>

/* Return the calling thread's TID.  Each thread in a process has a unique TID
 * even though getpid() returns the same value for all threads.  Using the TID
 * for the local socket path prevents concurrent threads from racing on the
 * same bind/unlink operations. */
static pid_t get_tid(void)
{
    return (pid_t)syscall(SYS_gettid);
}

/* --- Internal State --- */

// Removed: static pthread_mutex_t chronyctl_mutex = PTHREAD_MUTEX_INITIALIZER;
static int chronyctl_initialized = 0;
static _Atomic uint32_t chrony_sequence = 0;

static const char *socket_paths[] = {
    "/run/chrony/chronyd.sock",
    "/var/run/chrony/chronyd.sock",
    NULL
};

/* --- Helper Functions --- */

#define FLOAT_EXP_BITS 7
#define FLOAT_COEF_BITS ((int)sizeof (int32_t) * 8 - FLOAT_EXP_BITS)

static double float_to_double(Float f) {
    int32_t exp, coef;
    uint32_t x = ntohl(f.f);

    exp = x >> FLOAT_COEF_BITS;
    if (exp >= 1 << (FLOAT_EXP_BITS - 1))
        exp -= 1 << FLOAT_EXP_BITS;
    exp -= FLOAT_COEF_BITS;

    coef = x % (1U << FLOAT_COEF_BITS);
    if (coef >= 1 << (FLOAT_COEF_BITS - 1))
        coef -= 1 << FLOAT_COEF_BITS;

    return coef * pow(2.0, exp);
}

static int parse_address(const char *address, IPAddr *ip) {
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET; 
    
    memset(ip, 0, sizeof(*ip));
    if (getaddrinfo(address, NULL, &hints, &res) == 0) {
        struct sockaddr_in *sin = (struct sockaddr_in *)res->ai_addr;
        ip->addr.in4 = ntohl(sin->sin_addr.s_addr);
        ip->family = IPADDR_INET4;
        freeaddrinfo(res);
        return 0;
    }
    return -1;
}

static void ip_host_to_network(const IPAddr *src, IPAddr *dest) {
  memset(dest, 0, sizeof(IPAddr));
    dest->family = htons(src->family);
    if (src->family == IPADDR_INET4) {
        dest->addr.in4 = htonl(src->addr.in4);
    }
}

static void cleanup_local_socket() {
    char local_path[128];
    snprintf(local_path, sizeof(local_path), "/run/chrony/chronyc.%d.sock", get_tid());
    unlink(local_path);
    snprintf(local_path, sizeof(local_path), "/var/run/chrony/chronyc.%d.sock", get_tid());
    unlink(local_path);
    snprintf(local_path, sizeof(local_path), "/var/run/chronyc.%d.sock", get_tid());
    unlink(local_path);
    snprintf(local_path, sizeof(local_path), "/tmp/chronyc.%d.sock", get_tid());
    unlink(local_path);
}

static int connect_to_chronyd(void) {
    int sockfd;
    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };

    sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_un local_address;
    int bound = 0;

    /*
     * The client socket must be in the same directory as chronyd.sock.
     * chronyd runs as _chrony and can only deliver reply datagrams to a
     * path it can write to.  /run/chrony/ is drwx------ owned by _chrony,
     * so any socket created there is accessible to chronyd regardless of
     * the socket's own mode.  After bind(), chmod the socket to 0666 so
     * that chronyd can write reply datagrams back to it (required on
     * Raspberry Pi OS for the kernel inode_permission check).
     */
    static const char *local_paths[] = {
        "/run/chrony/chronyc.%d.sock",
        "/var/run/chrony/chronyc.%d.sock",
        "/var/run/chronyc.%d.sock",
        NULL
    };

    memset(&local_address, 0, sizeof(local_address));
    local_address.sun_family = AF_UNIX;
    for (int i = 0; local_paths[i] != NULL; i++) {
        snprintf(local_address.sun_path, sizeof(local_address.sun_path), local_paths[i], get_tid());
        unlink(local_address.sun_path);
        if (bind(sockfd, (struct sockaddr *)&local_address, sizeof(local_address)) == 0) {
            /* 0666 lets chronyd write back replies; execute bits are not
             * meaningful for Unix sockets and 0777 is unnecessarily permissive. */
            if (chmod(local_address.sun_path, 0666) != 0) {
                unlink(local_address.sun_path);
                close(sockfd);
                return -1;
            }
            bound = 1;
            break;
        }
    }
    if (!bound) {
        close(sockfd);
        return -1;
    }

    for (int i = 0; socket_paths[i] != NULL; i++) {
        struct sockaddr_un addr;
        memset(&addr, 0, sizeof(addr));
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, socket_paths[i], sizeof(addr.sun_path) - 1);
        if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) == 0) {
            setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            return sockfd;
        }
    }

 /* All Unix socket paths failed — clean up and signal unreachable */
   close(sockfd);
   cleanup_local_socket();

    return -1;
}

static size_t get_request_length(uint16_t command) {
    switch (command) {
        /* REQ_Null has EOR at offset 0, so offsetof(CMD_Request, data.X.EOR) == offsetof(CMD_Request, data).
         * PROTO v6 requires every request to be >= its reply size (anti-amplification).
         * Use the reply struct to compute the correct minimum packet length. */
        case REQ_TRACKING:        return offsetof(CMD_Reply, data) + offsetof(RPY_Tracking, EOR);
        case REQ_MAKESTEP:        return offsetof(CMD_Reply, data) + offsetof(RPY_Null, EOR);
        case REQ_ONLINE:          return offsetof(CMD_Request, data.online.EOR);
        case REQ_BURST:           return offsetof(CMD_Request, data.burst.EOR);
        case REQ_ADD_SOURCE:      return offsetof(CMD_Request, data.add_source.EOR);
        case REQ_DEL_SOURCE:      return offsetof(CMD_Request, data.del_source.EOR);
        case REQ_MODIFY_MINPOLL:  return offsetof(CMD_Request, data.modify_minpoll.EOR);
        case REQ_MODIFY_MAXPOLL:  return offsetof(CMD_Request, data.modify_maxpoll.EOR);
        default:                  return sizeof(CMD_Request);
    }
}

static int send_request(int sockfd, uint16_t command, void *data, size_t data_size) {
    CMD_Request req;
    memset(&req, 0, sizeof(req));
    
    req.version  = PROTO_VERSION_NUMBER;
    req.packet_type = PKT_TYPE_CMD_REQUEST;
    req.command  = htons(command);
    req.sequence = htonl(chrony_sequence++);
    
    if (data && data_size > 0) {
        memcpy(&req.data, data, data_size);
    }
    
    size_t len = get_request_length(command);
    ssize_t sent = send(sockfd, &req, len, 0);
    return (sent == (ssize_t)len) ? 0 : -1;
}

static int receive_reply(int sockfd, uint16_t expected_reply, void *data, size_t data_size) {
    CMD_Reply reply;
    memset(&reply, 0, sizeof(reply));
    ssize_t received = recv(sockfd, &reply, sizeof(reply), 0);
    
    if (received < 0) {
        return CHRONYCTL_ERROR_EXEC;
    }
    
    if (received < (ssize_t)offsetof(CMD_Reply, data)) {
        return CHRONYCTL_ERROR_EXEC;
    }
    
    if (reply.version != PROTO_VERSION_NUMBER || reply.packet_type != PKT_TYPE_CMD_REPLY) {
        return CHRONYCTL_ERROR_EXEC;
    }
    
    uint16_t st = ntohs(reply.status);
    if (st != STT_SUCCESS) {
        if (st == STT_UNAUTH) return CHRONYCTL_ERROR_UNAUTH;
        if (st == STT_NOSUCHSOURCE) return CHRONYCTL_ERROR_EXEC;
        return CHRONYCTL_ERROR_EXEC;
    }

    uint16_t rpy = ntohs(reply.reply);
    if (expected_reply != RPY_NULL && rpy != expected_reply) {
        return CHRONYCTL_ERROR_PARSE;
    }
    
    if (data && data_size > 0) {
        size_t required = offsetof(CMD_Reply, data) + data_size;
        if (received < (ssize_t)required) {
            return CHRONYCTL_ERROR_EXEC;
        }
        memcpy(data, &reply.data, data_size);
    }
    
    return CHRONYCTL_SUCCESS;
}


/* --- Public API --- */

/*
 * Query chronyd's live source list to find the IP address it is actually
 * using for a given hostname.  chronyd tracks sources by the IP it resolved
 * at add-time; using getaddrinfo() directly may return a different IP if DNS
 * has rotated since then, causing DEL_SOURCE / MODIFY_POLL to fail with
 * NOSUCHSOURCE.
 *
 * sockfd   - already-connected socket to chronyd (reused for all sub-requests)
 * hostname - the configured hostname to look up (case-insensitive match)
 * out_net_ip - on success, filled with the IPAddr in network byte order,
 *              ready to embed directly in REQ_Del_Source / REQ_Modify_* payloads
 *
 * Returns 0 on success, -1 if the hostname is not found in chronyd's list.
 */
static int find_source_ip_by_name(int sockfd, const char *hostname, IPAddr *out_net_ip) {
    /* Step 1: get number of tracked sources */
    if (send_request(sockfd, REQ_N_SOURCES, NULL, 0) != 0)
        return -1;

    RPY_N_Sources n_rpy;
    if (receive_reply(sockfd, RPY_N_SOURCES, &n_rpy, sizeof(n_rpy)) != CHRONYCTL_SUCCESS)
        return -1;

    uint32_t count = ntohl(n_rpy.source_count);

    for (uint32_t i = 0; i < count; i++) {
        /* Step 2: get IPAddr for source at index i */
        REQ_Source_Data sd_req;
        memset(&sd_req, 0, sizeof(sd_req));
        sd_req.index = htonl(i);
        if (send_request(sockfd, REQ_SOURCE_DATA, &sd_req, sizeof(sd_req)) != 0)
            continue;

        RPY_Source_Data sd_rpy;
        if (receive_reply(sockfd, RPY_SOURCE_DATA, &sd_rpy, sizeof(sd_rpy)) != CHRONYCTL_SUCCESS)
            continue;

        /* Step 3: ask chronyd for the configured hostname of this IP */
        REQ_NTPSourceName sn_req;
        memset(&sn_req, 0, sizeof(sn_req));
        sn_req.ip_address = sd_rpy.ip_address;  /* already in network byte order from the reply */
        if (send_request(sockfd, REQ_NTP_SOURCE_NAME, &sn_req, sizeof(sn_req)) != 0)
            continue;

        RPY_NTPSourceName sn_rpy;
        if (receive_reply(sockfd, RPY_NTP_SOURCE_NAME, &sn_rpy, sizeof(sn_rpy)) != CHRONYCTL_SUCCESS)
            continue;

        sn_rpy.name[sizeof(sn_rpy.name) - 1] = '\0';
        if (strncasecmp((char *)sn_rpy.name, hostname, sizeof(sn_rpy.name)) == 0) {
            *out_net_ip = sd_rpy.ip_address;   /* network byte order, ready for wire */
            return 0;
        }
    }
    return -1;  /* hostname not found in chronyd's live source list */
}

/* --- Public API --- */

int chronyctl_init(void) {
    chronyctl_initialized = 1;
    return CHRONYCTL_SUCCESS;
}

int chronyctl_cleanup(void) {
    chronyctl_initialized = 0;
    return CHRONYCTL_SUCCESS;
}

int chronyctl_get_offset(double *offset_sec) {
    if (!offset_sec) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;
    
    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;
    
    int ret = send_request(sockfd, REQ_TRACKING, NULL, 0);
    if (ret == 0) {
        RPY_Tracking tracking;
        ret = receive_reply(sockfd, RPY_TRACKING, &tracking, sizeof(tracking));
        if (ret == CHRONYCTL_SUCCESS) {
            *offset_sec = float_to_double(tracking.last_clock_offset);
        }
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }
    
    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_makestep(void) {
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;
    
    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;
    
    int ret = send_request(sockfd, REQ_MAKESTEP, NULL, 0);
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }
    
    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_online(const IPAddr *addr, const IPAddr *mask) {
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    REQ_Online payload;
    memset(&payload, 0, sizeof(payload));

    if (addr)
        ip_host_to_network(addr, &payload.address);
    if (mask)
        ip_host_to_network(mask, &payload.mask);

    int ret = send_request(sockfd, REQ_ONLINE, &payload, sizeof(payload));
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }

    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_burst(const IPAddr *addr, const IPAddr *mask, int good_sample_count, int total_sample_count)  {
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    REQ_Burst payload;
    memset(&payload, 0, sizeof(payload));

    if (addr)
        ip_host_to_network(addr, &payload.address);
    if (mask)
        ip_host_to_network(mask, &payload.mask);

    payload.good_sample_count  = htonl(good_sample_count);
    payload.total_sample_count = htonl(total_sample_count);

    int ret = send_request(sockfd, REQ_BURST, &payload, sizeof(payload));
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }

    close(sockfd);
    cleanup_local_socket();
    return ret;
}


int chronyctl_add_server(const char *address, int minpoll, int maxpoll) {
    if (!address) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;
    
    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;
    
    REQ_NTP_Source payload;
    memset(&payload, 0, sizeof(payload));
    payload.type = htonl(REQ_ADDSRC_SERVER);
    strncpy((char *)payload.name, address, sizeof(payload.name) - 1);
    ((char *)payload.name)[sizeof(payload.name) - 1] = '\0';
    payload.port = htonl(123);
    payload.minpoll = htonl(minpoll);
    payload.maxpoll = htonl(maxpoll);
    payload.min_sample_count = htonl(6);
    payload.max_sample_count = htonl(12);
    payload.flags = htonl(REQ_ADDSRC_IBURST);
    
    int ret = send_request(sockfd, REQ_ADD_SOURCE, &payload, sizeof(payload));
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }
    
    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_delete_server(const char *address) {
    if (!address) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    /*
     * Prefer the IP that chronyd is actually tracking for this hostname over a
     * fresh DNS lookup.  If DNS has rotated since the server was added,
     * getaddrinfo() would return a different IP and DEL_SOURCE would fail with
     * NOSUCHSOURCE.  find_source_ip_by_name() uses REQ_NTP_SOURCE_NAME to
     * match the configured hostname against chronyd's live source list and
     * returns the exact IP chronyd resolved at add-time.
     */
    IPAddr net_ip;
    memset(&net_ip, 0, sizeof(net_ip));
    if (find_source_ip_by_name(sockfd, address, &net_ip) != 0) {
        /* Fall back to DNS resolution if hostname is not found in chronyd's list */
        IPAddr host_ip;
        if (parse_address(address, &host_ip) != 0) {
            close(sockfd); cleanup_local_socket();
            return CHRONYCTL_ERROR_INVALID;
        }
        ip_host_to_network(&host_ip, &net_ip);
    }

    REQ_Del_Source payload;
    memset(&payload, 0, sizeof(payload));
    payload.ip_address = net_ip;

    int ret = send_request(sockfd, REQ_DEL_SOURCE, &payload, sizeof(payload));
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }

    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_set_poll(const char *address, int minpoll, int maxpoll) {
    if (!address) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    /* Same rationale as chronyctl_delete_server: use the IP chronyd is actually
     * tracking for this hostname rather than re-resolving via DNS. */
    IPAddr net_ip;
    memset(&net_ip, 0, sizeof(net_ip));
    if (find_source_ip_by_name(sockfd, address, &net_ip) != 0) {
        /* Fall back to DNS resolution */
        IPAddr host_ip;
        if (parse_address(address, &host_ip) != 0) {
            close(sockfd); cleanup_local_socket();
            return CHRONYCTL_ERROR_INVALID;
        }
        ip_host_to_network(&host_ip, &net_ip);
    }

    REQ_Modify_Minpoll min_payload = { .address = net_ip, .min_poll_interval = htonl(minpoll) };
    int ret = send_request(sockfd, REQ_MODIFY_MINPOLL, &min_payload, sizeof(min_payload));
    if (ret == 0) {
        ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
    } else {
        ret = CHRONYCTL_ERROR_EXEC;
    }

    if (ret == CHRONYCTL_SUCCESS) {
        REQ_Modify_Maxpoll max_payload = { .address = net_ip, .max_poll_interval = htonl(maxpoll) };
        ret = send_request(sockfd, REQ_MODIFY_MAXPOLL, &max_payload, sizeof(max_payload));
        if (ret == 0) {
            ret = receive_reply(sockfd, RPY_NULL, NULL, 0);
        } else {
            ret = CHRONYCTL_ERROR_EXEC;
        }
    }

    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_has_selectable_source(int *has_selectable) {
    if (!has_selectable) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    /* Step 1: get the number of sources chronyd is tracking */
    int ret = send_request(sockfd, REQ_N_SOURCES, NULL, 0);
    if (ret != 0) {
        close(sockfd); cleanup_local_socket();
        return CHRONYCTL_ERROR_EXEC;
    }

    RPY_N_Sources n_rpy;
    ret = receive_reply(sockfd, RPY_N_SOURCES, &n_rpy, sizeof(n_rpy));
    if (ret != CHRONYCTL_SUCCESS) {
        close(sockfd); cleanup_local_socket();
        return ret;
    }

    uint32_t count = ntohl(n_rpy.source_count);
    *has_selectable = 0;

    /* Step 2: inspect each source — mirror of 'chronyc sources -v' */
    uint32_t query_failures = 0;
    for (uint32_t i = 0; i < count; i++) {
        REQ_Source_Data sd_req;
        memset(&sd_req, 0, sizeof(sd_req));
        sd_req.index = htonl(i);

        if (send_request(sockfd, REQ_SOURCE_DATA, &sd_req, sizeof(sd_req)) != 0) {
            query_failures++;
            continue;
        }

        RPY_Source_Data sd_rpy;
        if (receive_reply(sockfd, RPY_SOURCE_DATA, &sd_rpy, sizeof(sd_rpy)) != CHRONYCTL_SUCCESS) {
            query_failures++;
            continue;
        }

        uint16_t state = ntohs(sd_rpy.state);
        /* RPY_SD_ST_SELECTED (0) == '*' and RPY_SD_ST_SELECTABLE (5) == '+'
           in chronyc sources -v output */
        if (state == RPY_SD_ST_SELECTED || state == RPY_SD_ST_SELECTABLE) {
            *has_selectable = 1;
            break;
        }
    }

    /* If chronyd told us there are sources but every single per-source query
     * failed, the socket is broken — report an error rather than silently
     * returning "no selectable source found". */
    if (count > 0 && query_failures == count) {
        close(sockfd);
        cleanup_local_socket();
        return CHRONYCTL_ERROR_EXEC;
    }

    close(sockfd);
    cleanup_local_socket();
    return CHRONYCTL_SUCCESS;
}

int chronyctl_get_source_count(int *count) {
    if (!count) return CHRONYCTL_ERROR_INVALID;
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;

    int sockfd = connect_to_chronyd();
    if (sockfd < 0) return CHRONYCTL_ERROR_NO_DATA;

    int ret = send_request(sockfd, REQ_N_SOURCES, NULL, 0);
    if (ret != 0) {
        close(sockfd); cleanup_local_socket();
        return CHRONYCTL_ERROR_EXEC;
    }

    RPY_N_Sources n_rpy;
    ret = receive_reply(sockfd, RPY_N_SOURCES, &n_rpy, sizeof(n_rpy));
    if (ret == CHRONYCTL_SUCCESS)
        *count = (int)ntohl(n_rpy.source_count);

    close(sockfd);
    cleanup_local_socket();
    return ret;
}

int chronyctl_waitsync(int max_tries, int interval_sec) {
    if (!chronyctl_initialized) return CHRONYCTL_ERROR_NOT_INIT;
    if (max_tries <= 0 || interval_sec <= 0) return CHRONYCTL_ERROR_INVALID;

    for (int i = 0; i < max_tries; i++) {
        int sockfd = connect_to_chronyd();
        if (sockfd < 0) {
            if (i < max_tries - 1)
                sleep(interval_sec);
            continue;
        }

        int ret = send_request(sockfd, REQ_TRACKING, NULL, 0);
        if (ret == 0) {
            RPY_Tracking tracking;
            ret = receive_reply(sockfd, RPY_TRACKING, &tracking, sizeof(tracking));
            if (ret == CHRONYCTL_SUCCESS) {
                uint16_t leap   = ntohs(tracking.leap_indicator);
                uint32_t reference_id = ntohl(tracking.reference_id);
                /* Mirrors chronyc waitsync: synchronized when leap_indicator is not
                 * LEAP_Unsynchronised (3) and a reference source is active. */
                if (leap != 3 && reference_id != 0) {
                    close(sockfd);
                    cleanup_local_socket();
                    return CHRONYCTL_SUCCESS;
                }
            }
        }

        close(sockfd);
        cleanup_local_socket();
        if (i < max_tries - 1)
            sleep(interval_sec);
    }
    return CHRONYCTL_ERROR_NO_DATA;
}

const char* chronyctl_strerror(int err) {
    switch (err) {
        case CHRONYCTL_SUCCESS: return "Success";
        case CHRONYCTL_ERROR_INIT: return "Initialization error";
        case CHRONYCTL_ERROR_NOT_INIT: return "Library not initialized";
        case CHRONYCTL_ERROR_EXEC: return "Communication error with chronyd";
        case CHRONYCTL_ERROR_PARSE: return "Parse error";
        case CHRONYCTL_ERROR_INVALID: return "Invalid parameter";
        case CHRONYCTL_ERROR_MUTEX: return "Mutex error";
        case CHRONYCTL_ERROR_NO_DATA: return "Cannot reach chronyd";
        case CHRONYCTL_ERROR_UNAUTH: return "Permission denied (not root?)";
        default: return "Unknown error";
    }
}

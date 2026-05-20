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
 * @file libchronyctl.h
 * @brief shared library for chronyd interaction
 *
 * Thread safety: each chronyctl_* call (other than init/cleanup) opens and
 * closes its own Unix socket using the calling thread's TID, so concurrent
 * calls to the data-plane functions are safe. The initialization flag
 * (chronyctl_initialized) is not protected; chronyctl_init() and
 * chronyctl_cleanup() must not be called concurrently with any other API.
 * The internal sequence counter is _Atomic and does not require external
 * serialization.
 */

#ifndef LIBCHRONYCTL_H
#define LIBCHRONYCTL_H

#include <stdint.h>
#include <stddef.h>
#include "chrony_address.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Library error codes
 */
typedef enum {
    CHRONYCTL_SUCCESS = 0,
    CHRONYCTL_ERROR_INIT = -1,
    CHRONYCTL_ERROR_NOT_INIT = -2,
    CHRONYCTL_ERROR_EXEC = -3,
    CHRONYCTL_ERROR_PARSE = -4,
    CHRONYCTL_ERROR_INVALID = -5,
    CHRONYCTL_ERROR_MUTEX = -6,
    CHRONYCTL_ERROR_NO_DATA = -7,
    CHRONYCTL_ERROR_UNAUTH = -8
} chronyctl_error;

/**
 * @brief Initialize the library
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_init(void);

/**
 * @brief Cleanup library resources
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_cleanup(void);

/**
 * @brief Get current time offset from chronyd
 * @param offset_sec Pointer to double to store offset in seconds
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_get_offset(double *offset_sec);

/**
 * @brief Force chronyd to step the system clock
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_makestep(void);

/**
 * @brief Add a new NTP server
 * @param address Hostname or IP of the server
 * @param minpoll Min poll interval (log2 seconds, e.g., 6 for 64s)
 * @param maxpoll Max poll interval (log2 seconds, e.g., 10 for 1024s)
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_add_server(const char *address, int minpoll, int maxpoll);

/**
 * @brief Request a burst of measurements from matching NTP sources
 *
 * Equivalent to `chronyc burst [addr/mask] good/total`. When both @p addr
 * and @p mask are NULL the burst request applies to all sources
 * (IPADDR_UNSPEC wildcard).
 *
 * @param addr            IP address to match, host byte-order
 *                        (NULL = all sources)
 * @param mask            IP mask to apply, host byte-order
 *                        (NULL = all sources)
 * @param n_good_samples  Minimum number of good samples to acquire
 * @param n_total_samples Total number of samples to attempt
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_burst(const IPAddr *addr, const IPAddr *mask, int good_sample_count, int total_sample_count);

/**
 * @brief Set NTP sources matching mask/address to online mode
 *
 * Equivalent to `chronyc online [addr/mask]`.  When both @p addr and @p mask
 * are NULL all sources are brought online (IPADDR_UNSPEC wildcard).
 *
 * @param addr  IP address to match, host byte-order (NULL = all sources)
 * @param mask  IP mask to apply,    host byte-order (NULL = all sources)
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_online(const IPAddr *addr, const IPAddr *mask);

/**
 * @brief Delete an NTP server
 * @param address Server address or hostname to delete
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_delete_server(const char *address);

/**
 * @brief Update poll intervals for a server
 * @param address Server address or hostname
 * @param minpoll New min poll interval
 * @param maxpoll New max poll interval
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_set_poll(const char *address, int minpoll, int maxpoll);

/**
 * @brief Check whether at least one selectable NTP source is available
 *
 * Iterates through all sources tracked by chronyd (equivalent to the output
 * of `chronyc sources -v`) and tests whether any source is in the
 * "selected" (*) or "selectable" (+) state.
 *
 * @param has_selectable  Set to 1 if a selectable/selected source exists,
 *                        0 otherwise.  Must not be NULL.
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_has_selectable_source(int *has_selectable);

/**
 * @brief Get the number of NTP sources chronyd is currently tracking (any state)
 *
 * Equivalent to counting lines in `chronyc sources`.  Unlike
 * chronyctl_has_selectable_source() this function counts sources in all
 * states ('^?', '^*', '^+', '^-', '^x', '^~'), making it useful to
 * distinguish "chronyd has no configured sources" (count == 0) from
 * "chronyd has sources but none is selected yet" (count > 0).
 *
 * Returns CHRONYCTL_ERROR_NO_DATA when chronyd's socket is unreachable,
 * which typically means chronyd is not yet running.
 *
 * @param count  Set to the number of tracked sources on success.  Must not
 *               be NULL.
 * @return CHRONYCTL_SUCCESS on success, error code otherwise
 */
int chronyctl_get_source_count(int *count);

/**
 * @brief Wait until chronyd reports that it is synchronized
 *
 * Polls chronyd tracking status every @p interval_sec seconds, up to
 * @p max_tries attempts. Synchronization is determined from chronyd's
 * tracking reply (for example, the leap indicator and reference ID),
 * rather than by checking whether any source is merely selectable.
 *
 * This corresponds to waiting for chronyd to report an active reference,
 * similar to `chronyc waitsync max_tries 0 0 interval_sec`.
 *
 * @param max_tries    Maximum number of poll iterations
 * @param interval_sec Sleep duration in seconds between polls
 * @return CHRONYCTL_SUCCESS when chronyd reports synchronization,
 *         CHRONYCTL_ERROR_NO_DATA if timed out or chronyd unreachable
 */
int chronyctl_waitsync(int max_tries, int interval_sec);

/**
 * @brief Get human-readable error message
 * @param err Error code
 * @return Const string description
 */
const char* chronyctl_strerror(int err);

#ifdef __cplusplus
}
#endif

#endif /* LIBCHRONYCTL_H */

## Purpose

Defines the functional contract of `libchronyctl` — the lightweight C library that controls the `chronyd` NTP daemon over its native Unix-domain-socket protocol. Covers the initialization lifecycle, the per-call socket/request/reply pattern, query and control operations, DNS-rotation-safe source management, and error handling.

---

## Diagram: Request/Reply Socket Lifecycle

Shows the exact sequence every `chronyctl_*` data-plane call follows, from socket creation through cleanup.

→ [View diagram](../../diagrams/02-libchronyctl-request-flow.md)

---

## Requirement: Library Must Be Initialized Before Use

Every `chronyctl_*` function other than `chronyctl_init()` and `chronyctl_strerror()` MUST reject calls made before `chronyctl_init()` has run.

### Scenario: Call before init

- **WHEN** any data-plane `chronyctl_*` function is called
- **AND** `chronyctl_init()` has not yet been called
- **THEN** the function returns `CHRONYCTL_ERROR_NOT_INIT` without touching the network

### Scenario: Call after cleanup

- **WHEN** `chronyctl_cleanup()` has been called
- **AND** a data-plane function is called afterward without a new `chronyctl_init()`
- **THEN** the function returns `CHRONYCTL_ERROR_NOT_INIT`

---

## Requirement: Per-Call Socket Lifecycle

Every `chronyctl_*` data-plane call MUST create a fresh, per-thread local Unix datagram socket, exchange exactly one request/reply packet with `chronyd`, and tear the socket down before returning. No connection, socket, or heap state persists between calls.

### Scenario: Successful call

- **WHEN** a data-plane function is invoked
- **THEN** a local socket is created (`AF_UNIX`, `SOCK_DGRAM`) and bound to `chronyc.<tid>.sock` using the calling thread's TID (not PID)
- **AND** the local socket is `chmod`'d to `0666` after `bind()`
- **AND** the socket is `connect()`'d to the first reachable of `/run/chrony/chronyd.sock` or `/var/run/chrony/chronyd.sock`
- **AND** a 2-second receive timeout (`SO_RCVTIMEO`) is set
- **AND** exactly one `CMD_Request` is sent and one `CMD_Reply` is received
- **AND** the socket is `close()`'d and the local socket path is `unlink()`'d before the function returns

### Scenario: chronyd socket unreachable

- **WHEN** neither `/run/chrony/chronyd.sock` nor `/var/run/chrony/chronyd.sock` can be connected to
- **THEN** the function returns `CHRONYCTL_ERROR_NO_DATA`
- **AND** the local socket (if created) is still unlinked

### Scenario: Receive times out

- **WHEN** `chronyd` does not reply within 2 seconds
- **THEN** the function returns `CHRONYCTL_ERROR_EXEC`
- **AND** the socket is still closed and unlinked (no leaked descriptors or stale socket files)

### Scenario: Concurrent calls from different threads do not collide

- **WHEN** two threads call `chronyctl_*` functions at the same time
- **THEN** each thread binds its own `chronyc.<tid>.sock` using its own TID (via `gettid()`), so neither `bind()` nor `unlink()` races with the other
- **AND** the per-request sequence counter is `_Atomic`, so concurrent sequence numbers never collide

---

## Requirement: Offset Queries Distinguish Last-Sample vs. Continuous Estimate

The library MUST expose two distinct offset readings so callers can choose between "offset at last NTP sample" and "chronyd's continuously updated current estimate."

### Scenario: chronyctl_get_offset returns the last measured sample offset

- **WHEN** `chronyctl_get_offset(&offset_sec)` is called
- **THEN** the value returned is `RPY_Tracking.last_clock_offset` (equivalent to the "Last offset" line of `chronyc tracking`)

### Scenario: chronyctl_get_system_time_offset returns the running estimate

- **WHEN** `chronyctl_get_system_time_offset(&system_offset_sec)` is called
- **THEN** the value returned is `RPY_Tracking.clock_correction` (equivalent to the "System time" line of `chronyc tracking`), continuously updated between NTP exchanges via chrony's frequency model

### Scenario: NULL output pointer rejected

- **WHEN** either offset function is called with a NULL output pointer
- **THEN** the function returns `CHRONYCTL_ERROR_INVALID` without touching the network

---

## Requirement: add_server Applies Fixed Protocol Defaults

`chronyctl_add_server()` MUST always configure a new source as an `iburst` server on port 123 with fixed min/max sample counts, taking only the hostname/address and poll-interval exponents from the caller.

### Scenario: Add server with caller-supplied poll range

- **WHEN** `chronyctl_add_server(address, minpoll, maxpoll)` is called
- **THEN** the `REQ_ADD_SOURCE` payload is sent with `port = 123`, `flags = REQ_ADDSRC_IBURST`, `min_sample_count = 6`, `max_sample_count = 12`, and `minpoll`/`maxpoll` set to the caller-supplied values
- **AND** on success the function returns `CHRONYCTL_SUCCESS`

### Scenario: NULL address rejected

- **WHEN** `chronyctl_add_server(NULL, ...)` is called
- **THEN** the function returns `CHRONYCTL_ERROR_INVALID` without contacting `chronyd`

---

## Requirement: Server Management Is DNS-Rotation-Safe

`chronyctl_delete_server()` and `chronyctl_set_poll()` MUST resolve the target source through chronyd's live source list rather than a fresh DNS lookup, so a hostname whose DNS record has rotated since it was added can still be matched.

### Scenario: Delete resolves IP via chronyd's source list

- **WHEN** `chronyctl_delete_server(hostname)` is called
- **THEN** `find_source_ip_by_name()` walks `REQ_N_SOURCES` → `REQ_SOURCE_DATA` → `REQ_NTP_SOURCE_NAME` to find the IP chronyd currently associates with that hostname (case-insensitive match)
- **AND** `REQ_DEL_SOURCE` is sent using that resolved IP, not a freshly resolved one

### Scenario: DNS has rotated since the server was added

- **WHEN** the hostname's DNS record now points to a different IP than when it was added
- **AND** `chronyctl_delete_server()` / `chronyctl_set_poll()` is called
- **THEN** the original (still-tracked-by-chronyd) IP is used, and the operation succeeds instead of failing with `STT_NOSUCHSOURCE`

### Scenario: Hostname not tracked by chronyd

- **WHEN** the hostname is not found in chronyd's current source list
- **THEN** the function returns `CHRONYCTL_ERROR_EXEC`

---

## Requirement: Selectable-Source and Source-Count Queries Are Distinct

The library MUST let callers distinguish "no sources configured at all" from "sources configured but none selected yet."

### Scenario: has_selectable_source checks for `*`/`+` state

- **WHEN** `chronyctl_has_selectable_source(&has_selectable)` is called
- **THEN** `*has_selectable` is set to `1` if any tracked source is in the selected (`*`) or selectable (`+`) state, else `0`

### Scenario: get_source_count counts all tracked sources regardless of state

- **WHEN** `chronyctl_get_source_count(&count)` is called
- **THEN** `*count` is set to the number of sources chronyd tracks in any state (`^?`, `^*`, `^+`, `^-`, `^x`, `^~`)
- **AND** `count == 0` means no sources are configured, while `count > 0` with no selectable source means sources exist but none has been selected yet

---

## Requirement: waitsync Polls Until chronyd Reports Synchronization

`chronyctl_waitsync()` MUST poll chronyd's tracking reply at a caller-specified interval, up to a caller-specified number of attempts, and report success only once chronyd itself reports an active, synchronized reference.

### Scenario: Synchronization achieved within the attempt budget

- **WHEN** `chronyctl_waitsync(max_tries, interval_sec)` is called
- **AND** chronyd's tracking reply indicates synchronization before `max_tries` polls elapse
- **THEN** the function returns `CHRONYCTL_SUCCESS`

### Scenario: Attempt budget exhausted

- **WHEN** `max_tries` polls complete without chronyd reporting synchronization
- **THEN** the function returns `CHRONYCTL_ERROR_NO_DATA`

---

## Requirement: Error Codes Are Stable and Human-Readable

Every `chronyctl_*` function (except `chronyctl_cleanup()` and `chronyctl_strerror()`) MUST return one of the fixed `chronyctl_error` codes, and `chronyctl_strerror()` MUST provide a non-NULL human-readable string for every defined code.

### Scenario: strerror covers all defined codes

- **WHEN** `chronyctl_strerror(err)` is called with any value from the `chronyctl_error` enum (`CHRONYCTL_SUCCESS` through `CHRONYCTL_ERROR_UNAUTH`)
- **THEN** a non-NULL, immutable static string describing that code is returned

### Scenario: strerror is concurrency-safe

- **WHEN** `chronyctl_strerror()` is called from multiple threads simultaneously
- **THEN** no external synchronization is required, since it only returns pointers to immutable string literals

### Scenario: chronyd rejects the request as unauthorized

- **WHEN** `chronyd` replies with `STT_UNAUTH`
- **THEN** the calling `chronyctl_*` function returns `CHRONYCTL_ERROR_UNAUTH`

---

## Requirement: No Persistent State Beyond a Minimal Static Footprint

The library MUST NOT retain heap allocations, background threads, or open sockets between API calls.

### Scenario: Static footprint bound

- **WHEN** the library is linked into any process
- **THEN** its only persistent state is the `chronyctl_initialized` flag and the atomic `chrony_sequence` counter (8 bytes total)
- **AND** no background thread is created by `chronyctl_init()` or any data-plane call

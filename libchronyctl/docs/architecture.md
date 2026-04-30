# libchronyctl — Architecture

## Overview

`libchronyctl` is a lightweight, embedded C library that provides programmatic control of the `chronyd` NTP daemon on RDK-based devices. It communicates directly over chronyd's native Unix domain socket protocol, replacing fragile `chronyc` CLI subprocess invocations with a typed, in-process API. While designed primarily for `chronyd`, the repository also ships a backend-agnostic test binary (`test_timectl`) whose `ntp_ops_t` function-pointer table can accommodate other NTP clients with no changes to the test harness.

---

## Design

The library is a thin synchronous shim between the calling application and the `chronyd` daemon. Every public API call follows the same three-step pattern: open a per-process Unix socket, exchange exactly one request/reply packet with chronyd, then close and unlink the socket before returning. There are no background threads, no connection pool, and no persistent heap allocations.

### Component Diagram

```mermaid
graph TB
    A[Client Application] -->|chronyctl_*| B[libchronyctl public API]
    B --> C[connect_to_chronyd]
    C -->|AF_UNIX SOCK_DGRAM| D["chronyd.sock"]
    D -->|Unix socket| E[chronyd daemon]
    B --> F["send_request - CMD_Request + hton"]
    F --> D
    B --> G["receive_reply - CMD_Reply + ntoh"]
    G --> D
    B --> H["cleanup_local_socket - unlink PID.sock"]
    I[test_timectl CLI] -->|ntp_ops_t vtable| B
```

### Dependency Map

```
libchronyctl.c
  ├── libchronyctl.h      (public API, error codes)
  ├── candm.h             (chronyd wire protocol: CMD_Request, CMD_Reply, REQ_*, RPY_*, STT_*)
  └── addressing.h        (IPAddr type, IPADDR_INET4/INET6/UNSPEC constants)

test_timectl.c
  ├── libchronyctl.h      (chrony backend)
  └── addressing.h        (IPAddr for burst/online wrappers)
```

---

## Key Components

### `libchronyctl.h` — Public API and Error Codes

Defines the `chronyctl_error` enum and all `chronyctl_*` function declarations.

```c
typedef enum {
    CHRONYCTL_SUCCESS       =  0,
    CHRONYCTL_ERROR_INIT    = -1,   /* init failed (reserved) */
    CHRONYCTL_ERROR_NOT_INIT= -2,   /* chronyctl_init() not yet called */
    CHRONYCTL_ERROR_EXEC    = -3,   /* protocol exchange failed */
    CHRONYCTL_ERROR_PARSE   = -4,   /* reply parse error (reserved) */
    CHRONYCTL_ERROR_INVALID = -5,   /* invalid argument (e.g. NULL pointer) */
    CHRONYCTL_ERROR_MUTEX   = -6,   /* mutex error (reserved) */
    CHRONYCTL_ERROR_NO_DATA = -7,   /* chronyd socket unreachable */
    CHRONYCTL_ERROR_UNAUTH  = -8    /* chronyd rejected: STT_UNAUTH */
} chronyctl_error;
```

### `candm.h` — Wire Protocol Types

Verbatim from the chrony source tree. Defines `CMD_Request`, `CMD_Reply`, all `REQ_*` command codes, `RPY_*` reply codes, `STT_*` status codes, and protocol data structures (`RPY_Tracking`, `REQ_NTP_Source`, `REQ_Del_Source`, `REQ_Burst`, etc.).

### `addressing.h` — Network Address Type

Defines `IPAddr` — a union holding an IPv4 address, IPv6 address, or numeric ID — and the `IPADDR_*` family constants. All addresses in `libchronyctl` are stored in **host byte order** in `IPAddr`; conversion to network byte order happens inside `ip_host_to_network()` before embedding in wire payloads.

### `libchronyctl.c` — Implementation

Contains all internal helpers and the public API implementations.

| Symbol | Type | Purpose |
|--------|------|---------|
| `chronyctl_initialized` | `static int` | Guard: set by `chronyctl_init()`, checked by every API |
| `chrony_sequence` | `static uint32_t` | Per-request sequence number, incremented on each `send_request()` |
| `socket_paths[]` | `static const char*[]` | Probe list: `/var/run/chrony/chronyd.sock`, `/run/chrony/chronyd.sock` |
| `connect_to_chronyd()` | internal | Bind local socket, probe paths, set 2s recv timeout |
| `send_request()` | internal | Zero-fill `CMD_Request`, set version/type/command/sequence, `send()` |
| `receive_reply()` | internal | `recv()`, validate version/type/status, copy reply data |
| `cleanup_local_socket()` | internal | `unlink` both candidate local socket paths |
| `parse_address()` | internal | `getaddrinfo()` → `IPAddr` (IPv4 only) |
| `ip_host_to_network()` | internal | Convert `IPAddr` fields to network byte order for wire payloads |
| `float_to_double()` | internal | Decode chrony's 7-bit-exponent `Float` type to IEEE `double` |
| `find_source_ip_by_name()` | internal | Walk `REQ_N_SOURCES` / `REQ_SOURCE_DATA` / `REQ_NTP_SOURCE_NAME` to resolve hostname → tracked IP (DNS-rotation-safe) |

### `test_timectl.c` — Backend-Agnostic CLI

Provides a command-line interface for all library operations and demonstrates the `ntp_ops_t` abstraction for swapping NTP client backends at compile time.

```c
typedef struct {
    const char *name;
    int  (*init)(void);
    int  (*cleanup)(void);
    int  (*get_offset)(double *offset_sec);
    int  (*makestep)(void);
    int  (*add_server)(const char *host, int minpoll, int maxpoll);
    int  (*delete_server)(const char *host);
    int  (*set_poll)(const char *host, int minpoll, int maxpoll);
    int  (*burst)(const char *addr, const char *mask, int n_good, int n_total);
    int  (*online)(const char *addr, const char *mask);
    int  (*has_selectable_source)(int *has_selectable);
    const char *(*strerror)(int err);
} ntp_ops_t;
```

Adding a new backend requires only: adding a `#include`, filling in one `ntp_ops_t` row, and recompiling.

---

## Socket Lifecycle

Every `chronyctl_*` API call (except `init` and `cleanup`) executes this exact lifecycle. No socket state persists between calls.

```mermaid
sequenceDiagram
    participant App
    participant libchronyctl
    participant Kernel
    participant chronyd

    App->>libchronyctl: chronyctl_*(args)
    libchronyctl->>libchronyctl: check chronyctl_initialized
    libchronyctl->>Kernel: socket(AF_UNIX, SOCK_DGRAM, 0)
    libchronyctl->>Kernel: bind(/var/run/chronyc.PID.sock)
    libchronyctl->>Kernel: chmod 0666
    libchronyctl->>Kernel: connect(/var/run/chrony/chronyd.sock)
    libchronyctl->>Kernel: setsockopt SO_RCVTIMEO = 2s
    libchronyctl->>chronyd: send(CMD_Request, REQ_*)
    chronyd-->>libchronyctl: recv(CMD_Reply, RPY_*)
    libchronyctl->>libchronyctl: validate version / status
    libchronyctl->>Kernel: close(sockfd)
    libchronyctl->>Kernel: unlink(/var/run/chronyc.PID.sock)
    libchronyctl-->>App: CHRONYCTL_SUCCESS or error code
```

**Key properties:**
- Socket is `AF_UNIX SOCK_DGRAM` — connectionless; one `send()` + one `recv()` per API call
- Local path falls back from `/var/run/chronyc.<pid>.sock` to `/tmp/chronyc.<pid>.sock` if the primary location is not writable
- Receive timeout is 2 seconds; exceeding it returns `CHRONYCTL_ERROR_EXEC`
- Local socket is always unlinked, even on error paths

---

## Performance Considerations

| Metric | Value | Notes |
|--------|-------|-------|
| Per-call stack usage | ~1 KB peak | Largest: `chronyctl_add_server` with 520-byte payload |
| Persistent heap | 0 bytes | Stack-only model |
| Static footprint | 8 bytes | `initialized` flag + `sequence` counter |
| Socket connect latency | <1 ms on-device | Unix domain socket; loopback |
| Receive timeout | 2 seconds | Hard-coded in `connect_to_chronyd()` |
| CPU when idle | 0 | No background threads or timers |
| `find_source_ip_by_name` overhead | O(n) source queries | Delete/set_poll only; n = number of sources tracked by chronyd |

---

## Platform Notes

### Linux (general)

- chronyd socket probed in order: `/var/run/chrony/chronyd.sock`, `/run/chrony/chronyd.sock`
- Local reply socket: `/var/run/chronyc.<pid>.sock` (falls back to `/tmp/chronyc.<pid>.sock`)
- `chmod 0666` applied to local socket; requires chronyd configured to accept unauthenticated local commands
- Receive timeout: 2 seconds (`SO_RCVTIMEO`)

### Constraints

| Constraint | Value |
|-----------|-------|
| chronyd protocol version | chrony 3.x+ (`PROTO_VERSION_NUMBER` from `candm.h`) |
| IP family | IPv4 only (`AF_INET`; `parse_address()` uses `hints.ai_family = AF_INET`) |
| Min stack per call | ~1 KB |
| Min RAM | No library-imposed minimum beyond kernel defaults |
| Build dependencies | C99 compiler, `libm` (`-lm` for `pow()`), POSIX sockets |

---

## See Also

- [libchronyctl.h](../libchronyctl.h) — Public API header and error code definitions
- [libchronyctl.c](../libchronyctl.c) — Full implementation with inline documentation
- [test_timectl.c](../test_timectl.c) — Backend-agnostic CLI and `ntp_ops_t` usage examples
- [api.md](api.md) — API reference, usage examples, error handling, and testing
- [README.md](../README.md) — Component quick-start
- [CONTRIBUTING.md](../../CONTRIBUTING.md) — Contribution guidelines
- [CHANGELOG.md](../../CHANGELOG.md) — Version history

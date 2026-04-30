# libchronyctl — API Reference

## Overview

All functions require `chronyctl_init()` to have been called first (except `chronyctl_init()` itself and `chronyctl_strerror()`).

### Lifecycle

| Function | Signature | Description |
|----------|-----------|-------------|
| `chronyctl_init` | `int chronyctl_init(void)` | Set initialized flag. Call once before any other API. |
| `chronyctl_cleanup` | `int chronyctl_cleanup(void)` | Clear initialized flag. Call once when done. |

### Query

| Function | Signature | Description |
|----------|-----------|-------------|
| `chronyctl_get_offset` | `int chronyctl_get_offset(double *offset_sec)` | Get current NTP clock offset in seconds from `RPY_Tracking.last_clock_offset`. |
| `chronyctl_has_selectable_source` | `int chronyctl_has_selectable_source(int *has_selectable)` | Set `*has_selectable = 1` if any source is in selected (`*`) or selectable (`+`) state. |

### Control

| Function | Signature | Description |
|----------|-----------|-------------|
| `chronyctl_makestep` | `int chronyctl_makestep(void)` | Force an immediate clock step (`REQ_MAKESTEP`). |
| `chronyctl_online` | `int chronyctl_online(const IPAddr *addr, const IPAddr *mask)` | Bring matching NTP sources online. Pass both `NULL` to bring all sources online. |
| `chronyctl_burst` | `int chronyctl_burst(const IPAddr *addr, const IPAddr *mask, int n_good, int n_total)` | Trigger a burst of NTP polls. `n_good` of `n_total` samples must be good. |

### Source Management

| Function | Signature | Description |
|----------|-----------|-------------|
| `chronyctl_add_server` | `int chronyctl_add_server(const char *address, int minpoll, int maxpoll)` | Add an NTP server. Configured with `IBURST`, port 123, min 6 / max 12 samples. |
| `chronyctl_delete_server` | `int chronyctl_delete_server(const char *address)` | Remove a server. Uses `find_source_ip_by_name()` to avoid DNS-rotation mismatches. |
| `chronyctl_set_poll` | `int chronyctl_set_poll(const char *address, int minpoll, int maxpoll)` | Update min/max poll intervals for a server. Also DNS-rotation-safe. |

### Diagnostics

| Function | Signature | Description |
|----------|-----------|-------------|
| `chronyctl_strerror` | `const char* chronyctl_strerror(int err)` | Return a const string for a `chronyctl_error` code. Thread-safe (static strings). |

---

## Usage Examples

### Example: Initialize, Query Offset, Cleanup

```c
#include "libchronyctl.h"
#include <stdio.h>

int main(void) {
    double offset = 0.0;
    int ret;

    ret = chronyctl_init();
    if (ret != CHRONYCTL_SUCCESS) {
        fprintf(stderr, "init: %s\n", chronyctl_strerror(ret));
        return 1;
    }

    ret = chronyctl_get_offset(&offset);
    if (ret != CHRONYCTL_SUCCESS) {
        fprintf(stderr, "get_offset: %s\n", chronyctl_strerror(ret));
        chronyctl_cleanup();
        return 1;
    }

    printf("NTP offset: %.9f s\n", offset);

    chronyctl_cleanup();
    return 0;
}
```

**Expected output:**
```
NTP offset: 0.000042371 s
```

### Example: Add Server and Force Step

```c
#include "libchronyctl.h"
#include <stdio.h>

int main(void) {
    int ret;

    chronyctl_init();

    ret = chronyctl_add_server("pool.ntp.org", 6, 10);
    if (ret != CHRONYCTL_SUCCESS) {
        fprintf(stderr, "add_server: %s\n", chronyctl_strerror(ret));
        goto done;
    }

    ret = chronyctl_makestep();
    if (ret != CHRONYCTL_SUCCESS)
        fprintf(stderr, "makestep: %s\n", chronyctl_strerror(ret));

done:
    chronyctl_cleanup();
    return (ret == CHRONYCTL_SUCCESS) ? 0 : 1;
}
```

### Example: Check Source Availability Before Acting

```c
int has_source = 0;
if (chronyctl_has_selectable_source(&has_source) == CHRONYCTL_SUCCESS && has_source) {
    chronyctl_get_offset(&offset);
} else {
    /* No usable source — skip offset read */
}
```

### CLI via test_timectl

```bash
# Query offset
./test_timectl offset_check

# Add an NTP server (minpoll=6, maxpoll=10)
./test_timectl server pool.ntp.org 6 10

# Force clock step
./test_timectl makestep

# Delete a server
./test_timectl delete_server pool.ntp.org

# Update poll intervals
./test_timectl set_poll pool.ntp.org 4 8

# Trigger burst (4 good out of 8 total, all sources)
./test_timectl burst 4 8
```

---

## Error Handling

### Error Code Reference

| Code | Value | Meaning | Common Cause |
|------|-------|---------|--------------|
| `CHRONYCTL_SUCCESS` | 0 | Operation succeeded | — |
| `CHRONYCTL_ERROR_INIT` | -1 | Init failed (reserved) | Future use |
| `CHRONYCTL_ERROR_NOT_INIT` | -2 | `chronyctl_init()` not called | Call `chronyctl_init()` first |
| `CHRONYCTL_ERROR_EXEC` | -3 | Protocol exchange failed | recv timeout, malformed reply, `STT_NOSUCHSOURCE` |
| `CHRONYCTL_ERROR_PARSE` | -4 | Reply parse error (reserved) | Future use |
| `CHRONYCTL_ERROR_INVALID` | -5 | Invalid argument | NULL pointer passed to output parameter |
| `CHRONYCTL_ERROR_MUTEX` | -6 | Mutex error (reserved) | Future use |
| `CHRONYCTL_ERROR_NO_DATA` | -7 | chronyd socket unreachable | chronyd not running; wrong socket path |
| `CHRONYCTL_ERROR_UNAUTH` | -8 | Unauthorized | `STT_UNAUTH` from chronyd; caller lacks permission |

### Error Handling Pattern

Every `chronyctl_*` call should be checked. On error, `chronyctl_strerror()` provides a human-readable description:

```c
int ret = chronyctl_add_server("time.cloudflare.com", 6, 10);
if (ret != CHRONYCTL_SUCCESS) {
    fprintf(stderr, "[ntp] add_server failed: %s (%d)\n",
            chronyctl_strerror(ret), ret);
    /* handle or propagate */
}
```

### DNS Rotation Safety

`chronyctl_delete_server()` and `chronyctl_set_poll()` resolve hostnames through chronyd's live source list (`find_source_ip_by_name()`) rather than calling `getaddrinfo()` directly. This prevents `CHRONYCTL_ERROR_EXEC` (mapped from `STT_NOSUCHSOURCE`) when DNS has rotated the IP for a hostname after the server was originally added.

### Thread Safety Summary

This section defines the public concurrency contract for the API. Unless a function is explicitly documented here as safe for concurrent use, callers must assume it is **not** internally synchronized and must serialize access externally.

In particular, callers should use a process-local mutex (or otherwise ensure single-threaded access) around all `chronyctl_*()` operations except `chronyctl_strerror()`. `chronyctl_strerror()` is the only API documented as concurrently safe because it returns a pointer to an immutable static string literal.

| Function | Safe to call concurrently? |
|----------|---------------------------|
| `chronyctl_init()` | No — call once at startup |
| `chronyctl_cleanup()` | No — call once at shutdown |
| All other `chronyctl_*()` | No — serialize with external mutex |
| `chronyctl_strerror()` | Yes — returns pointer to immutable static string literal |

---

## Testing

### Building

```bash
cd libchronyctl
autoreconf -fiv         # generate configure script
./configure
make
```

This produces:
- `libchronyctl.la` — libtool shared library
- `test_timectl` — CLI test binary

### Running Tests with test_timectl

`test_timectl` requires a running `chronyd` on the host. All commands map 1:1 to public API functions:

```bash
# Verify library init and offset query
./test_timectl offset_check

# Full workflow: add → step → verify → delete
./test_timectl server pool.ntp.org 6 10
./test_timectl makestep
./test_timectl offset_check
./test_timectl delete_server pool.ntp.org

# Check source selection state
./test_timectl --backend=chrony selectable_check  # (if supported)
```

### Expected Error Scenarios

| Scenario | Expected error |
|----------|---------------|
| chronyd not running | `CHRONYCTL_ERROR_NO_DATA` |
| Calling API before `init` | `CHRONYCTL_ERROR_NOT_INIT` |
| Deleting a server not tracked by chronyd | `CHRONYCTL_ERROR_EXEC` |
| NULL output pointer | `CHRONYCTL_ERROR_INVALID` |
| chronyd socket permission denied | `CHRONYCTL_ERROR_UNAUTH` |

---

## See Also

- [libchronyctl.h](../libchronyctl.h) — Public API header and error code definitions
- [libchronyctl.c](../libchronyctl.c) — Full implementation with inline documentation
- [test_timectl.c](../test_timectl.c) — Backend-agnostic CLI and `ntp_ops_t` usage examples
- [architecture.md](architecture.md) — Internal design, component diagrams, socket lifecycle, performance, and platform notes
- [README.md](../README.md) — Component quick-start
- [CONTRIBUTING.md](../../CONTRIBUTING.md) — Contribution guidelines
- [CHANGELOG.md](../../CHANGELOG.md) — Version history

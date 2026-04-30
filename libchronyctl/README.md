# libchronyctl

Lightweight C library for programmatic control of the `chronyd` NTP daemon on RDK-based devices. It communicates directly over chronyd's native Unix domain socket protocol, replacing fragile `chronyc` CLI subprocess invocations with a typed, in-process API.

## Key Features

- **C interface for chronyd** — Typed API over Unix domain socket; no subprocess spawning.
- **Lightweight footprint** — No background threads, no persistent heap allocations, 8 bytes of static state.
- **Full NTP lifecycle control** — Query offset, add/remove servers, force clock step, burst polls, bring sources online.
- **DNS-rotation-safe source management** — `delete_server` and `set_poll` resolve hostnames through chronyd's live source list.
- **Extensible backend design** — The `ntp_ops_t` vtable in `test_timectl` allows new NTP daemon backends to be plugged in without changing the test harness.
- **Structured error codes** — `chronyctl_error` enum with `chronyctl_strerror()` for human-readable diagnostics.

## Usage Example

```c
#include "libchronyctl.h"
#include <stdio.h>

int main(void) {
    int    ret;
    int    has_source = 0;
    double offset     = 0.0;

    ret = chronyctl_init();
    if (ret != CHRONYCTL_SUCCESS) {
        fprintf(stderr, "init failed: %s\n", chronyctl_strerror(ret));
        return 1;
    }

    ret = chronyctl_has_selectable_source(&has_source);
    if (ret != CHRONYCTL_SUCCESS || !has_source) {
        fprintf(stderr, "no selectable source: %s\n", chronyctl_strerror(ret));
        chronyctl_cleanup();
        return 1;
    }

    ret = chronyctl_get_offset(&offset);
    if (ret == CHRONYCTL_SUCCESS)
        printf("NTP offset: %.9f s\n", offset);

    chronyctl_cleanup();
    return (ret == CHRONYCTL_SUCCESS) ? 0 : 1;
}
```

## API Reference

| Function | Description |
|---|---|
| `chronyctl_init()` | Initialize the library. Call once before any other API. |
| `chronyctl_cleanup()` | Release library state. Call once at shutdown. |
| `chronyctl_get_offset(double *offset_sec)` | Get the current NTP clock offset in seconds. |
| `chronyctl_has_selectable_source(int *has_sel)` | Check whether any NTP source is selected or selectable. |
| `chronyctl_makestep()` | Force an immediate clock step. |
| `chronyctl_add_server(addr, minpoll, maxpoll)` | Add an NTP server with specified poll intervals. |
| `chronyctl_delete_server(addr)` | Remove a tracked NTP server. |
| `chronyctl_set_poll(addr, minpoll, maxpoll)` | Update poll intervals for a tracked server. |
| `chronyctl_burst(addr, mask, n_good, n_total)` | Trigger a burst of NTP polls. Pass `NULL`/`NULL` for all sources. |
| `chronyctl_online(addr, mask)` | Bring matching NTP sources online. |
| `chronyctl_strerror(int err)` | Return a human-readable string for an error code. |

For full details on error codes, thread safety, and platform notes see [docs/api.md](docs/api.md). For internal design, component diagrams, and socket lifecycle see [docs/architecture.md](docs/architecture.md).

## Building

```bash
autoreconf -fiv
./configure
make
```

Produces `libchronyctl.la` (shared library) and `test_timectl` (CLI test binary).

## Testing

`test_timectl` requires a running `chronyd` on the host:

```bash
./test_timectl offset_check
./test_timectl server pool.ntp.org 6 10
./test_timectl makestep
./test_timectl selectable_check
./test_timectl delete_server pool.ntp.org
```

# time-utils

**RDK Centralized Time Management Libraries**

![Build Status](https://img.shields.io/badge/build-passing-brightgreen)
![License](https://img.shields.io/badge/license-Apache%202.0-blue)
![RDK Version](https://img.shields.io/badge/RDK-compatible-orange)

---

## Introduction

`time-utils` is the centralized RDK repository for time management on RDK-based devices. It provides a collection of libraries that abstract NTP daemon interactions, giving RDK components a stable, typed, in-process C interface for all time synchronization operations — without spawning shell subprocesses or parsing CLI output.

The initial library, **`libchronyctl`**, targets the `chronyd` NTP daemon and communicates directly over its native Unix domain socket protocol. The repository is structured to accommodate additional NTP client backends in the future through a backend-agnostic function-pointer table (`ntp_ops_t`).

---

## Key Features

- **C interface for chronyd** — Typed API that communicates directly with `chronyd` over its Unix domain socket, replacing brittle `chronyc` subprocess invocations.
- **Lightweight footprint** — No background threads, no persistent heap allocations, and only 8 bytes of static state (initialized flag + sequence counter).
- **Full NTP lifecycle control** — Initialize, query offset, add/remove servers, force clock step, trigger burst polls, and bring sources online.
- **DNS-rotation-safe source management** — `delete_server` and `set_poll` resolve hostnames through chronyd's live source list, preventing stale-IP mismatches after DNS rotation.
- **Extensible backend design** — The `ntp_ops_t` vtable in `test_timectl` allows new NTP daemon backends to be added by filling in a single struct row.
- **Structured error codes** — All functions return a `chronyctl_error` enum with a companion `chronyctl_strerror()` for human-readable diagnostics.

---

## Usage Example

The following snippet shows the typical pattern: initialize the library, verify that a selectable NTP source is available, query the current clock offset, then clean up.

```c
#include "libchronyctl.h"
#include <stdio.h>

int main(void) {
    int    ret;
    int    has_source = 0;
    double offset     = 0.0;

    /* Initialize the library (call once at startup) */
    ret = chronyctl_init();
    if (ret != CHRONYCTL_SUCCESS) {
        fprintf(stderr, "init failed: %s\n", chronyctl_strerror(ret));
        return 1;
    }

    /* Check whether chronyd has at least one selectable NTP source */
    ret = chronyctl_has_selectable_source(&has_source);
    if (ret != CHRONYCTL_SUCCESS || !has_source) {
        fprintf(stderr, "no selectable source: %s\n", chronyctl_strerror(ret));
        chronyctl_cleanup();
        return 1;
    }

    /* Query the current NTP clock offset */
    ret = chronyctl_get_offset(&offset);
    if (ret == CHRONYCTL_SUCCESS)
        printf("NTP offset: %.9f s\n", offset);
    else
        fprintf(stderr, "get_offset failed: %s\n", chronyctl_strerror(ret));

    chronyctl_cleanup();
    return (ret == CHRONYCTL_SUCCESS) ? 0 : 1;
}
```

**Compile:**
```bash
gcc -o ntp_check ntp_check.c -lchronyctl -lm
```

**Expected output:**
```
NTP offset: 0.000042371 s
```

### API Quick Reference

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

---

## Architecture

For a detailed description of internal design, component diagrams, socket lifecycle, error codes, and performance characteristics, see the [architecture overview](docs/architecture/overview.md).

**Summary:** `libchronyctl` is a thin synchronous shim that sits between the calling application and the `chronyd` daemon. Every API call opens a per-process `AF_UNIX SOCK_DGRAM` socket, exchanges exactly one request/reply packet with `chronyd` using its native wire protocol (`candm.h`), then closes and unlinks the socket before returning. There are no persistent connections, no background threads, and no heap allocations — making the library suitable for resource-constrained RDK embedded targets.

---

## Development

### Building

```bash
cd libchronyctl
autoreconf -fiv    # generate configure script from configure.ac
./configure
make
```

This produces:
- **`libchronyctl.la`** — libtool shared library
- **`test_timectl`** — backend-agnostic CLI test binary

### Running Tests

`test_timectl` provides a CLI wrapper around every public API function and requires a running `chronyd` on the host:

```bash
# Query current clock offset
./test_timectl offset_check

# Add an NTP server (minpoll=6 → 64 s, maxpoll=10 → 1024 s)
./test_timectl server pool.ntp.org 6 10

# Force an immediate clock step
./test_timectl makestep

# Check whether a selectable source is available
./test_timectl selectable_check

# Remove a server
./test_timectl delete_server pool.ntp.org
```

### Contributing

Contributions are welcome. Please review [CONTRIBUTING.md](CONTRIBUTING.md) before submitting a pull request. All contributors must sign the **RDK Contributor License Agreement (CLA)** before code can be accepted into the project.

---

## License

This project is licensed under the **Apache License, Version 2.0**. See [LICENSE](LICENSE) for the full text.


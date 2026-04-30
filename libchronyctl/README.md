# libchronyctl

Lightweight C library for programmatic control of the `chronyd` NTP daemon on RDK-based devices. It communicates directly over chronyd's native Unix domain socket protocol, replacing fragile `chronyc` CLI subprocess invocations with a typed, in-process API.

## Key Features

- **C interface for chronyd** — Typed API over Unix domain socket; no subprocess spawning.
- **Lightweight footprint** — No background threads, no persistent heap allocations, 8 bytes of static state.
- **Full NTP lifecycle control** — Query offset, add/remove servers, force clock step, burst polls, bring sources online.
- **DNS-rotation-safe source management** — `delete_server` and `set_poll` resolve hostnames through chronyd's live source list.
- **Extensible backend design** — The `ntp_ops_t` vtable in `test_timectl` allows new NTP daemon backends to be plugged in without changing the test harness.
- **Structured error codes** — `chronyctl_error` enum with `chronyctl_strerror()` for human-readable diagnostics.

## Documentation

- [docs/api.md](docs/api.md) — Full API reference, usage examples, error codes, thread safety, and testing guide.
- [docs/architecture.md](docs/architecture.md) — Internal design, component diagrams, socket lifecycle, performance, and platform notes.

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

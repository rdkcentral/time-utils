## Purpose

Describes the component structure of the `time-utils` repository — what each component produces, what it depends on, and which directory to modify for a given type of change.

---

## Diagram: Repo Component Overview

Shows how the components hosted in this repo relate to each other and to the external `chronyd` daemon.

→ [View diagram](../../diagrams/01-time-utils-repo-components.md)

---

## Components

`time-utils` is the centralized home for time-management libraries and tools used across RDK-based devices. Each component is built independently (its own `configure.ac` / `Makefile.am`) and, where it is large enough to warrant one, owns its own OpenSpec documentation set.

### `libchronyctl` → chronyd control library

- **Produces**: `libchronyctl.la` (shared library) + `test_timectl` (CLI test binary)
- **Depends on**: POSIX sockets, `libm` — no RDK/platform dependencies, no IARM/WPEFramework
- **Contains**: a typed, synchronous C API that speaks `chronyd`'s native Unix-domain-socket control protocol directly, replacing `chronyc` subprocess calls
- **OpenSpec docs**: [specs/libchronyctl/spec.md](../libchronyctl/spec.md) (this repo's top-level `openspec/`)

**Change this component when**: adding or changing a chronyd control operation, changing wire-protocol handling (`chrony_protocol.h`), or changing the per-call socket lifecycle.

### `systemtimemgr` → time-source arbitration daemon (consumer)

- **Produces**: `libsysTimeMgr.so` + `sysTimeMgr` binary, plus the `interface` and `systimerfactory` sub-packages
- **Depends on**: `libchronyctl` (only when the Chrony RFC feature flag is enabled), IARM, WPEFramework, optionally TEE/DTT
- **Contains**: the state machine that arbitrates between NTP, DRM/secure, and DTT time sources and decides when to trust and broadcast a given time-quality level
- **OpenSpec docs**: `systemtimemgr/openspec/` — a complete, independent OpenSpec instance. See [systemtimemgr/openspec/specs/architecture/spec.md](../../systemtimemgr/openspec/specs/architecture/spec.md) for its internal three-package structure, and [systemtimemgr/openspec/specs/chrony-ntp-sync/spec.md](../../systemtimemgr/openspec/specs/chrony-ntp-sync/spec.md) for the `libchronyctl` integration contract from the consumer side.

**Change this component when**: changing time-source arbitration, state-machine transitions, or platform IPC integration — start in its own `openspec/` first.

---

## Requirement: Dependency Direction Is One-Way

`systemtimemgr` MAY depend on `libchronyctl`; `libchronyctl` MUST NOT depend on `systemtimemgr` or any other component in this repo.

### Scenario: libchronyctl builds and is testable standalone

- **WHEN** `libchronyctl` is built via its own `configure.ac` / `Makefile.am`
- **THEN** it produces `libchronyctl.la` and `test_timectl` without requiring `systemtimemgr`, IARM, or WPEFramework to be present
- **AND** `test_timectl` can exercise the full public API against a bare `chronyd` instance

### Scenario: systemtimemgr links against libchronyctl only when Chrony RFC is enabled

- **WHEN** `systemtimemgr` is built with the Chrony RFC feature path enabled
- **THEN** `systimerfactory` links against `libchronyctl.h` / `libchronyctl.so`
- **AND** when the RFC flag is absent at runtime, no `chronyctl_*` call is made (see `systemtimemgr/openspec/specs/chrony-ntp-sync/spec.md`)

```
systemtimemgr  ──▶  libchronyctl   (build-time link; call-time gated by Chrony RFC flag)
                        │
                        ▼
                   chronyd daemon  (external process, via Unix domain socket)
```

---

## Adding a New Component

1. Create a top-level directory with its own `configure.ac` / `Makefile.am` (autotools component), following the pattern in [libchronyctl](../../../libchronyctl) or [systemtimemgr](../../../systemtimemgr).
2. Add an entry to this spec's **Components** section: produces / depends on / contains / OpenSpec docs / change triggers.
3. If the component is large or independently evolving enough to need its own requirement scenarios, give it its own `openspec/specs/<component>/` subfolder — follow `systemtimemgr/openspec/` as the full reference example.
4. List the component in the top-level [README.md](../../../README.md) under **Components**.

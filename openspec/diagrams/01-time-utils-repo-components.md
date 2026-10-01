# Diagram: time-utils Repo Component Overview

Shows the components hosted in this repo, what each one produces, and how they relate to each other and to the external `chronyd` daemon. `systemtimemgr` owns a complete, independent OpenSpec instance under its own directory — this diagram shows only the repo-level boundary between components.

**Related spec:** [specs/architecture/spec.md](../specs/architecture/spec.md)

---

```mermaid
graph TB
    subgraph REPO["time-utils repo"]
        subgraph LIB["libchronyctl component"]
            A1["libchronyctl.la\n(shared library)"]
            A2["test_timectl\n(CLI test binary)"]
            A3["libchronyctl.h / .c\nchrony_protocol.h\nchrony_address.h"]
            A3 --> A1
            A3 --> A2
        end

        subgraph MGR["systemtimemgr component\n(own openspec/ — see its architecture spec)"]
            B1["interface\n(header-only contract)"]
            B2["systimerfactory\n(platform adapters:\nIARM, WPEFramework,\nTEE/DTT, chronyctl)"]
            B3["systimemgr\n(core state machine,\nlibsysTimeMgr.so + sysTimeMgr)"]
            B1 --> B2
            B1 --> B3
            B2 --> B3
        end
    end

    B2 -->|"chronyctl_* calls\n(only when Chrony RFC\nfeature flag enabled)"| A1
    A1 -->|"AF_UNIX SOCK_DGRAM\nchronyd.sock"| C["chronyd daemon\n(external process)"]

    style LIB fill:#e8f4ff,stroke:#333
    style MGR fill:#fff4e6,stroke:#333
    style C fill:#f0f0f0,stroke:#333,stroke-dasharray: 5 5
```

**Key properties:**
- `libchronyctl` has zero dependency on `systemtimemgr` — it builds, links, and is testable (`test_timectl`) entirely standalone against a bare `chronyd`.
- `systemtimemgr` links against `libchronyctl` only in the `systimerfactory` platform-adapter package; the core `systimemgr` package never includes `libchronyctl.h` directly (see [systemtimemgr/openspec/specs/architecture/spec.md](../../systemtimemgr/openspec/specs/architecture/spec.md)).
- The `chronyctl_*` calls themselves are further gated at runtime by the Chrony RFC feature flag — see [systemtimemgr/openspec/specs/chrony-ntp-sync/spec.md](../../systemtimemgr/openspec/specs/chrony-ntp-sync/spec.md) for that contract.
- For the internal thread/state-machine flow inside `systemtimemgr`, see its own diagram: [systemtimemgr/openspec/diagrams/03-systimemgr-architecture.md](../../systemtimemgr/openspec/diagrams/03-systimemgr-architecture.md).

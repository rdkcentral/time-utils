# Diagram: libchronyctl Request/Reply Socket Lifecycle

Shows the exact sequence every `chronyctl_*` data-plane call follows (socket creation through cleanup), plus the decision flow for the two DNS-rotation-safe operations (`delete_server`, `set_poll`) that resolve their target through chronyd's live source list instead of fresh DNS.

**Related spec:** [specs/libchronyctl/spec.md](../specs/libchronyctl/spec.md)

---

## Per-Call Lifecycle

```mermaid
sequenceDiagram
    participant App
    participant libchronyctl
    participant Kernel
    participant chronyd

    App->>libchronyctl: chronyctl_*(args)
    libchronyctl->>libchronyctl: chronyctl_initialized?
    alt not initialized
        libchronyctl-->>App: CHRONYCTL_ERROR_NOT_INIT
    else initialized
        libchronyctl->>Kernel: socket(AF_UNIX, SOCK_DGRAM, 0)
        libchronyctl->>Kernel: bind(/run/chrony/chronyc.<tid>.sock)
        libchronyctl->>Kernel: chmod(..., 0666)
        libchronyctl->>Kernel: connect(/run/chrony/chronyd.sock)
        alt chronyd socket unreachable
            Kernel-->>libchronyctl: connect() fails (both paths)
            libchronyctl->>Kernel: unlink(chronyc.<tid>.sock)
            libchronyctl-->>App: CHRONYCTL_ERROR_NO_DATA
        else connected
            libchronyctl->>Kernel: setsockopt SO_RCVTIMEO = 2s
            libchronyctl->>chronyd: send(CMD_Request, REQ_*, sequence++)
            alt reply within 2s
                chronyd-->>libchronyctl: recv(CMD_Reply, RPY_*)
                libchronyctl->>libchronyctl: validate version / status
                libchronyctl->>Kernel: close(sockfd)
                libchronyctl->>Kernel: unlink(chronyc.<tid>.sock)
                libchronyctl-->>App: CHRONYCTL_SUCCESS or mapped error (e.g. ERROR_UNAUTH on STT_UNAUTH)
            else recv timeout (2s)
                libchronyctl->>Kernel: close(sockfd)
                libchronyctl->>Kernel: unlink(chronyc.<tid>.sock)
                libchronyctl-->>App: CHRONYCTL_ERROR_EXEC
            end
        end
    end
```

> **Concurrency note**: the local socket name embeds the calling thread's TID (`gettid()`), not the process PID, so two threads calling `chronyctl_*` at the same time bind/unlink distinct paths and never race.

---

## DNS-Rotation-Safe Source Resolution (`delete_server`, `set_poll`)

```mermaid
flowchart TD
    A["chronyctl_delete_server(hostname)\nor chronyctl_set_poll(hostname, ...)"] --> B["send REQ_N_SOURCES"]
    B --> C["recv RPY_N_SOURCES → source_count"]
    C --> D{"i < source_count?"}
    D -- "No — exhausted list" --> E["CHRONYCTL_ERROR_EXEC\n(hostname not tracked by chronyd)"]
    D -- "Yes" --> F["send REQ_SOURCE_DATA(index=i)"]
    F --> G["recv RPY_SOURCE_DATA → ip_address"]
    G --> H["send REQ_NTP_SOURCE_NAME(ip_address)"]
    H --> I["recv RPY_NTP_SOURCE_NAME → name"]
    I --> J{"strncasecmp(name, hostname) == 0?"}
    J -- "No" --> K["i++"]
    K --> D
    J -- "Yes — match found" --> L["use this ip_address\n(network byte order, as tracked\nby chronyd right now)"]
    L --> M["send REQ_DEL_SOURCE / REQ_MODIFY_MINPOLL\nwith resolved ip_address"]
    M --> N["CHRONYCTL_SUCCESS or ERROR_EXEC"]
```

**Why this matters:** if `getaddrinfo()` were called fresh at delete/set-poll time, a DNS record that rotated after the server was added could resolve to a different IP than the one chronyd actually tracks, causing `REQ_DEL_SOURCE` / `REQ_MODIFY_MINPOLL` to fail with `STT_NOSUCHSOURCE`. Walking chronyd's live source list first guarantees the IP used on the wire matches what chronyd currently has on record.

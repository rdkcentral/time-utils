## Purpose

Describes the structure of the `time-utils` repository so new requests can be scoped correctly: which directory to change, and where to add the spec for it.

This repo currently hosts **one** component. The structure below is written so that adding a second component later is a predictable, low-friction step.

---

## Current Components

| Component | Directory | Produces | Spec |
|---|---|---|---|
| `libchronyctl` | [libchronyctl/](../../../libchronyctl) | `libchronyctl.la` (shared library), `test_timectl` (CLI test binary) | [specs/libchronyctl/spec.md](../libchronyctl/spec.md) |

---

## Requirement: Each Component Owns Exactly One Spec Folder

Every top-level buildable component in this repo MUST have a matching folder under `openspec/specs/<component-name>/spec.md` that describes its behavior in requirement/scenario form.

### Scenario: Finding the spec for existing code

- **WHEN** someone wants to understand or change `libchronyctl`
- **THEN** [specs/libchronyctl/spec.md](../libchronyctl/spec.md) is the single source of truth for its expected behavior

### Scenario: Adding a new component to the repo

- **WHEN** a new top-level buildable component is added (its own `configure.ac`/`Makefile.am`, e.g. a new library or tool)
- **THEN** a new `openspec/specs/<component-name>/spec.md` is created for it
- **AND** a row is added to the **Current Components** table above
- **AND** the component is listed in the top-level [README.md](../../../README.md) under **Components**

---

## Requirement: New Requests Are Written as Spec Changes First

To add a new capability or change existing behavior, write the requirement as a new (or updated) `Requirement` + `Scenario` block in the relevant component's spec **before** writing code.

### Scenario: Adding a new capability to an existing component (e.g. libchronyctl)

- **WHEN** a new function, behavior, or rule is requested for `libchronyctl`
- **THEN** add a new `## Requirement: <short name>` section to [specs/libchronyctl/spec.md](../libchronyctl/spec.md) with one or more `### Scenario:` blocks describing the expected `WHEN` / `AND` / `THEN` behavior
- **AND** implement the code to satisfy that spec
- **AND** keep the spec and the code in sync — if behavior changes, the spec changes in the same PR

### Scenario: Changing existing behavior

- **WHEN** existing behavior needs to change (not just extend)
- **THEN** update the relevant `Scenario` in place rather than leaving the old, now-incorrect scenario in the spec

---

## Adding a New Component — Checklist

1. Create `<component-name>/` at the repo root with its own `configure.ac` / `Makefile.am`, following [libchronyctl](../../../libchronyctl) as the reference layout.
2. Create `openspec/specs/<component-name>/spec.md` describing its behavior as `Requirement` / `Scenario` blocks.
3. Add it to the **Current Components** table in this file.
4. Add it to the top-level [README.md](../../../README.md) **Components** list.
5. If the component's call flow benefits from a diagram, add one under `openspec/diagrams/` and link it from the top of the component's spec.

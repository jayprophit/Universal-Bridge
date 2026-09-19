# Supplied Research Traceability

This document records how the six supplied research/specification texts are incorporated without treating research prose as implemented capability. The source texts remain external evidence; the checked-in requirements, architecture, code, tests, and qualification records are the product truth.

## Document-by-document mapping

| Supplied document | Durable contribution | Canonical repository location | Current gap or gate |
|---|---|---|---|
| Universal Hardware-to-DAW Bridge 100-problem opportunity audit | Cross-manufacturer problem taxonomy, competitive fragmentation, ten initial functional systems | `MASTER_REQUIREMENTS_MATRIX.md`, `RESEARCH_AND_PRODUCT_MATRIX.md`, `CAPABILITY_MATRIX.md` | Competitor observations must be refreshed from primary sources before publication; no competitor compatibility is inferred |
| MPC top 100 software-solvable problems | MPC-family backlog, MPC Sample reference workflow, MPC-specific value ordering | Requirements 14–16, 19, 24, 26–27, 34–35, 46, 49–50, 55, 59, 64, 66, 69, 73, 76–98 | Proprietary formats, live controller behavior, firmware differences, and automated capture require cleared evidence and real-device tests |
| Merged master 100-problem specification | De-duplicated IDs 1–100, source lineage, delivery priority, reusable-engine mapping | `MASTER_REQUIREMENTS_MATRIX.md` contains exactly 100 stable IDs; `RESEARCH_AND_PRODUCT_MATRIX.md` adds lineage, difficulty, cooperation, revenue, and competition fields | No requirement is considered qualified merely because its engine contract exists |
| Cross-device and cross-platform expansion | One shared C++ core; device, DAW, OS, connection, and form-factor adapters; per-family research feeds one engine backlog | `BUILDABLE_ARCHITECTURE.md`, `PLATFORM_CONTRACT.md`, device/platform/DAW profiles | Only Windows project-folder preflight is an implemented reference route; other native platforms remain unqualified |
| Missing foundational layers review | Protocol discovery, capability negotiation, canonical parameters, IPC, real-time separation, rollback, virtual hardware, trust/signing, identity, multi-device routing | E1, E2, E5, E10–E12 and the new E14; `protocol_capabilities.hpp`; session/sync/test-lab code | IPC, real-time audio, persistent crash recovery, signed adapter loading, and multi-device routing remain later qualified implementations |
| Mobile/tablet as real bridge hosts | Direct mobile hosting is distinct from companion control; effective features are device × OS × DAW × connection | `PlatformCapability::direct_mobile_host_route`, `IntegrationPlan::mobile_bridge`, E14 decisions, mobile platform profiles | Android, ChromeOS, iPadOS, and iOS direct-host routes remain disabled until native runtimes and exact USB/audio/MIDI/DAW combinations are qualified |

## Stable requirements rule

The merged research maps one-to-one to requirement IDs 1–100. Raw universal and MPC lists are source lineage, not additional duplicate requirements. Future SP-404, Elektron, Maschine, Circuit, or other device-family audits must either:

1. map a finding to an existing stable requirement and improve its acceptance evidence; or
2. add a new requirement with a new ID, source, engine owner, delivery stage, and qualification gate.

An existing ID must never be silently deleted, repurposed, or renumbered.

## Engine-number preservation

The existing repository had already assigned E13 to the Workflow Orchestrator before the supplied mobile-host text named a thirteenth Host Capability & Routing Negotiator. Renumbering E13 would break earlier traceability. The host negotiator is therefore E14:

| Engine | Responsibility | Current implementation boundary |
|---|---|---|
| E13 Workflow Orchestrator | Preflight, backup, approval, execution plan, verification, rollback/result flow | Safe folder workflow is partial; parser/capture/direct-host/sync steps remain gated |
| E14 Host Capability & Routing | Combine device, OS, DAW, connection, form factor, and evidence maturity into an effective feature set | Evidence-aware C++ decisions are implemented and tested; native mobile and direct DAW routes remain unqualified |

## Evidence maturity contract

Protocol and host facts progress monotonically:

```text
unavailable → declared → observed → qualified
```

- **Declared** means a profile or user-selected connection describes a possible route.
- **Observed** means an OS/platform adapter saw an identity, class service, or endpoint without proving usable direction, channels, semantics, timing, or safety.
- **Qualified** means the exact route passed its physical, host, timing, recovery, and regression gates.

Only qualified evidence may activate live MIDI control, audio capture, bidirectional state synchronization, or direct mobile hosting. Read-only file preflight may use a declared route because it remains outside the source project and produces a transparent report/package.

For the observed MPC Sample, Windows currently proves the shared container and Audio-class service identities only. `MI_03` remains unknown. Container membership must not be used to label it CDC-NCM, MIDI, storage, control, or any proprietary protocol.

## Research versus delivery truth

The supplied opportunity analysis describes a plausible product category and product value. It does not establish:

- a legal right to parse or write a proprietary format;
- a documented device-control protocol;
- functional MIDI/audio stream direction or channel counts;
- a DAW API capable of producing an equivalent native project;
- measured latency, sample accuracy, drift, recovery, or real-time safety;
- native Android, ChromeOS, iPadOS, or iOS availability;
- a signed, installed, supported commercial release.

Those claims require the release gates in `CAPABILITY_MATRIX.md` and `PRODUCTION_STATUS.md`.

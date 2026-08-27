# Buildable Universal Bridge Architecture

The Universal Bridge is organized as a reusable local core with adapters around it. This structure is designed to cover the entire supplied specification without forcing every device, DAW, platform, or plug-in format into one unstable executable.

> The standalone app, background service, VST3/AU/CLAP clients, mobile companion, hardware adapters, and DAW adapters are all consumers of one canonical session and capability engine. None of them owns the project truth independently.

## Layer diagram

```text
┌───────────────────────────────────────────────────────────────────┐
│ UX clients: Desktop App · VST3/AU/CLAP · Mobile/Tablet · CLI       │
└───────────────────────────┬───────────────────────────────────────┘
                            │ authenticated local IPC
┌───────────────────────────▼───────────────────────────────────────┐
│ Workflow Orchestrator: preflight · approval · execute · verify     │
│                        rollback · result summary                   │
└──────────┬────────────────┬────────────────┬──────────────────────┘
           │                │                │
┌──────────▼──────┐ ┌───────▼─────────┐ ┌───▼──────────────────────┐
│ Session & Sync  │ │ Capability Graph │ │ Diagnostics & Safety     │
│ revisions/diffs │ │ device/OS/DAW/   │ │ trace/recovery/health    │
│ policies/conflict│ │ connection plan │ │ privacy/permissions      │
└──────────┬──────┘ └───────┬─────────┘ └───┬──────────────────────┘
           │                │                │
┌──────────▼────────────────▼────────────────▼──────────────────────┐
│ Shared C++ Core: assets · MIDI · audio/timing · routing · archive  │
└───────┬────────────────┬────────────────┬─────────────────────────┘
        │                │                │
┌───────▼────────┐ ┌─────▼─────────┐ ┌───▼────────────────────────┐
│ Device adapters│ │ DAW adapters  │ │ Platform backends          │
│ MPC/SP/Elektron│ │ Cubase/Reason │ │ Windows/macOS/Linux/mobile │
│ Maschine/etc.  │ │ Ableton/etc.  │ │ files/USB/audio/MIDI/IPC   │
└────────────────┘ └───────────────┘ └────────────────────────────┘
```

## Build targets

| Target | Form | Responsibility | May access real hardware? |
|---|---|---|---|
| `ubridge_core` | Portable C++ static/shared library | Canonical models, capability negotiation, conflict detection, transaction state | No |
| `ubridge` | Portable CLI executable | Read-only intake, backup, asset/MIDI exchange, diagnostics | No in current prototype |
| `ubridge_service` | Future local daemon | Device discovery, audio/MIDI backends, IPC, reconnect/recovery | Only after platform/device qualification |
| `ubridge_desktop` | Future native desktop app | Device/session UI, preflight, mapping, diagnostics, user approvals | Through service only |
| `ubridge_vst3` | Future plug-in client | Session identity/persistence and bounded real-time-safe interaction | Never direct file/device enumeration in audio thread |
| `ubridge_mobile` | Future iPadOS/iOS/Android/ChromeOS apps | Companion mode first; direct host only for qualified routes | Through native mobile platform backend |
| `ubridge_test_lab` | Test executable/library | Virtual devices, fixtures, packet/timing replay, regression tests | No |

## Core contracts implemented now

The compiled `ubridge_core` library introduces the key structures that every future module must use. They are deliberately generic: `CanonicalSession`, `AssetReference`, `MusicalEvent`, `CanonicalParameter`, `RevisionVector`, `DeviceCapability`, `PlatformCapability`, `DawCapability`, `ConnectionCapability`, `IntegrationPlan`, `Change`, `Conflict`, and `Transaction`.

| Contract | Rule |
|---|---|
| `CanonicalSession` | Holds vendor-neutral session data and schema/revision identity; device and DAW formats are adapters around it |
| `IntegrationPlan` | Is calculated from device × OS × DAW × connection; limitations are first-class output, not hidden failure |
| `SyncPolicy` | Defines authority for every mutable field: hardware, DAW, one-way, bidirectional, render-only, or unsupported |
| `TransactionJournal` | Enforces `planned → approval → running → verified → committed` or safe `rolled_back`/`failed` outcomes |
| `Conflict` | Is created when both hardware and DAW change the same field to different values; it must be reviewed unless a declared authority policy resolves it |
| `Diagnostic` | Is structured, severity-ranked, and suitable for CLI, desktop UI, support bundles, and test assertions |

## Adapter contracts

Every adapter must declare a compatibility profile before performing work. An adapter is responsible for **translation**, not for changing product safety policy.

| Adapter | Required inputs | Required outputs | Prohibited behavior before qualification |
|---|---|---|---|
| Device | Identity, firmware, storage/MIDI/audio/control availability | `DeviceCapability`, project/session translation, recoverable action plan | Write project data, send undocumented protocol commands, claim unavailable audio channels |
| DAW | Host version, plug-in/interchange route, import/create/control capabilities | `DawCapability`, host route, import/create report, host-state persistence | Write proprietary project files without a verified documented route |
| Platform | OS version, permissions, file/USB/audio/MIDI/IPC availability | `PlatformCapability`, backend diagnostics, path/permission route | Claim physical device access merely from compilation portability |
| Connection | Storage, USB MIDI/audio, analogue audio, network path | `ConnectionCapability`, endpoint identity, timing limits | Infer a connection feature that was not probed or user-configured |

## Real-time and data-safety rules

The product will never parse projects, access disk, enumerate USB devices, perform network operations, write traces, load profiles, or negotiate connections on a DAW audio thread. Plug-ins exchange bounded messages with the local service; the service owns hardware, filesystem, and long-running work. Each source project begins read-only and each potentially destructive action begins with an approved transaction and a restorable snapshot.

## Source tree

```text
include/ubridge/core/    public canonical models and contracts
src/core/                shared implementations
src/adapters/            device and DAW adapter modules
src/platform/            native platform and IPC backends
src/orchestrator/        workflow plans, approvals, transactions
src/test_lab/            virtual hardware and regression harness
profiles/                declarative device, DAW, platform, connection profiles
fixtures/                sanitized project/audio/MIDI/trace fixtures
tests/                   deterministic integration tests
docs/                    requirements matrix, contracts, roadmap, legal gates
```

The v0.2.0 codebase implements the first compile-tested shared core and CLI. The next source updates add interfaces/stubs for every remaining module so they can be built, mocked, and tested before any unverified device or DAW control is enabled.

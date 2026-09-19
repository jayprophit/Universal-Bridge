# Architecture Decision Record: Universal Bridge Foundation

**Status:** Accepted for the developer prototype  
**Reference path:** MPC Sample → Windows → Cubase / Reason  
**Scope:** local session intake and exchange package, not live hardware control

## Decision

Universal Bridge will use a **shared portable C++ core** with narrowly scoped platform, device, and DAW adapters. The core owns the canonical session, asset inventory, capability calculation, diagnostics, transaction records, and future synchronization rules. Applications and plug-ins are clients of that core; they must not independently implement device access, parsing, or project state.

The first executable is intentionally a command-line preflight engine. This is the correct foundation for a later desktop interface because it makes the core’s behavior deterministic, scriptable, testable, and usable from a future background service without coupling business logic to a UI toolkit.

## System context

```text
Hardware project / hardware connection
                │
                ▼
  Device Adapter + Protocol Discovery
                │
                ▼
      Capability Negotiator
                │
                ▼
      Canonical .ubridge Session
  ┌─────────────┼────────────────┐
  ▼             ▼                ▼
Assets       MIDI/Control      Sync/Recovery
  │             │                │
  └─────────────┴────────────────┘
                │
                ▼
            DAW Adapter
                │
      Cubase / Reason / later hosts
```

The system must calculate the effective integration route from **device × operating system × DAW × connection**. A device profile alone cannot guarantee a feature because an MPC connected by storage has different capabilities than the same MPC connected by USB MIDI or stereo audio, and because DAWs expose different host interfaces.

## Prototype module contract

| Module | Current responsibility | Explicitly excluded in v0.1.0 |
|---|---|---|
| CLI/workflow shell | Receive paths and target DAW, enforce source/output separation, present a concise result | Native GUI and background service |
| Project intake | Traverse a selected folder read-only and classify known extensions | Parsing proprietary MPC project semantics |
| Asset inventory | Record relative path, size, type, FNV-1a fingerprint, and copied status | Audio decoding, waveform inspection, duplicate grouping across projects |
| Neutral session | Write a versioned JSON state record of provenance, capabilities, assets, and findings | Full tracks, patterns, effects, automation, and cross-device model |
| Device profile | Declare MPC Sample reference assumptions and prohibited operations | USB enumeration, MIDI handling, control messages, write-back |
| DAW adapters | Declare Cubase/Reason exchange route and generate import guidance | Proprietary project-file writing, host automation, track creation |
| Backup and recovery | Copy input project into external output backup and report its status | Transactional hardware/DAW rollback |
| Diagnostics | Produce visible and machine-readable limitations and outcomes | USB/audio/MIDI packet trace capture |

## Data safety and authority

No production bridge should decide silently that hardware or DAW has priority. The canonical session will eventually assign an authority policy to every value: `hardware_authoritative`, `daw_authoritative`, `one_way_mapped`, `bidirectional`, `render_only`, or `unsupported`. In v0.1.0, every hardware-write and live synchronization policy resolves to `unsupported`.

Each potentially destructive operation must become a transaction with the following lifecycle: **preflight → snapshot → plan → user approval → execute → verify → commit or roll back**. The prototype implements the first two conservative elements: preflight and separate backup. Future work must add journal persistence, revision vectors, UI review, cancellation recovery, and source-specific recovery procedures.

## Real-time safety boundary

When audio capture and a VST3 client are added, the audio callback must only carry bounded, preallocated, real-time-safe data. Filesystem traversal, hashing, project parsing, device discovery, logs, network transport, IPC reconnection, user interface updates, and profile loading run outside the callback. This follows the design premise of VST3 as an API for real-time processing components, and it mirrors the separation Reason documents between its real-time context and message-driven non-real-time work.[1] [2]

## Adapter evolution

### Device adapters

A production device adapter should implement a stable universal interface with methods such as identity/discovery, declared capability, project intake, asset resolution, MIDI endpoint registration, audio endpoint registration, normalized parameter mapping, safe action planning, and recovery. Device support must begin read-only. No write-back can ship until project-format semantics, firmware behavior, rollback guarantees, and legal rights to access the relevant protocol have been validated.

### DAW adapters

The default adapter route is an interchange package. A direct route may be added only when an exact target version offers a documented, testable API or supported workflow. The future shared VST3 client should preserve the bridge session identity when a DAW project is reopened, communicate with a local service through authenticated IPC, and never assume a host supports optional VST3 interfaces. Reason’s VST3 support and its Rack Extension environment are separate possible routes, each requiring its own distribution and technical feasibility decision.[2] [4]

### Platform adapters

Windows is the production reference because it matches the available MPC Sample, Cubase, and Reason workflow. The C++ core must avoid Windows-only assumptions. macOS and Linux are subsequent desktop ports; Android, ChromeOS, iPadOS, and iOS receive companion support before direct bridge-host support unless an OS/host combination passes the full capability and recovery tests.

## Product state from this decision

This foundation protects the long-term universal goal while avoiding two common failures: building a one-off MPC utility that cannot accommodate a second device, and making a universal marketing promise that hides device/DAW limitations. The correct immediate output is a **truthful exchange workflow**. The correct long-term output is a capability-aware bridge that can create the best verified route for each supported combination.

## References

[1]: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Index.html "Steinberg — VST 3 Technical Documentation"
[2]: https://developer.reasonstudios.com/documentation/rack-extension-sdk/4.3.0/rack-extension-dev-guide "Reason Studios — Rack Extension Developer Guide"
[3]: https://www.steinberg.net/developers/vstsdk/ "Steinberg — About the VST SDK"
[4]: https://www.reasonstudios.com/press/vst3-support-for-the-reason-music-making-software "Reason Studios — VST3 Support for Reason"

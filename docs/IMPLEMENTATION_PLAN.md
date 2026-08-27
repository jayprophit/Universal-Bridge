# Universal Hardware Session Bridge — Recommended Implementation Plan

## Goal and planning decision

The goal is to build a **local-first Universal Hardware Session Bridge** that takes a project made on supported music hardware, preserves its musical meaning, and reconstructs a usable session in a DAW while retaining a safe path for subsequent synchronization. The first reference workflow will be **Akai MPC Sample → Windows 11 → Cubase and Reason**, because it aligns with the available hardware and DAWs for continuous real-world testing. The architecture will be intentionally portable to macOS, Linux, Android, ChromeOS, iPadOS, and iOS; however, feature parity will be earned through capability testing rather than promised uniformly across all host combinations.

> **Product principle:** one session model, many hardware devices, many DAWs, and capability-aware fallbacks. The bridge must never claim a device or DAW can do more than its hardware, operating system, or published integration surface permits.

The supplied research brief establishes the product direction: a universal **hardware-to-DAW session layer**, not merely a stem exporter, MIDI mapper, or conventional DAW plug-in. This plan converts that direction into an initial, buildable product sequence.

## Recommended technical route

A native, local desktop product is the correct first implementation. It is the only route that can safely access USB/MIDI/audio devices, project folders, real-time audio paths, and DAW plug-in interfaces while avoiding dependence on an internet connection or remote service. The background component will run **on the producer’s own computer** and be controlled by the desktop application; it is not a cloud-hosted service.

| Approach | Tradeoffs | Cost | Setup complexity |
|---|---|---:|---|
| **Recommended: native C++ bridge core, desktop app, and DAW plug-in clients** | Best fit for USB, MIDI, audio timing, VST3/AU/CLAP, local project access, and future mobile bindings. It requires disciplined C++ engineering and device-specific testing. | No required hosted-service cost; standard signing and developer-program costs apply when distributing platform builds. | High initially, but lowest long-term integration risk. |
| **Alternative: desktop web shell plus native service** | Faster iteration for settings and dashboards, but real-time audio, plug-in integration, USB access, and inter-process reliability still require a full native service. This duplicates more architecture. | No hosting required for a local app; similar distribution costs. | Medium-to-high, with extra communication and packaging complexity. |

The plan selects the **native route**. Use a **C++20 shared core with CMake**, a cross-platform native desktop UI framework, and thin host-specific layers. A VST3 client is the initial DAW-facing plug-in because both target DAWs can be evaluated through the same plug-in family; the plug-in remains an IPC client and must not own USB/audio capture logic. AU, CLAP, and other plug-in formats are subsequent wrappers around the same core. Any framework or SDK choice must be approved against its current commercial licensing before implementation begins.

## Scope boundaries and honest MVP promise

The first product must deliver a meaningful end-to-end workflow without claiming complete universal, bidirectional control on day one. The initial release will support a **Tier B** device path: project files, MIDI, and stereo USB/analogue audio, with automated or assisted sequential stem capture where the hardware exposes control required for safe automation.

| Included in the Windows MVP | Deferred or conditional work |
|---|---|
| MPC Sample discovery, firmware/capability inventory, non-destructive project intake, and project backup | Simultaneous multichannel capture from stereo-only devices, which is physically impossible without manufacturer support |
| Canonical `.ubridge` session, asset manifest, MIDI/performance representation, project/source hashes, and versioned metadata | Universal lossless reconstruction of proprietary DAW project files where no stable public API or import route exists |
| MPC project and asset dependency analysis; naming, missing-sample reporting, and a preflight diagnostics report | Full two-way synchronization of every hardware effect, automation lane, and proprietary parameter |
| Reconstructed track/package output containing aligned audio, MIDI, names, tempo, arrangement metadata, mixer data where mappable, and an import report | AAX, community profile marketplace, cloud collaboration, and remote operation |
| Cubase and Reason integration feasibility spike; VST3 bridge client, standard MIDI/audio exchange bundle, and direct integration only where documented host capabilities permit it | macOS, Linux, mobile/tablet native hosts, ChromeOS, and second-device support in the first executable release |
| MIDI clock/transport and an explicit, supported parameter-mapping subset, with per-parameter direction and conflict rules | An embedded full DAW, audio editor, synth collection, or broad mastering suite |

The initial user experience will be **“Finish in Cubase”** or **“Finish in Reason.”** It will first execute a preflight, create a transaction-safe bridge session, collect or capture the available assets, generate the DAW-facing package, and show exactly which elements were reconstructed, rendered, mapped, limited, or unavailable. Where direct DAW project creation cannot be justified by a documented interface, the bridge will generate a deterministic import bundle and template instead of silently producing an incomplete or misleading project.

## Core architecture

The core will be structured around three worlds: connected hardware, a canonical session, and DAW hosts. All interfaces flow through explicit capability negotiation and a transaction layer. The project will be designed so each new hardware device, DAW, operating system, or connection method adds an adapter and test profile rather than requiring changes to every subsystem.

| Layer | Responsibilities | Initial implementation priority |
|---|---|---|
| **Platform abstraction** | File access, USB/device enumeration, MIDI I/O, audio I/O, virtual ports, process control, permissions, and secure local IPC | Windows first; APIs isolated for macOS/Linux/mobile bindings later |
| **Protocol discovery and capability negotiator** | Detect device identity, firmware, MIDI/audio/storage/HID availability, OS/DAW conditions, connection type, and derive an effective feature set | Required before adding device-specific logic |
| **Device-profile API** | Normalize device actions, project reading, parameter mapping, transport, sample access, and profile-declared limitations | MPC Sample profile #1 |
| **Canonical session model** | Versioned project graph for devices, tracks, pads, clips/sequences, MIDI, automation, mixer state, routing, effects intent, assets, and provenance | Required before parsers or DAW writers |
| **Project parser and asset manager** | Read project folders without modifying them; resolve dependencies; fingerprint assets; create backups; surface conversion warnings | MPC Sample parser and fixture corpus first |
| **Audio, stem, and timing engine** | Capture, sequentially render where supported, detect failures/empty takes, retain wet/dry provenance, calibrate and apply alignment | Stereo input and repeatable capture workflow first |
| **MIDI/control engine** | Notes, velocity, timing, CC, transport, clock, mapping, and supported bidirectional parameter paths | MIDI recording/export and mapped control subset first |
| **Synchronization and transaction engine** | Revision hashes, diffs, source authority, conflict review, rollback journal, offline changes, and recoverable operations | Snapshot/reconcile first; live synchronization grows per capability |
| **DAW adapter API** | Discover host capabilities, expose plug-in IPC client, write interchange data, generate import templates, and add direct actions only when supported | Cubase and Reason first |
| **Diagnostics and virtual hardware laboratory** | Explain connection/import/sync limitations, collect opt-in traces, emulate devices, replay fixtures, and run regression tests | Required from the first executable build |

### Canonical session and synchronization rules

The `.ubridge` session will be the product’s source of truth for bridge operations, but it will **not overwrite either original project automatically**. It stores a versioned state graph, source file/device fingerprints, immutable backups, change hashes, and the effective-capability report for the exact device × OS × DAW × connection combination.

Each field will declare a synchronization policy: **hardware-authoritative**, **DAW-authoritative**, **one-way mapped**, **bidirectional with conflict handling**, **render-only**, or **unsupported**. If both sides changed while disconnected, the bridge creates a reviewable conflict rather than guessing. Every import, capture, or synchronization operation runs in a journaled transaction with a rollback point and a human-readable summary.

## Repository and module structure

The implementation will be maintained as a modular monorepository so proprietary format work, test fixtures, plug-in wrappers, and profiles remain auditable and replaceable.

```text
universal-bridge/
├── core/                 # C++ session, sync, timing, assets, capability model
├── platform/             # Windows now; macOS/Linux/Android/iOS bindings later
├── device-adapters/      # MPC Sample first; later Akai/Roland/Elektron/etc.
├── daw-adapters/         # Cubase and Reason first; shared interchange adapters
├── apps/desktop/         # Native standalone UI and local-service controller
├── plugins/              # VST3 first; AU and CLAP wrappers later
├── profiles/             # Signed declarative device/DAW/connection profiles
├── test-lab/             # Virtual hardware, MIDI/audio replay, simulators
├── fixtures/             # Sanitized project files and expected bridge sessions
├── docs/                 # Architecture decision records, capability matrix, UX specs
└── tools/                # Fixture builders, migration tools, diagnostic parsers
```

## Delivery phases

### Phase 0 — feasibility, rights, and acceptance contracts

Freeze the MVP definition and collect the required reference materials: the MPC Sample firmware/software versions, a set of sanitized representative projects, Windows/Cubase/Reason versions, an audio interface inventory, and example desired project outcomes. Validate all usable DAW extension, project-import, plug-in, driver, SDK, and file-format routes before promising direct project creation. Record licensing constraints for VST3, ASIO, hardware protocol use, audio libraries, UI framework, and distribution signing.

The output is an architecture decision record, a capability matrix, a non-destructive data-handling policy, and measurable acceptance scenarios for a basic drum program, melodic/sample program, arrangement, mixer state, automation, missing sample, stereo-only capture, and conflicting edit.

### Phase 1 — universal foundation and virtual hardware laboratory

Build the C++ core interfaces, canonical session schema, revision/transaction journal, content hashing, structured diagnostics, device-profile manifest format, and capability calculation. Create a virtual MPC Sample adapter with recorded/synthetic MIDI, audio, and project fixtures so parsing and sync tests can run without physical hardware attached.

This phase also establishes the critical real-time boundary: plug-in and audio callback code may only exchange bounded data with lock-free or otherwise real-time-safe mechanisms. Disk traversal, project parsing, network activity, UI work, hashing, and diagnostics never run on the real-time audio thread.

### Phase 2 — Windows desktop bridge and MPC Sample intake

Implement the first Windows desktop application and its managed local service. Add device discovery, USB/MIDI/audio/storage inventory, MPC Sample capability report, project-folder selection or supported storage intake, read-only parsing, backup creation, sample dependency collection, and a visual preflight report. The app will create a `.ubridge` session and record every inferred or unavailable feature.

The initial interface should optimize for the user’s actual workflow: select/detect MPC Sample, select **Cubase** or **Reason**, choose a project, inspect the preflight, select capture and effects options, run the bridge transaction, then review the result and diagnostics.

### Phase 3 — session reconstruction, audio/MIDI export, and controlled capture

Map the supported MPC Sample project elements into the neutral graph. Produce well-named audio/MIDI assets and a clear arrangement/mixer/automation metadata representation. Implement a deterministic exchange package with track folders, pad/track names, tempo/time signature, MIDI files, asset manifest, import notes, and a machine-readable completion report.

Add latency measurement, start-offset compensation, tail handling, take validation, and repeatable stereo capture. Sequential stems are only enabled when the profile confirms the required actions can be controlled reliably; otherwise the user is given an assisted capture workflow and an explicit limitation notice.

### Phase 4 — Cubase and Reason adapters

Build a shared VST3 bridge client that reconnects to the local service when the DAW project is reopened and persists the `.ubridge` session identity safely. Implement the host-capability adapter for Cubase and Reason, including supported import, routing, parameter, MIDI, and plug-in-state operations.

For each DAW, select the highest safe route in this order: direct documented creation/control; supported interchange import; or a generated, labelled import package/template. The bridge will keep the exact route visible in the session report. This preserves product trust while APIs and DAW versions differ.

### Phase 5 — synchronization, diagnostics, and release hardening

Add transport/MIDI clock, selected one-way and two-way control maps, revision-diff viewer, conflict-resolution choices, recovery from disconnect/reconnect, crash-safe journaling, and trace export. Establish a DAW/device/firmware compatibility database that is local by default and updated only with user permission.

Run end-to-end testing on the reference Windows machine and create release installers, code-signing plan, backup/restore instructions, privacy notice, accessibility baseline, documentation, and a support-ready diagnostic bundle that excludes musical content by default.

### Phase 6 — desktop platform expansion

Port the platform shell and backends to macOS and Linux while keeping the core and profile interfaces unchanged. Prioritize macOS next because it materially increases the DAW market and supports AU alongside VST3; prioritize Linux once audio/MIDI distribution, package formats, and exact DAW-host combinations have been qualified. Add CLAP where it adds practical host coverage. Do not position Linux or macOS as feature-equivalent until their device, driver, and host matrices pass the same regression suite.

### Phase 7 — tablet, mobile, and ChromeOS host modes

Deliver three distinct modes rather than a single misleading “mobile support” label: **desktop bridge**, **tablet/mobile bridge host**, and **companion control**. Start with a companion app for Android and iPadOS that can view sessions, diagnostics, mappings, backups, and remote transport/control over authenticated local IPC. Then add direct hardware-to-mobile-DAW integration only when the OS, DAW, connection method, and device profile negotiate adequate capability.

Android and ChromeOS should share the mobile core where possible; ChromeOS support will be qualified separately because USB/audio behavior differs by device and execution environment. iPadOS/iOS direct-host capability must be designed around the platform’s audio/MIDI, backgrounding, plug-in, and file-access rules rather than assumed from desktop behavior.

### Phase 8 — new devices, multi-device sessions, and profile SDK

Expand in the evidence-driven order documented in the brief: modern MPC family, SP-404, Elektron, then Maschine/Circuit and additional devices. Add a signed profile/adapter SDK only after the profile sandbox, permission model, test harness, migration system, and compatibility rules are mature. Multi-device graph routing, optional network transport, and community distribution follow this security foundation.

## Capability matrix and release governance

The bridge will maintain a testable four-dimensional matrix rather than a vague compatibility list:

| Dimension | Examples | Product behavior |
|---|---|---|
| Device | MPC Sample, MPC One, SP-404MKII, Digitakt | Determines project parsing, control, audio, and storage capabilities |
| Operating system | Windows, macOS, Linux, Android, ChromeOS, iPadOS/iOS | Determines device access, audio/MIDI backends, plug-in formats, permissions, and routing |
| DAW/host | Cubase, Reason, Ableton, mobile hosts | Determines import, plug-in, project, automation, and feedback capabilities |
| Connection | USB MIDI, USB audio, mass storage, analogue audio/MIDI, network | Determines actual transport and capture paths |

The effective feature set is calculated from these four inputs and shown before the bridge makes changes. A capability matrix entry may be **supported**, **supported with a controlled fallback**, **experimental**, **limited by the host/device**, or **unsupported**. This prevents a universal brand promise from becoming an inaccurate compatibility promise.

## Test strategy and release gates

Testing must be designed in from the first milestone because a bridge that silently shifts timing, loses samples, or overwrites a project is unsafe for music production.

| Test level | Method | Release gate |
|---|---|---|
| Unit and schema tests | Session round trips, migrations, hashing, conflict rules, parser edge cases, parameter transforms | No data loss or non-deterministic conversion in fixture corpus |
| Virtual-device tests | Recorded MIDI/audio/project replay, disconnect/reconnect, missing storage, corrupt project, capability changes | Device profile works without requiring manual hardware during regression |
| Audio/timing tests | Impulse loopback, calibration-repeatability, drift, stem alignment, silence/clip/tail detection | Alignment meets the documented calibrated tolerance for the tested path |
| DAW adapter tests | Fresh project, reopen, plug-in state restore, import package, transport, mapped control, failure fallback | Cubase and Reason deliver the stated route without corrupting sessions |
| Hardware-in-the-loop tests | User’s MPC Sample with Windows, Cubase, Reason, real audio/MIDI setup | End-to-end success for the agreed reference fixtures |
| Platform regression tests | Build/package/sign/install/uninstall and permissions on each qualified OS/device | Each public platform has its own released compatibility matrix entry |
| Safety and privacy tests | Read-only source intake, backup restore, cancellation, crash/restart, redacted diagnostics | Original hardware and DAW projects remain intact after all failure paths |

The MVP should not be labelled ready until it can complete a documented MPC Sample project transfer into both Cubase and Reason using the qualified route, preserve the tested MIDI and naming semantics, collect/flag assets correctly, validate captured audio, restore bridge identity after DAW reopen, and safely recover from cancellation or device interruption.

## Key risks and mitigations

| Risk | Impact | Mitigation |
|---|---|---|
| Proprietary or undocumented device/project formats | Parser coverage and write-back may be limited | Start read-only; use documented formats/behaviors where available; separate parser adapters; preserve originals; obtain explicit legal review before reverse engineering or distribution |
| DAW project-file and automation APIs vary or are unavailable | Fully automatic native project generation may not be possible | Validate early; use VST3/host APIs where documented; provide a complete interchange package and import template fallback |
| Stereo-only hardware and restricted device modes | No true simultaneous multitrack capture; limited live control | Capability-driven sequential capture, assisted workflows, calibrated alignment, and transparent limitation reporting |
| Real-time audio instability | Clicks, dropouts, crashes, loss of trust | Hard real-time thread boundaries, stress tests, bounded IPC, diagnostics, and safe bypass/fallback states |
| Cross-platform USB/audio differences | Delayed or uneven platform support | Keep platform layer thin, certify combinations independently, and stage releases behind the capability matrix |
| Two-way synchronization conflicts | Lost edits or damaged project state | Revision hashes, authority policies, previewable diffs, explicit resolution, journaled transactions, and immutable backups |
| Community profiles create security risks | Arbitrary file/device access could be abused | No community execution until signed profiles, permissions, validation, sandboxing boundaries, and automated profile tests exist |

## Assumptions and inputs needed during execution

This plan assumes the development reference environment is a **Windows x64 computer**, an **MPC Sample**, and installed licensed copies of **Cubase** and **Reason**. The bridge will be local-first and will not upload projects, samples, or diagnostic traces unless the user explicitly enables a future opt-in feature. The physical device, its firmware version, the audio/MIDI interface, cable/hub setup, and actual DAW editions will be recorded in the compatibility matrix before a particular feature is claimed.

The brief’s long-term platform ambition is retained, but **cross-platform architecture does not mean simultaneous feature parity**. The first executable deliverable is a Windows reference implementation. macOS, Linux, Android, ChromeOS, iPadOS, and iOS are architectural targets with staged, verified adapters and host modes. Direct Cubase/Reason session creation and deep bidirectional state sync are feasibility-gated; where an integration surface does not safely support it, the bridge delivers a complete, auditable exchange workflow instead.

## First execution backlog after approval

Upon approval, implementation should begin by converting this plan into tracked requirements and architecture decisions, then scaffolding the native repository and test harness. The first working slice will be deliberately narrow: Windows desktop application, MPC Sample project-folder preflight, versioned `.ubridge` session creation, asset inventory, MIDI/audio exchange bundle, and a human-readable capability/diagnostics report. That slice establishes the irreversible foundations—session model, capabilities, backups, and test fixtures—before device control, audio capture, DAW plug-ins, or additional platform targets are added.

**Primary reference:** user-supplied `UniversalBridge.txt`, especially its neutral session-model, capability-negotiation, virtual hardware laboratory, MPC Sample-first, Cubase/Reason, and cross-platform host requirements.

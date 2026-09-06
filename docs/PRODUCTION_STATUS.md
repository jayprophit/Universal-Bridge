# v0.5 production-foundation status

## Implemented and testable

- C++20 canonical session, capability negotiation, transaction journal, asset/archive planning, MIDI-route validation, timing/stem analysis, compatibility reporting, and read-only fixture exchange.
- Production-oriented CMake presets, warning policy, install rules, ZIP packaging, Windows/Linux CI, deterministic tests, and source-preservation checks.
- Dedicated Visual Studio 2026/MSVC Debug and Release presets, checked-in VS Code configuration, compile-command export for analysis tooling, and a portable static MSVC runtime policy for ZIP builds.
- Sync authorization guard requiring current revision vectors, no unresolved conflicts, explicit approval, and a verified backup.
- Windows read-only enumeration of the observed Akai MPC Sample identity (`VID 09E8`, `PID 205C`), grouping composite interfaces by container ID and recording interface number/service/name.
- A `ubridge devices` diagnostic command exposes that inventory while explicitly confirming that no interfaces were opened.
- Windows WinMM MIDI endpoint enumeration, input callbacks, bounded standard-message output, clean close, message semantics for note/control/pressure and Clock/Start/Continue/Stop/Song Position, and portable endpoint-role resolution.
- Windows WASAPI input enumeration and bounded receive-only signal probes. The Soundcraft AUX Optical → Audient Input 9/10 route has a recorded controlled comparison; this does not imply desk control.
- A partial read-only MPC XPJ importer for separately copied working files, including supported assets, pads, slices, tracks, sequences, songs and evidenced MIDI event fields. Source write-back is absent.
- In-memory hardware/DAW branch and merge planning with conflict resolution, section extraction and gated pad-assignment intent.
- Desired-versus-observed parameter state, bounded acknowledged-write lifecycle, independent clock-domain validation, recording destination/monitor/processing plans, and receive-only Hardware Learn contracts.
- A single-instance per-user service diagnostic executable. It inventories endpoints but does not yet own persistent sessions or expose IPC.
- Abstract-only Poietek ChatGPT Project intake with validated JSON/JSONL sources and an optional generated local SQLite research index. The index is not a runtime dependency.

## Experimental or scaffolding only

- MPC Sample discovery identifies profile candidates; it does not prove firmware, every interface purpose, parameter protocol support, or safe project/control writes. `MI_03` remains unknown unless a separate evidence record qualifies it.
- WinMM is an experimental Windows 10 fallback with legacy single-client ownership. Recorded MPC note/clock evidence applies only to the exact tested route; persistent routing, virtual DAW ports, Start/Stop relay, echo suppression and long-run timing remain unqualified.
- WASAPI streaming capture is not implemented: the current backend only enumerates inputs and runs bounded signal probes. Audio recording, monitoring, sample-accurate placement and simultaneous hardware/DAW capture remain unavailable.
- The XPJ importer is partial and version-specific. It does not preserve every vendor field, write projects, prove lossless reconstruction, or authorize reverse engineering beyond the cleared test material.
- State-mirror, acknowledgement, clock, recording and Hardware Learn code is a core contract/test slice. It does not prove live MPC parameter read/write, LED feedback, DAW automation or synchronization.
- Cubase and Reason support is an exchange workflow, not direct native project creation, plug-in hosting, or bidirectional control.
- Other devices, DAWs, operating systems, mobile companions, persistent authenticated IPC, VST3/AU/CLAP clients, hardware project write-back and total recall remain gated as documented in the requirements matrix.

## Release gates

An external release requires clean CI on supported toolchains, dependency/license review, signed reproducible packages, installer/uninstaller verification, fuzzing of every parser, threat modeling, privacy review, hardware-in-the-loop qualification, DAW reopen/recovery tests, crash-safe journal recovery, performance/dropout limits, accessibility review, documentation verification, and a truthful compatibility matrix tied to exact firmware/driver/host versions.

Until those gates pass, builds must be labelled **pre-release** and capabilities must retain their maturity status.

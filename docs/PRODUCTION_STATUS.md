# v0.5 production-foundation status

## Implemented and testable

- C++20 canonical session, capability negotiation, transaction journal, asset/archive planning, MIDI-route validation, timing/stem analysis, compatibility reporting, and read-only fixture exchange.
- Production-oriented CMake presets, warning policy, install rules, ZIP packaging, Windows/Linux CI, deterministic tests, and source-preservation checks.
- Dedicated Visual Studio 2026/MSVC Debug and Release presets, checked-in VS Code configuration, compile-command export for analysis tooling, and a portable static MSVC runtime policy for ZIP builds.
- Sync authorization guard requiring current revision vectors, no unresolved conflicts, explicit approval, and a verified backup.
- Windows read-only enumeration of the observed Akai MPC Sample identity (`VID 09E8`, `PID 205C`), grouping composite interfaces by container ID and recording interface number/service/name.
- A `ubridge devices` diagnostic command exposes that inventory while explicitly confirming that no interfaces were opened.

## Experimental or scaffolding only

- MPC Sample discovery identifies candidates; it does not prove model, firmware, endpoint purpose, protocol support, or safe control. The shared container and `MI_03` CDC-NCM observation are diagnostic evidence only. No interface is opened and no driver is installed.
- MIDI and audio backend interfaces are contracts without live implementations. Real-time capture, virtual ports, control, clock, and routing are not claimed.
- Cubase and Reason support is an exchange workflow, not direct native project creation, plug-in hosting, or bidirectional control.
- Other devices, DAWs, operating systems, mobile companions, proprietary project parsing, write-back, and total recall remain gated as documented in the requirements matrix.

## Release gates

An external release requires clean CI on supported toolchains, dependency/license review, signed reproducible packages, installer/uninstaller verification, fuzzing of every parser, threat modeling, privacy review, hardware-in-the-loop qualification, DAW reopen/recovery tests, crash-safe journal recovery, performance/dropout limits, accessibility review, documentation verification, and a truthful compatibility matrix tied to exact firmware/driver/host versions.

Until those gates pass, builds must be labelled **pre-release** and capabilities must retain their maturity status.

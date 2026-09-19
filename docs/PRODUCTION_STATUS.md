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
- A persistent per-user Windows service transport using a SID-derived named pipe, current-user-only ACL, DPAPI-protected random credential, bounded framed JSON, protocol version negotiation, atomic status persistence, sequential authenticated clients and clean/unclean restart accounting.
- A crash-recoverable canonical-session store with bounded versioned envelopes, validated pending writes, current/previous snapshots, corruption detection, safe identifiers and previous-revision recovery. Its FNV-1a check detects ordinary corruption; it is not cryptographic authentication.
- A fixed-capacity real-time buffer and MIDI mediation core with explicit route directions, clock/transport authorities, echo suppression, rate limits, queue saturation behavior and Start/Continue/Stop/Song Position state.
- Event-driven shared-mode WASAPI streaming capture into the fixed queue, native float/integer conversion, bounded startup, dropout/discontinuity counters and deterministic close.
- A qualified parameter-map validator and write-intent planner. It rejects observed-only, out-of-range, unacknowledged or unevidenced mappings and does not itself dispatch hardware/DAW writes.
- An optional audio-transparent VST3 component/controller. It exposes two automatable parameters, persists a versioned state, performs no IPC or operating-system work in the audio callback, and passed 47 Steinberg VST3 3.8.1 validator tests locally.
- A per-user WiX v4 MSI development route with one successful local install, installed-command run, repair and uninstall lifecycle. The generated artifact is intentionally unsigned.
- Machine-readable ten-gate release accounting, monthly dependency-update configuration, Linux ASan/UBSan CI, pinned-action CodeQL CI and deterministic concurrent queue/MIDI stress coverage.
- Abstract-only Poietek ChatGPT Project intake with validated JSON/JSONL sources and an optional generated local SQLite research index. The index is not a runtime dependency.

## Experimental or scaffolding only

- MPC Sample discovery identifies profile candidates; it does not prove firmware, every interface purpose, parameter protocol support, or safe project/control writes. `MI_03` remains unknown unless a separate evidence record qualifies it.
- WinMM is an experimental Windows 10 fallback with legacy single-client ownership. The mediation policy exists, but the service does not yet own the physical endpoint or expose a separately reviewed virtual DAW route. Recorded note/clock evidence applies only to the exact tested route; live Start/Stop/Continue/SPP relay, reconnect and long-run timing remain unqualified.
- WASAPI streaming capture is experimental. One silent ten-second MPC Sample run captured 442,764 frames and 885,528 samples with no queue drops, a clean stop and one startup discontinuity. Ring-buffered file transactions, monitoring, ASIO policy, latency calibration, sample-accurate placement, tail handling, format-change recovery and simultaneous hardware/DAW capture remain unavailable.
- The XPJ importer is partial and version-specific. It does not preserve every vendor field, write projects, prove lossless reconstruction, or authorize reverse engineering beyond the cleared test material.
- State-mirror, acknowledgement, clock, recording, Hardware Learn and qualified mapping code is a core contract/test slice. It does not prove live MPC parameter read/write, LED/display feedback, DAW automation or synchronization.
- Cubase and Reason support remains an exchange workflow. A validator-passing VST3 binary exists only when explicitly built with the external SDK; neither host has been certified and the client is not yet connected to service IPC.
- Local IPC is authenticated and persistent enough for diagnostics, but it is sequential, has no complete live-session protocol, and has not passed concurrent multi-host, forced-crash, endpoint-ownership or upgrade compatibility qualification.
- The MSI is unsigned development scaffolding. Code signing, secure update, version upgrade/rollback, SmartScreen reputation, malware review and clean-machine coverage are not complete.
- Other devices, DAWs, operating systems, mobile companions, AU/CLAP clients, hardware project write-back and total recall remain gated as documented in the requirements matrix.

## Release gates

An external release requires clean CI on supported toolchains, dependency/license review, signed reproducible packages, installer/uninstaller verification, fuzzing of every parser, threat modeling, privacy review, hardware-in-the-loop qualification, DAW reopen/recovery tests, crash-safe journal recovery, performance/dropout limits, accessibility review, documentation verification, and a truthful compatibility matrix tied to exact firmware/driver/host versions.

The authoritative gate ledger is [`../release/release-gates.json`](../release/release-gates.json), with a human-readable explanation in [`COMMERCIAL_RELEASE_GATES.md`](COMMERCIAL_RELEASE_GATES.md). Until all ten gates are `ready`, builds must be labelled **pre-release** and capabilities must retain their maturity status.

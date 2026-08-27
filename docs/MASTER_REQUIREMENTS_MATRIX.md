# Universal Bridge Master Requirements Matrix

**Source:** the supplied *Universal Hardware-to-DAW Bridge* research specification.  
**Purpose:** turn every documented product problem into a traceable, buildable requirement.  
**Status labels:** `Implemented` means present and tested in the current source; `Foundation` means its core contract is being implemented now; `Scaffold` means a buildable module/API/test seam will exist but no real-device claim is made; `Planned` means later implementation after the listed gate; `External` means manufacturer, host, or physical-device cooperation is necessary.

> No row may be marketed as complete merely because it has a code stub. A row becomes **Qualified** only when the exact device × OS × DAW × connection route passes the safety, transfer, timing, recovery, and regression gates.

## Engine key

| Engine | Buildable responsibility |
|---|---|
| E1 Device & Protocol | Device discovery, profile loading, USB/MIDI/audio/storage, capabilities, reconnects |
| E2 Session & Sync | Canonical session graph, revisions, diffs, authority, transactions, Total Recall |
| E3 Parsers & Conversion | Device/project parsers, schema mapping, legacy conversion, compatibility analysis |
| E4 Audio & Stem | Capture, render, validate, align, wet/dry, submix and capture-recovery workflows |
| E5 MIDI & Control | MIDI/performance model, routing, mappings, parameter normalization, controller profiles |
| E6 Timing & Transport | Calibration, clock, drift, placement, tempo/meter, multi-device timing |
| E7 Assets & Archive | Dependency inventory, relinking, collection, hashing, tagging, portable archive |
| E8 Mix, FX & Automation | Mixer graph, sends, FX intent, automation translation, gain and loudness analysis |
| E9 DAW & Host | Cubase/Reason and future DAW adapters, plug-in wrappers, import/direct host routes |
| E10 Diagnostics & Safety | Preflight, compatibility, trace, health, firmware/driver checks, recovery guidance |
| E11 Platform & IPC | Windows/macOS/Linux/mobile backends, local service, IPC, permissions, routing |
| E12 SDK & Test Lab | Virtual devices, fixtures, emulator, signed profile SDK, regression automation |
| E13 Workflow Orchestrator | User-facing “Finish in My DAW” plans, approval, execution, summary, rollback |

## Groups A–C: session, audio, MIDI, and performance

| ID | Requirement | Engine(s) | Delivery stage | Current coverage | Dependency / acceptance gate |
|---:|---|---|---|---|---|
| 1 | Automatic hardware project → DAW reconstruction | E2, E3, E9, E13 | MVP → V1 | Foundation | Parser and DAW route create a verified equivalent structure or an explicit fallback |
| 2 | Bidirectional hardware ↔ DAW project/state synchronization | E2, E5, E9, E11 | V1 | Foundation | Revision/authority model, tested host/device controls, conflict recovery |
| 3 | Automatically match/reopen the corresponding computer project | E2, E9, E13 | MVP | Foundation | Stable session identity, DAW persistence route, wrong-project prevention |
| 4 | Reconstruct tracks and stems | E2, E3, E4, E9 | MVP | Foundation | Source track/pad semantics parsed and output tracks remain named/aligned |
| 5 | Universal project parser/device profile architecture | E1, E3, E12 | Foundation → MVP | Scaffold | Profile contract and parser fixtures for every supported format |
| 6 | Preserve editable MIDI/performance data | E2, E5 | MVP | Foundation | Notes, velocity, duration, timing, and supported expressive data round-trip |
| 7 | Convert song/sequence structure to DAW arrangement | E2, E3, E9 | V1 | Scaffold | Arrangement parser plus qualified DAW timeline route |
| 8 | Incrementally synchronize only changes | E2, E7 | V1 | Foundation | Cryptographic/content revision vectors, diff preview, idempotent sync |
| 9 | Automatic timing, clock, and latency calibration | E4, E6 | V1 | Scaffold | Repeatable loopback measurements and documented alignment tolerance |
| 10 | Universal neutral session model | E2 | Foundation | Implemented partial | Versioned manifest exists; full track/pad/clip/mixer/FX graph is next |
| 11 | Individual pad → DAW track extraction | E3, E4, E9 | MVP | Scaffold | Device parser/control profile and output naming/alignment test |
| 12 | Automatic separated stem generation | E4, E13 | MVP | Foundation | Multichannel route or controlled sequential capture validated per device |
| 13 | Automated capture for stereo-output equipment | E1, E4, E5, E6, E13 | V1 | Scaffold | Device can safely solo/mute/control parts; no unsupported automation claim |
| 14 | Detect silent exported stems | E4, E10 | MVP | Scaffold | Audio analysis flags silence using configurable threshold and reports result |
| 15 | Detect missing stem exports | E2, E4, E10 | MVP | Foundation | Planned-track list reconciles with captured/exported assets |
| 16 | Normalize all stem lengths | E4 | MVP | Scaffold | Shared origin, tail policy, and rendered lengths documented in manifest |
| 17 | Correct start-point alignment | E4, E6 | MVP | Foundation | Measured offset and deterministic sample placement within declared tolerance |
| 18 | Preserve reverb/delay tails | E4, E8 | V1 | Scaffold | Capture/render tail policy is visible and testable |
| 19 | Export wet and dry versions | E4, E8 | V1 | Scaffold | Device capability route supports separate capture or emits limitation |
| 20 | Rebuild submix/bus relationships | E2, E8, E9 | V1 | Scaffold | Mixer/routing graph maps to host route or produces an import plan |
| 21 | Preserve note velocity | E2, E5 | MVP | Foundation | Fixture assertions compare note values exactly |
| 22 | Preserve microtiming | E2, E5, E6 | MVP | Foundation | Tick/sample timing retained or documented quantization fallback |
| 23 | Preserve swing/groove | E2, E5, E6 | V1 | Scaffold | Groove parameters or rendered timing preserves source intent |
| 24 | Preserve note duration | E2, E5 | MVP | Foundation | Note-off/duration preserved in interchange tests |
| 25 | Preserve CC automation | E2, E5, E8 | V1 | Scaffold | Mapping table and host automation destination validated |
| 26 | Preserve aftertouch/pressure | E2, E5 | V1 | Scaffold | Endpoint/host capability confirms support; otherwise retain as metadata |
| 27 | Preserve pitch bend | E2, E5 | V1 | Scaffold | Pitch range and events preserved or a visible fallback recorded |
| 28 | Translate manufacturer pad-note maps | E1, E5 | MVP | Scaffold | Profile mapping tests device pad identity to canonical pad identity |
| 29 | Automatically configure MIDI channels/routing | E1, E5, E9 | V1 | Scaffold | Collision-free route plan requires user approval before activation |
| 30 | MIDI Thru, merge, and split manager | E5, E10, E11 | V1 | Scaffold | Deterministic routing graph with loop/collision diagnostics |

## Groups D–F: controller sync, timing, device discovery, and drivers

| ID | Requirement | Engine(s) | Delivery stage | Current coverage | Dependency / acceptance gate |
|---:|---|---|---|---|---|
| 31 | Hardware knob → DAW parameter mapping | E1, E5, E9 | V1 | Scaffold | Documented host parameter route and profile mapping test |
| 32 | DAW parameter → hardware parameter mapping | E1, E2, E5, E9 | V1 | Scaffold | Hardware write authorization, range mapping, rollback, conflict test |
| 33 | Automatic controller profile selection | E1, E5, E12 | MVP | Scaffold | Device/firmware/DAW combination resolves a signed profile deterministically |
| 34 | Automatic mapping on DAW track selection | E5, E9 | V1 | Planned | Host track-focus API and non-disruptive fallback required |
| 35 | Pad-pressure mapping | E1, E5 | V1 | Scaffold | Device and host expose compatible pressure semantics |
| 36 | Fader mapping | E1, E5, E9 | V1 | Scaffold | Normalized absolute/relative control curve validated |
| 37 | Encoder/knob scaling normalization | E5 | Foundation → V1 | Scaffold | Canonical parameter curve tests avoid jumps and value loss |
| 38 | Translate NRPN/CC/device-specific messages | E1, E5 | V1 | Scaffold | Profile declares message semantics and error-safe unsupported fallback |
| 39 | User-created hardware profiles | E1, E5, E12 | V2 | Scaffold | Declarative profile validator, permissions, signing, test fixtures |
| 40 | Community device-profile SDK | E1, E12 | V2 | Scaffold | Sandbox/trust/review/distribution and virtual test lab required |
| 41 | Automatic round-trip latency calibration | E4, E6 | V1 | Scaffold | Loopback/calibration workflow produces repeatable result |
| 42 | Sample-accurate stem placement | E4, E6, E9 | V1 | Foundation | Measured timestamp/offset reaches route-specific documented tolerance |
| 43 | Detect timing drift | E4, E6, E10 | V1 | Scaffold | Long-session reference samples identify threshold breach |
| 44 | Correct long-session drift | E4, E6 | V2 | Planned | Correction strategy must preserve musical timing and be reversible |
| 45 | MIDI-clock jitter compensation | E5, E6 | V2 | Planned | Measured jitter model and user-configurable correction policy |
| 46 | Audio latency compensation | E4, E6 | V1 | Scaffold | Calibrated recording offset is stored in session provenance |
| 47 | Multi-device clock alignment | E1, E5, E6 | V2 | Scaffold | Master-clock selection and per-device clock reliability qualification |
| 48 | Play/stop/continue synchronization | E5, E6, E9 | V1 | Scaffold | Device/host transport support and safe event loop prevention |
| 49 | Tempo-change translation | E2, E5, E6, E9 | V1 | Scaffold | Tempo map conversion fixture and host import test |
| 50 | Time-signature compatibility checking | E2, E3, E6, E9 | MVP | Scaffold | Preflight identifies lossless, mapped, or unsupported meter path |
| 51 | Plug-and-play hardware discovery | E1, E10, E11 | V1 | Scaffold | Native device service finds/disambiguates real hardware on each platform |
| 52 | Automatic capability detection | E1, E10 | Foundation → V1 | Implemented profile-only | Real probe outcomes must supersede static profile assumptions |
| 53 | Reconnect after USB interruption | E1, E2, E10, E11 | V1 | Scaffold | Device identity and state recovery test after disconnect/reconnect |
| 54 | Driver conflict detection | E1, E10, E11 | V1 | Planned | Platform-specific diagnostics and least-privilege system inspection |
| 55 | Audio-interface conflict detection | E1, E4, E10, E11 | V1 | Scaffold | Backend claims/input contention visible with recovery advice |
| 56 | MIDI-device collision detection | E1, E5, E10, E11 | V1 | Scaffold | Route graph detects duplicate endpoint/feedback loop before start |
| 57 | Firmware/software version checking | E1, E10 | MVP | Scaffold | Profile compatibility database and transparent unknown-version state |
| 58 | USB bandwidth/hub diagnostics | E1, E10, E11 | V2 | Planned | Platform telemetry/probe needs per-driver reliability review |
| 59 | Buffer-size optimizer | E4, E6, E10, E11 | V2 | Planned | Advice only until platform/audio backend measurements are reliable |
| 60 | Unified connection diagnostics screen | E1, E4, E5, E6, E10, E11 | MVP | Implemented CLI report partial | Native desktop dashboard follows local service implementation |

## Groups G–I: assets, effects, compatibility, and MPC migration

| ID | Requirement | Engine(s) | Delivery stage | Current coverage | Dependency / acceptance gate |
|---:|---|---|---|---|---|
| 61 | Collect project dependencies | E3, E7 | MVP | Implemented partial | Recognized asset inventory/copy exists; parser references complete it |
| 62 | Repair missing samples | E3, E7, E10 | V1 | Scaffold | Candidate hash/path match requires approval and reversible relink plan |
| 63 | Relink sample paths | E3, E7 | V1 | Scaffold | Parser-specific reference writer only after format safety qualification |
| 64 | Search SD, USB, SSD, and computer simultaneously | E7, E10, E11 | V1 | Planned | Permission-aware indexed search; no unapproved removable-media writes |
| 65 | Detect duplicate samples | E7 | V1 | Scaffold | Cryptographic content hashes and user-visible duplicate groups |
| 66 | Unused-sample cleanup | E3, E7, E10 | V2 | Planned | Dependency graph proves safety; default is report-only |
| 67 | Automatic sample tagging | E7 | V2 | Planned | Opt-in classification/tags must preserve original names and be editable |
| 68 | BPM/key/type analysis | E4, E7 | V2 | Planned | Deterministic/opt-in analyzer; confidence and provenance visible |
| 69 | One-click collect-all project assets | E3, E7, E13 | MVP | Implemented partial | Existing copy bundle; parser-required assets and archive manifest next |
| 70 | Portable project archive format | E2, E7 | V1 | Foundation | Versioned archive with hashes, schema migrations, and extraction test |
| 71 | Mixer-state translation | E2, E8, E9 | V1 | Scaffold | Canonical volume/pan/mute/solo/routing graph maps or reports loss |
| 72 | Hardware automation → DAW automation | E2, E5, E8, E9 | V1 | Scaffold | Parser/control profile plus host-lane capability test |
| 73 | DAW automation → supported hardware automation | E1, E2, E5, E8, E9 | V2 | Planned | Hardware write permission, parameter semantics, rate limits, rollback |
| 74 | Preserve FX-chain state | E2, E8 | V1 | Scaffold | Canonical FX intent/parameter representation and host mapping table |
| 75 | Preserve send/return structure | E2, E8, E9 | V1 | Scaffold | Routing graph import or rendered fallback with manifest explanation |
| 76 | Preserve mute/solo automation | E2, E5, E8, E9 | V1 | Scaffold | Automation state translation and host action validation |
| 77 | Record live hardware FX gestures | E1, E5, E8 | V2 | Scaffold | Real control capture and rate-limited automation record route |
| 78 | Hardware FX → closest DAW equivalent | E8, E9 | V2 | Scaffold | Opt-in mapping table; never hide non-equivalence; wet render fallback |
| 79 | Automatic gain staging | E4, E8 | V2 | Planned | Non-destructive recommendations and user approval |
| 80 | Headroom, clipping, and loudness warnings | E4, E8, E10 | V1 | Scaffold | Audio analysis thresholds, reproducible warnings, no silent normalization |
| 81 | MPC 2 → MPC 3 project analyzer | E3, E10 | V1 | Scaffold | Legally cleared parser fixtures and version capability table |
| 82 | MPC 3 → older MPC compatibility analyzer | E3, E10 | V1 | Scaffold | Target-profile feature comparison and report; no unsafe conversion |
| 83 | Safe duplicate before conversion | E2, E3, E10, E13 | MVP | Implemented general backup partial | Format-specific conversion transaction and restore test |
| 84 | Program/track-architecture converter | E2, E3 | V2 | Scaffold | Explicit source/target semantics and deterministic conversion fixtures |
| 85 | Render unsupported features automatically | E3, E4, E8, E13 | V2 | Planned | User-selected dry/wet render plan and loss report |
| 86 | Preserve legacy sequence structure | E2, E3, E5 | V1 | Scaffold | Legacy parser fixtures and canonical arrangement representation |
| 87 | Legacy MPC project importer | E1, E3, E7 | V2 | Scaffold | Isolated parser per model/storage format and fixture corpus |
| 88 | Cross-MPC-model compatibility report | E1, E3, E10 | V1 | Scaffold | Version/device matrix and transparent estimated coverage rules |
| 89 | Plug-in dependency scanner | E3, E7, E10 | V1 | Scaffold | Parser/host metadata source and exact unknown-state reporting |
| 90 | Freeze/render computer-only plug-ins for standalone transfer | E4, E7, E8, E9, E13 | V2 | Planned | Host render capability, asset archive, and user-approved lossy action |

## Group J: MPC Sample workflow and universal product outcome

| ID | Requirement | Engine(s) | Delivery stage | Current coverage | Dependency / acceptance gate |
|---:|---|---|---|---|---|
| 91 | MPC Sample project → DAW reconstruction | E1, E2, E3, E4, E9, E13 | MVP | Foundation | MPC Sample parser plus Cubase/Reason qualified creation/import route |
| 92 | Automated MPC Sample per-pad stem capture | E1, E4, E5, E6, E13 | V1 | Scaffold | Device accepts safe control plan; capture/timing/recovery gate |
| 93 | MPC Sample project matching / Total Recall | E2, E3, E9, E13 | MVP | Foundation | Persistent session IDs, fingerprints, host persistence, collision defense |
| 94 | MPC Sample DAW controller mapping layer | E1, E5, E9 | V1 | Scaffold | Explicit control profile, host capabilities, feedback-loop tests |
| 95 | Reduce SD-access workflow interruptions where possible | E1, E3, E7, E13 | V1 | Scaffold | Device behavior must be verified; no bypass of firmware restrictions |
| 96 | Automatic MPC Sample project backup | E2, E7, E10, E13 | MVP | Implemented partial | Current external folder backup; device/project-aware archive next |
| 97 | MPC Sample sequence/automation → editable DAW data | E2, E3, E5, E8, E9 | V1 | Scaffold | Cleared parser plus target host mapping/limitation report |
| 98 | MPC Sample automatic project health check | E3, E7, E10 | MVP | Implemented partial | Current inventory warnings; parser-level semantic validation next |
| 99 | “Finish This MPC Project in My DAW” workflow | E1–E13 | MVP | Foundation | Orchestrated preflight/approval/execution/verification/summary sequence |
| 100 | Remove the conceptual hardware/computer boundary | E1–E13 | Product vision | Foundation | Broad qualified adapter coverage; not a discrete feature claim |

## Coverage summary

| Delivery position | Requirement IDs | Meaning |
|---|---|---|
| **Implemented partial** | 10, 52, 60, 61, 69, 83, 96, 98 | Safe proof-of-foundation exists in v0.2.0; semantic/device/DAW completeness remains |
| **Foundation** | 1–9, 42, 70, 91, 93, 99, 100 | Canonical contracts and current code are being extended to support the eventual feature |
| **Scaffold / build next** | Most V1/V2 items | Concrete interface, test seam, profile/schema, or module will be included in the buildable codebase |
| **External capability gate** | Device/DAW write-back, driver control, proprietary parsing, audio capture, host project creation | Can be implemented only after the real device/host route and legal/technical surface are verified |

The matrix covers every numbered requirement in the supplied document. The implementation goal is not to turn all 100 rows on at once, but to build the reusable engines and proofs that let each row become a qualified, independently testable capability.

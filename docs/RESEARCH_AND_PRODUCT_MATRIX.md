# Universal Bridge Research and Product Matrix

This matrix adds the product-research fields requested by the supplied merged 100-problem specification without replacing the engineering acceptance gates in [MASTER_REQUIREMENTS_MATRIX.md](MASTER_REQUIREMENTS_MATRIX.md). IDs and requirement names remain identical across both files.

The classifications below are planning judgments, not capability claims:

- **Lineage:** BOTH appeared in both universal and MPC research; MPC is MPC-specific; UNIVERSAL came from the broader hardware audit.
- **Research priority:** preserves the supplied critical/high/useful classification.
- **Difficulty:** current engineering estimate for a qualified implementation, not for an interface stub.
- **Akai/manufacturer cooperation:** No for foundation means standards-based or offline foundations can be built independently; Helpful means official documentation/fixtures materially reduce risk; Route-dependent means direct, proprietary, write-capable, or host-specific behavior cannot be promised without a verified legal and technical route. It does not assert that a commercial partnership is always legally mandatory.
- **Revenue value:** relative product importance from the supplied research, not a financial forecast.
- **Competitive coverage:** research-era positioning that must be refreshed from primary sources before publication or investment use.
- **E14 Host Capability & Routing** is added as a cross-cutting engine. E13 was already assigned to the Workflow Orchestrator in the checked-in engineering matrix, so adding E14 preserves all stable engine IDs instead of silently renumbering earlier requirements.

| ID | Requirement | Lineage | Research priority | Stage | Engine(s) | Difficulty | Akai/manufacturer cooperation | Revenue value | Competitive coverage |
|---:|---|---|---|---|---|---|---|---|---|
| 1 | Automatic hardware project → DAW reconstruction | BOTH | Critical | MVP → V1 | E2, E3, E9, E13, E14 | High | Helpful | Core | Fragmented/partial |
| 2 | Bidirectional hardware ↔ DAW project/state synchronization | BOTH | Critical | V1 | E2, E5, E9, E11, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 3 | Automatically match/reopen the corresponding computer project | BOTH | Critical | MVP | E2, E9, E13, E14 | High | Route-dependent | Core | Fragmented/partial |
| 4 | Reconstruct tracks and stems | BOTH | Critical | MVP | E2, E3, E4, E9, E14 | High | Route-dependent | Core | Fragmented/partial |
| 5 | Universal project parser/device profile architecture | BOTH | Critical | Foundation → MVP | E1, E3, E12 | High | Helpful | Core | Fragmented/partial |
| 6 | Preserve editable MIDI/performance data | BOTH | Critical | MVP | E2, E5 | High | No for foundation | Core | Fragmented/partial |
| 7 | Convert song/sequence structure to DAW arrangement | BOTH | Critical | V1 | E2, E3, E9, E14 | High | Route-dependent | Core | Fragmented/partial |
| 8 | Incrementally synchronize only changes | BOTH | Critical | V1 | E2, E7 | Very high | No for foundation | Core | Fragmented/partial |
| 9 | Automatic timing, clock, and latency calibration | BOTH | Critical | V1 | E4, E6 | Very high | No for foundation | Core | Fragmented/partial |
| 10 | Universal neutral session model | BOTH | Critical | Foundation | E2 | Medium | No for foundation | Core | Fragmented/partial |
| 11 | Individual pad → DAW track extraction | BOTH | Critical | MVP | E3, E4, E9, E14 | High | Route-dependent | Core | Fragmented/partial |
| 12 | Automatic separated stem generation | BOTH | Critical | MVP | E4, E13, E14 | High | Route-dependent | Core | Fragmented/partial |
| 13 | Automated capture for stereo-output equipment | BOTH | Critical | V1 | E1, E4, E5, E6, E13, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 14 | Detect silent exported stems | MPC | High | MVP | E4, E10 | Medium | No for foundation | High | Akai/third-party partial |
| 15 | Detect missing stem exports | MPC | High | MVP | E2, E4, E10 | Medium | No for foundation | High | Akai/third-party partial |
| 16 | Normalize all stem lengths | MPC | High | MVP | E4 | Medium | No for foundation | High | Akai/third-party partial |
| 17 | Correct start-point alignment | BOTH | High | MVP | E4, E6 | High | Helpful | High | Fragmented/partial |
| 18 | Preserve reverb/delay tails | BOTH | High | V1 | E4, E8 | Medium | No for foundation | High | Fragmented/partial |
| 19 | Export wet and dry versions | MPC | High | V1 | E4, E8 | High | Helpful | High | Akai/third-party partial |
| 20 | Rebuild submix/bus relationships | BOTH | High | V1 | E2, E8, E9, E14 | High | Helpful | High | Fragmented/partial |
| 21 | Preserve note velocity | BOTH | Critical | MVP | E2, E5 | Medium | No for foundation | Core | Fragmented/partial |
| 22 | Preserve microtiming | BOTH | Critical | MVP | E2, E5, E6 | High | No for foundation | Core | Fragmented/partial |
| 23 | Preserve swing/groove | BOTH | Critical | V1 | E2, E5, E6 | High | Helpful | Core | Fragmented/partial |
| 24 | Preserve note duration | MPC | High | MVP | E2, E5 | Medium | No for foundation | High | Akai/third-party partial |
| 25 | Preserve CC automation | BOTH | High | V1 | E2, E5, E8 | High | Helpful | High | Fragmented/partial |
| 26 | Preserve aftertouch/pressure | MPC | High | V1 | E2, E5 | Medium | Helpful | High | Akai/third-party partial |
| 27 | Preserve pitch bend | MPC | High | V1 | E2, E5 | Medium | Helpful | High | Akai/third-party partial |
| 28 | Translate manufacturer pad-note maps | BOTH | High | MVP | E1, E5 | Medium | Helpful | High | Fragmented/partial |
| 29 | Automatically configure MIDI channels/routing | BOTH | Critical | V1 | E1, E5, E9, E14 | High | Helpful | Core | Fragmented/partial |
| 30 | MIDI Thru, merge, and split manager | BOTH | High | V1 | E5, E10, E11, E14 | High | Helpful | High | Fragmented/partial |
| 31 | Hardware knob → DAW parameter mapping | BOTH | Critical | V1 | E1, E5, E9, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 32 | DAW parameter → hardware parameter mapping | BOTH | Critical | V1 | E1, E2, E5, E9, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 33 | Automatic controller profile selection | BOTH | Critical | MVP | E1, E5, E12, E14 | High | Helpful | Core | Fragmented/partial |
| 34 | Automatic mapping on DAW track selection | MPC | Critical | V1 | E5, E9, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 35 | Pad-pressure mapping | MPC | High | V1 | E1, E5, E14 | Medium | Helpful | High | Akai/third-party partial |
| 36 | Fader mapping | UNIVERSAL | High | V1 | E1, E5, E9, E14 | Medium | Helpful | High | Manufacturer-specific |
| 37 | Encoder/knob scaling normalization | UNIVERSAL | High | Foundation → V1 | E5, E14 | Medium | Helpful | High | Manufacturer-specific |
| 38 | Translate NRPN/CC/device-specific messages | UNIVERSAL | High | V1 | E1, E5, E14 | Medium | Helpful | High | Manufacturer-specific |
| 39 | User-created hardware profiles | BOTH | Critical | V2 | E1, E5, E12 | Very high | Route-dependent | Core | Fragmented/partial |
| 40 | Community device-profile SDK | UNIVERSAL | Critical | V2 | E1, E12 | Very high | Route-dependent | Core | Manufacturer-specific |
| 41 | Automatic round-trip latency calibration | BOTH | Critical | V1 | E4, E6, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 42 | Sample-accurate stem placement | BOTH | Critical | V1 | E4, E6, E9, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 43 | Detect timing drift | BOTH | Critical | V1 | E4, E6, E10, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 44 | Correct long-session drift | BOTH | Critical | V2 | E4, E6, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 45 | MIDI-clock jitter compensation | BOTH | High | V2 | E5, E6, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 46 | Audio latency compensation | MPC | High | V1 | E4, E6, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 47 | Multi-device clock alignment | UNIVERSAL | Critical | V2 | E1, E5, E6, E14 | Very high | Route-dependent | Core | Manufacturer-specific |
| 48 | Play/stop/continue synchronization | BOTH | High | V1 | E5, E6, E9, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 49 | Tempo-change translation | MPC | High | V1 | E2, E5, E6, E9, E14 | High | Helpful | High | Akai/third-party partial |
| 50 | Time-signature compatibility checking | MPC | High | MVP | E2, E3, E6, E9, E14 | Medium | Helpful | High | Akai/third-party partial |
| 51 | Plug-and-play hardware discovery | BOTH | Critical | V1 | E1, E10, E11, E14 | Medium | No for foundation | Core | Fragmented/partial |
| 52 | Automatic capability detection | BOTH | Critical | Foundation → V1 | E1, E10, E14 | Medium | Helpful | Core | Fragmented/partial |
| 53 | Reconnect after USB interruption | BOTH | High | V1 | E1, E2, E10, E11, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 54 | Driver conflict detection | BOTH | High | V1 | E1, E10, E11, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 55 | Audio-interface conflict detection | MPC | High | V1 | E1, E4, E10, E11, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 56 | MIDI-device collision detection | UNIVERSAL | High | V1 | E1, E5, E10, E11, E14 | Very high | Route-dependent | High | Manufacturer-specific |
| 57 | Firmware/software version checking | BOTH | High | MVP | E1, E10, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 58 | USB bandwidth/hub diagnostics | UNIVERSAL | High | V2 | E1, E10, E11, E14 | Very high | Route-dependent | High | Manufacturer-specific |
| 59 | Buffer-size optimizer | MPC | High | V2 | E4, E6, E10, E11, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 60 | Unified connection diagnostics screen | BOTH | Critical | MVP | E1, E4, E5, E6, E10, E11, E14 | Medium | No for foundation | Core | Fragmented/partial |
| 61 | Collect project dependencies | BOTH | Critical | MVP | E3, E7 | Medium | Helpful | Core | Fragmented/partial |
| 62 | Repair missing samples | BOTH | Critical | V1 | E3, E7, E10 | Medium | Helpful | Core | Fragmented/partial |
| 63 | Relink sample paths | BOTH | High | V1 | E3, E7 | Medium | Helpful | High | Fragmented/partial |
| 64 | Search SD, USB, SSD, and computer simultaneously | MPC | High | V1 | E7, E10, E11 | High | Helpful | High | Akai/third-party partial |
| 65 | Detect duplicate samples | BOTH | High | V1 | E7 | Medium | No for foundation | High | Fragmented/partial |
| 66 | Unused-sample cleanup | MPC | High | V2 | E3, E7, E10 | Medium | No for foundation | High | Akai/third-party partial |
| 67 | Automatic sample tagging | BOTH | Useful | V2 | E7 | Medium | No for foundation | Supporting | Fragmented/partial |
| 68 | BPM/key/type analysis | UNIVERSAL | Useful | V2 | E4, E7 | Medium | No for foundation | Supporting | Manufacturer-specific |
| 69 | One-click collect-all project assets | MPC | High | MVP | E3, E7, E13 | Medium | Helpful | High | Akai/third-party partial |
| 70 | Portable project archive format | BOTH | Critical | V1 | E2, E7 | Medium | Helpful | Core | Fragmented/partial |
| 71 | Mixer-state translation | BOTH | Critical | V1 | E2, E8, E9, E14 | High | Helpful | Core | Fragmented/partial |
| 72 | Hardware automation → DAW automation | BOTH | Critical | V1 | E2, E5, E8, E9, E14 | Very high | Route-dependent | Core | Fragmented/partial |
| 73 | DAW automation → supported hardware automation | MPC | High | V2 | E1, E2, E5, E8, E9, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 74 | Preserve FX-chain state | BOTH | High | V1 | E2, E8, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 75 | Preserve send/return structure | BOTH | High | V1 | E2, E8, E9, E14 | Very high | Route-dependent | High | Fragmented/partial |
| 76 | Preserve mute/solo automation | MPC | High | V1 | E2, E5, E8, E9, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 77 | Record live hardware FX gestures | MPC | High | V2 | E1, E5, E8, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 78 | Hardware FX → closest DAW equivalent | MPC | High | V2 | E8, E9, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 79 | Automatic gain staging | MPC | High | V2 | E4, E8, E14 | Medium | No for foundation | High | Akai/third-party partial |
| 80 | Headroom, clipping, and loudness warnings | MPC | High | V1 | E4, E8, E10, E14 | Medium | Helpful | High | Akai/third-party partial |
| 81 | MPC 2 → MPC 3 project analyzer | MPC | Critical | V1 | E3, E10 | Very high | Helpful | Core | Akai/third-party partial |
| 82 | MPC 3 → older MPC compatibility analyzer | MPC | Critical | V1 | E3, E10 | Very high | Helpful | Core | Akai/third-party partial |
| 83 | Safe duplicate before conversion | MPC | Critical | MVP | E2, E3, E10, E13 | Medium | Helpful | Core | Akai/third-party partial |
| 84 | Program/track-architecture converter | MPC | High | V2 | E2, E3 | Very high | Route-dependent | High | Akai/third-party partial |
| 85 | Render unsupported features automatically | MPC | High | V2 | E3, E4, E8, E13 | Very high | Route-dependent | High | Akai/third-party partial |
| 86 | Preserve legacy sequence structure | MPC | High | V1 | E2, E3, E5 | Very high | Route-dependent | High | Akai/third-party partial |
| 87 | Legacy MPC project importer | MPC | High | V2 | E1, E3, E7 | Very high | Route-dependent | High | Akai/third-party partial |
| 88 | Cross-MPC-model compatibility report | MPC | Critical | V1 | E1, E3, E10 | Very high | Route-dependent | Core | Akai/third-party partial |
| 89 | Plug-in dependency scanner | MPC | Critical | V1 | E3, E7, E10 | Very high | Route-dependent | Core | Akai/third-party partial |
| 90 | Freeze/render computer-only plug-ins for standalone transfer | MPC | Critical | V2 | E4, E7, E8, E9, E13 | Very high | Route-dependent | Core | Akai/third-party partial |
| 91 | MPC Sample project → DAW reconstruction | MPC | Critical | MVP | E1, E2, E3, E4, E9, E13, E14 | Very high | Route-dependent | Core | Akai/third-party partial |
| 92 | Automated MPC Sample per-pad stem capture | MPC | Critical | V1 | E1, E4, E5, E6, E13, E14 | Very high | Route-dependent | Core | Akai/third-party partial |
| 93 | MPC Sample project matching / Total Recall | MPC | Critical | MVP | E2, E3, E9, E13, E14 | Very high | Route-dependent | Core | Akai/third-party partial |
| 94 | MPC Sample DAW controller mapping layer | MPC | Critical | V1 | E1, E5, E9, E14 | Very high | Route-dependent | Core | Akai/third-party partial |
| 95 | Reduce SD-access workflow interruptions where possible | MPC | High | V1 | E1, E3, E7, E13, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 96 | Automatic MPC Sample project backup | MPC | High | MVP | E2, E7, E10, E13, E14 | Medium | Helpful | High | Akai/third-party partial |
| 97 | MPC Sample sequence/automation → editable DAW data | MPC | High | V1 | E2, E3, E5, E8, E9, E14 | Very high | Route-dependent | High | Akai/third-party partial |
| 98 | MPC Sample automatic project health check | MPC | High | MVP | E3, E7, E10, E14 | Medium | Helpful | High | Akai/third-party partial |
| 99 | “Finish This MPC Project in My DAW” workflow | MPC | Critical | MVP | E1–E14 | Very high | Route-dependent | Core | Akai/third-party partial |
| 100 | Remove the conceptual hardware/computer boundary | BOTH | Critical | Product vision | E1–E14 | Very high | Route-dependent | Core | Fragmented/partial |

## Governance

A research row becomes an implementation commitment only when it is scheduled in the delivery roadmap. A coded route becomes **Qualified** only after the corresponding device × OS × DAW × connection entry passes the identity, safety, transfer, timing, reopen, failure, regression, and documentation gates. Competitor observations must be rechecked from primary sources before they are quoted outside internal product planning.


# v0.4 Safely Implemented Foundation

Version 0.4 extends the Universal Bridge from a capability-aware exchange prototype into a **testable offline session-intelligence foundation**. It implements the parts of the specification that can be built deterministically without live hardware, proprietary project-file writes, unverified USB/audio/MIDI drivers, or a connected DAW.

## Implemented components

| Component | Practical result | Requirements advanced |
|---|---|---|
| Full canonical session graph | Models tracks, pads, clips, arrangement regions, MIDI events, mixer channels, FX intent, automation curves, routing, tempo, meter, assets, revision, and provenance | 1–10, 21–27, 71–78 |
| Session validation | Detects missing/duplicate identities, orphan clips, invalid durations/meter/tempo, pad-map ambiguity, routing issues, and incomplete automation | 1, 5–7, 10, 28–30, 50, 71–78 |
| Asset health analysis | Detects missing required assets, same-content duplicates, possibly unused assets, size totals, and safe recovery recommendations | 15, 61–69, 89, 98 |
| Portable archive plan | Builds a versioned, hash-preserving asset manifest and blocks packaging while required assets are incomplete | 61–70, 83, 96 |
| Timing and drift calculations | Calculates supplied-marker round-trip latency, proposed sample correction, and long-session drift warnings without pretending to have measured a physical route | 9, 17, 41–47 |
| Stem validation | Validates take completion, silence, clipping risk, start/length deviations, and tail metadata from supplied capture metrics | 11–20, 42, 80, 92 |
| MIDI validation and routing | Validates MIDI event ranges, endpoints, directions, route collisions, and feedback loops; models Thru/merge/split constraints | 6, 21–40, 48–49, 94 |
| Controller scaling | Implements linear, bipolar, and logarithmic normalized-parameter conversion | 31–38 |
| Mixer and automation planning | Evaluates mixer/routing completeness and creates explicit editable-automation or render-fallback plans | 18–20, 71–80, 97 |
| Profile validation | Enforces stable profile identity/schema, prevents unsigned activation of physical capabilities, and flags unqualified platform claims | 5, 33, 39–40, 51–60 |
| Compatibility report | Produces an auditable route score with availability/gated status and limitations | 19, 52, 57, 60, 81–90 |
| Workflow/archive/report serialization | Produces machine-readable workflow, archive, and compatibility JSON records | 2–3, 8, 16, 70, 99 |
| Safe local-service state model | Owns safe-mode startup and creates only approval-gated transactions; it never opens a hardware endpoint in v0.4 | 20, 51–60, 93, 99 |
| Virtual hardware laboratory | Tests device/DAW/OS profile registries, virtual events, disconnect/reconnect, safety boundaries, core models, and planners | 5, 12, 19–20, 31–60, 91–100 |

## Verified behavior

The `ubridge_test_lab` executable now performs deterministic tests for session integrity, asset health, archive readiness, timing and drift calculations, stem safety, MIDI route loops, controller scaling, automation fallback, mixer topology, signed profile policy, compatibility output, workflow/archive/report serialization, service safe mode, and virtual-device reconnect behavior. Existing command-line tests also retain source-project preservation, output-path safety, Cubase/Reason exchange generation, and every declared OS profile.

> A calculation, profile, plan, or virtual-lab pass does **not** prove that any physical device, driver, DAW, plug-in, or proprietary project format has been controlled successfully. The output clearly distinguishes `available`, `gated`, and `simulated` work.

## What remains gated

| Gated capability | Why it cannot be safely enabled in this release |
|---|---|
| MPC Sample parser | Needs an authorized real-project corpus and a formally reviewed read-only parsing route |
| Live device discovery/MIDI/audio | Needs native platform services, actual hardware, driver/endpoints, and reconnect/error testing |
| Per-pad capture/stems | Needs validated control semantics, audio input, loopback calibration, and real capture takes |
| Cubase/Reason reconstruction | Needs a qualified documented import, host API, or plug-in route plus version-specific QA |
| Two-way sync/write-back | Needs user approval, conflict UI, snapshots, confirmed hardware/DAW write semantics, and recovery testing |
| Native macOS/Linux/mobile products | Need platform app/backend projects, permissions, lifecycle handling, packaging, and qualifying hardware/host test matrices |

The exact route from this foundation to a qualified release is maintained in [`DELIVERY_ROADMAP_AND_GATES.md`](DELIVERY_ROADMAP_AND_GATES.md).

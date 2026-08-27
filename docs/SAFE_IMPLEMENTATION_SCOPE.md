# Safe Implementation Scope: v0.4 Foundation

This release implements every remaining component that can be built and verified without touching a user’s hardware, writing proprietary device/DAW project files, assuming undocumented protocols, or claiming a mobile/desktop driver backend exists.

## Components implemented now

| Component | Requirements advanced | What is safe to implement now |
|---|---|---|
| Canonical session graph | 1–10, 21–27, 71–78 | Tracks, pads, clips, arrangement sections, MIDI performance, mixer, sends, FX intent, automation curves, routing, provenance, schema/revision state |
| Project health and assets | 15, 61–70, 81–90, 96, 98 | Content fingerprints, duplicate/missing/unused reporting, sample-path candidate ranking, archive manifests, dependency collection plan, compatibility report schemas |
| Timing and audio validation | 9, 11–20, 41–50, 92 | Loopback calculation, timing/drift records, stem/take analysis from supplied audio statistics, silence/clip/length/tail checks, capture-plan rules |
| MIDI and control planning | 6, 21–40, 94 | MIDI event validation, channel/route graph validation, pad map normalization, control scaling, feedback-loop checks, controller-profile contracts |
| Session comparison and recovery | 2, 3, 8, 14, 16, 93, 99 | Revisions, change sets, conflict classification, transaction/approval state, restore plans, workflow serialization |
| Profiles and compatibility | 5, 12, 31–40, 51–60, 81–100 | Signed-profile model, schema checks, capability matrices, safe fallback selection, report generation, test fixtures |
| Local service/mobile companion | 19, 20, 51–60, cross-platform sections | Service state model, IPC message schema, pairing/session-state contracts, permissions/capability boundaries, companion synchronization plan |

## Components intentionally not enabled

| Component | Why it remains gated | Required evidence before activation |
|---|---|---|
| Real MPC Sample project parsing | No verified formal project-format contract in this build | Authorized corpus, read-only parser, malformed-file fuzzing, semantic comparison, source-preservation results |
| Device discovery and live USB/driver use | Cannot safely claim endpoint behavior from a sandboxed build | Platform backend implementation, physical hardware tests, reconnect/error/recovery logs |
| Audio capture or calibration measurement | No user audio interface/loopback in this environment | Driver backend, calibrated test rig, repeatable timing measurements, no-clipping/take-recovery checks |
| MIDI routing/control | No verified physical endpoints or DAW host session | Virtual-port backend, feedback/collision tests, exact device/host mappings, consent before control |
| Cubase/Reason project creation/control | Must not write undocumented proprietary project data | Documented host route or qualified plug-in/import path, version matrix, rollback/reopen test |
| Direct host/plugin clients | VST3/AU/CLAP/AAX/Rack Extension implementations are separate licensed/distribution routes | Applicable SDK/licensing review, real-time safety architecture, host QA, code-signing/distribution process |
| Hardware write-back and two-way sync | Destructive without verified control semantics and recovery | Explicit authority, snapshots, transaction engine, device write policy, user approval, failure recovery |
| Native mobile apps | C++ contracts do not constitute iOS/Android/ChromeOS executables | Native app project, permission/lifecycle handling, mobile hardware/DAW tests, store/distribution review |

> The implementation uses deterministic planners, analyzers, validators, and virtual devices. A simulated output is always labelled as simulated; it is never presented as a successful physical device or DAW action.

## Acceptance policy

An unqualified feature may still create a **plan**, **compatibility report**, **manifest**, **diagnostic**, or **fixture result**. It may not create a device/DAW write operation or claim a successful physical capture/control/synchronization event. All such outputs preserve the exact prerequisite needed to move the feature into a qualified release.

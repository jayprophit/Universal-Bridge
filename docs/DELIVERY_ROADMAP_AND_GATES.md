# Universal Bridge Delivery Roadmap and Release Gates

This roadmap converts the full requirements matrix into executable releases. The order protects the user’s music first: the product becomes better at understanding and preserving sessions before it is allowed to command hardware, capture audio, or write host-specific state.

> **Build order:** read safely → model faithfully → exchange transparently → control cautiously → synchronize reversibly → expand only through qualification.

## Release sequence

| Release | Product outcome | Engines emphasized | Exit gate |
|---|---|---|---|
| v0.3 — Buildable foundation | Traceable 100-requirement program, core contracts, module registry, local service safe mode, virtual test lab | E1, E2, E10, E11, E12, E13 | All source targets compile; test lab confirms conservative defaults and transaction lifecycle |
| v0.4 — MPC Sample read-only intelligence | Parsed MPC Sample project representation, dependency graph, project health report, archive manifest | E2, E3, E7, E10 | Golden authorized projects parse without source changes; unsupported fields reported |
| v0.5 — Windows I/O proof | Native Windows device-service prototype, MIDI endpoint discovery, audio endpoint inventory, diagnostics trace bundle | E1, E5, E10, E11 | Reference MPC/Windows combinations pass permission, reconnect, and no-feedback-loop tests |
| v0.6 — Cubase and Reason hand-off | Qualified import/plug-in feasibility results, reliable session persistence, track/package reconstruction route | E2, E4, E5, E8, E9, E13 | Each DAW route is labelled direct, host-assisted, or exchange-only and passes reopen/rollback tests |
| v0.7 — Capture and timing | Calibrated audio capture, silence/tail detection, placement metadata, assisted/sequential capture if qualified | E4, E6, E10, E13 | Loopback and real-project timing results meet published route-specific tolerances |
| v0.8 — Controlled mapping | Selected MPC parameter maps, MIDI routing, transport, profile selection, mapping diagnostics | E1, E5, E9, E10 | No feedback loops; value scaling and device reconnection tests pass |
| v0.9 — Sync preview | Diffs, revision vectors, conflict viewer, user approval, recovery journal; still limited write actions | E2, E7, E10, E13 | Offline conflict, cancellation, and crash-recovery fixtures never silently lose state |
| v1.0 — Qualified reference bridge | User-approved hardware/DAW operations for the exact certified MPC Sample → Windows → Cubase/Reason routes | E1–E13 | End-to-end acceptance set passes on real hardware and current target host versions |
| v1.x — Adapter expansion | Modern MPC, SP-404, Elektron, Maschine, Circuit, and additional DAW/platform routes | E1, E3–E13 | Every new combination passes independent capability-matrix gates |
| v2.x — Ecosystem expansion | macOS/Linux desktop, Android/ChromeOS/iPadOS/iOS companion/direct host paths, profile SDK, multi-device sessions | E1, E5, E6, E9, E11, E12 | Signed profiles, permissions, mobile lifecycle, and multi-device conflict/timing tests pass |

## Build order by engine

| Engine | First code milestone | Evidence needed before user-facing enablement |
|---|---|---|
| E1 Device & Protocol | Profile registry, USB/MIDI/audio endpoint abstractions, device identities | Exact hardware/firmware probe, reconnect behavior, no unsupported protocol use |
| E2 Session & Sync | Canonical session graph, stable IDs, revision vectors, diffs, transaction journal | Cross-version migration and conflict/rollback fixtures |
| E3 Parsers & Conversion | Parser interface, read-only MPC Sample parser, semantic-loss report | Authorized golden projects, fuzz/corrupt-file handling, legal clearance |
| E4 Audio & Stem | Capture plan, take metadata, validation/analyzer interface, render plan | Audio driver/endpoint tests, calibration, tail/silence/clipping test results |
| E5 MIDI & Control | Canonical MIDI event model, route graph, parameter normalizer | Hardware/host endpoint test, loop prevention, scaling/persistence tests |
| E6 Timing & Transport | Clock model, calibration record, timestamp mapping, drift detector | Reproducible timing measurements and safe correction policy |
| E7 Assets & Archive | Content hash, dependency graph, archive manifest, read-only relink plan | Hash collision policy, missing-asset recovery tests, user approval for changes |
| E8 Mix, FX & Automation | Mixer/routing graph, automation curve model, FX intent record | Host/device mapping evidence or wet/dry rendered fallback proof |
| E9 DAW & Host | Cubase/Reason adapter interface, host route contract, plug-in client plan | Exact host/version QA, state persistence, licensing/distribution review |
| E10 Diagnostics & Safety | Structured events, preflight, compatibility table, trace redaction | Failure-mode coverage and no sensitive music/data leakage in bundles |
| E11 Platform & IPC | Authenticated local IPC, process ownership, Windows backend abstraction | OS permission, upgrade, packaging, and non-real-time boundary tests |
| E12 SDK & Test Lab | Virtual device, profile validator, replay tests, signing model | Malicious/invalid profile handling and full regression fixtures |
| E13 Workflow Orchestrator | Finish-in-DAW plan, approvals, transaction steps, summary | No action executes before approval; cancellation and recovery verified |

## Legal, licensing, and rights gates

The project must not treat a file parser, device protocol, DAW project writer, plug-in format, driver API, or brand name as freely usable merely because it is technically observable. Each adapter requires a written record of its allowed implementation route.

| Area | Gate before implementation/distribution | Evidence to retain |
|---|---|---|
| Device project formats and protocols | Confirm whether public documentation, vendor permission, or legally reviewed interoperability work allows the intended read/write behavior | Source citation, license/permission, legal review outcome, supported firmware boundaries |
| Hardware control/write-back | Obtain a safe documented command route and rollback/recovery behavior | Command specification, test log, failure/recovery trace, device/firmware matrix |
| DAW project files | Never write undocumented proprietary files in production | Documented host API/import/export route, exact version tests, fallback package behavior |
| VST3 | Include applicable license notices and follow current SDK requirements; validate each host’s supported interfaces | SDK license record and per-host integration test report [1] |
| Reason Rack Extensions | Treat as a separate product/distribution route, including its platform and submission requirements | SDK/distribution review and test plan [2] |
| Audio backends and drivers | Review SDK/license/distribution terms individually, especially on Windows/macOS/Linux | Dependency inventory, licenses, installer/signing decisions |
| Open-source components | Maintain a software bill of materials and required notices | SBOM, lockfiles, license texts, attribution files |
| Community profiles | No executable community adapter before trust, signing, permissions, and revocation exist | Threat model, signing key policy, validator tests, moderation/revocation procedure |
| Music/project data | Default to local-only processing; obtain opt-in consent before any diagnostic upload or cloud feature | Privacy notice, redaction test, consent record, retention policy |
| Names and marketing | Do not imply manufacturer endorsement or use protected marks without permission | Trademark review and approved naming/copy list |

## Cross-platform implementation guide

The core remains portable, but the deployable products differ. Windows stays first because it is the reference workflow; mobile is not reduced to an afterthought, but direct hardware hosting begins only when its lifecycle and audio/MIDI paths are proven.

| Platform | First deliverable | Later direct-host work | Critical qualification concerns |
|---|---|---|---|
| Windows | Desktop app + local service + MPC Sample/Cubase/Reason reference tests | VST3 client, capture, mapping, sync | Driver/audio backend behavior, device identity, installer/signing, DAW version compatibility |
| macOS | Core/CLI build, desktop shell, storage intake, adapter test plan | AU/VST3, Core Audio/MIDI/USB routes, full desktop service | Code signing/notarization, DAW/plugin host versions, device permissions |
| Linux | Core/CLI build, package plan, storage/MIDI/audio capability inventory | Selected DAW routes and package-specific audio backends | Distribution fragmentation, audio backend choice, device access rules |
| Android | Companion session browser, diagnostics, project package access | USB-C hardware host and mobile DAW routing per qualified combination | USB permissions, audio/MIDI lifecycle, background limits, device variation |
| ChromeOS | Companion route and Android-compatible feasibility layer | USB/audio path only when ChromeOS device class passes test matrix | Hardware and container differences, USB/audio behavior variation |
| iPadOS | Companion session browser and desktop-service remote control | Direct bridge host and compatible mobile DAW routing | Entitlements, file access, audio/MIDI, lifecycle/background behavior |
| iOS | Companion session browser, remote monitoring/control plan | Direct host only for a supported mobile route | Same platform constraints as iPadOS plus phone connection/power constraints |

## Reference acceptance scenario

A v1.0 readiness test uses a user-authorized MPC Sample project and must perform this conservative workflow:

1. Identify the exact device, firmware, Windows version, Cubase or Reason version, and connection route.
2. Read the source project without modification and create an external backup plus session snapshot.
3. Parse the supported project state and enumerate every retained, rendered, unknown, or unsupported item.
4. Resolve assets; collect them with robust content fingerprints and human-readable provenance.
5. Build the DAW route using only a qualified direct, host-assisted, or exchange-package path.
6. If audio capture is enabled, calibrate timing, capture with take validation, preserve source/tail policy, and record alignment metadata.
7. Reopen the host project or session client and verify it resolves only to the correct bridge session.
8. Simulate cancellation, missing asset, device disconnect, and conflicting offline change; verify that the originals remain usable and recovery choices are visible.
9. Export an end report stating exactly which requirements from the matrix were met for that configuration.

## Definition of “universal”

The word **universal** refers to a shared canonical model, adapter system, profile format, and capability negotiator—not to an unsupported promise that every device and DAW can be controlled perfectly. A device/DAW/platform route is universal-product coverage only when it integrates through these common contracts and meets its own qualification gates. Where a physical or host limitation exists, the universal behavior is to select the best safe fallback and explain it clearly.

## References

[1]: https://www.steinberg.net/developers/vstsdk/ "Steinberg — About the VST SDK"
[2]: https://developer.reasonstudios.com/documentation/rack-extension-sdk/4.3.0/rack-extension-dev-guide "Reason Studios — Rack Extension Developer Guide"

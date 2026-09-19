# Capability Matrix and Cross-Platform Qualification

The Universal Bridge must not advertise generic “cross-platform support” without identifying the exact hardware, operating system, DAW, and connection path. A capability is available only when all four layers have been qualified together.

> **Effective feature set = device capability + OS capability + DAW capability + connection capability.**

## Reference matrix

| Device | OS | Host/DAW | Connection | Current route | Status |
|---|---|---|---|---|---|
| MPC Sample | Windows | Cubase | Project folder | Read-only preflight, asset/MIDI exchange package, external backup | Implemented prototype |
| MPC Sample | Windows | Reason | Project folder | Read-only preflight, asset/MIDI exchange package, external backup | Implemented prototype |
| MPC Sample | Windows | Cubase | USB MIDI / USB audio | Planned capability probe and local service | Not implemented |
| MPC Sample | Windows | Reason | USB MIDI / USB audio | Planned capability probe and local service | Not implemented |
| MPC Sample | macOS | Cubase / Reason | Project folder, USB MIDI/audio | C++ core portability target | Not qualified |
| MPC Sample | Linux | Qualified hosts | Project folder, ALSA/JACK/PipeWire MIDI/audio | C++ core portability target | Not qualified |
| MPC Sample | Android / ChromeOS | Qualified mobile DAW | USB-C | Companion first; direct host only after profile qualification | Not qualified |
| MPC Sample | iPadOS / iOS | Qualified mobile DAW | USB-C | Companion first; direct host only after platform evaluation | Not qualified |
| Future devices | Any | Any | Any | Adapter/profile dependent | Not qualified |

## Feature vocabulary

The following labels have precise meanings and must be used in application UI, marketing, reports, and support material.

| Label | Meaning |
|---|---|
| **Implemented prototype** | Included in the checked-in executable and covered by the fixture suite |
| **Qualified** | Tested on real hardware/host/OS versions with published acceptance evidence |
| **Supported with fallback** | A safe reduced path is available, such as a transparent audio/MIDI exchange package |
| **Experimental** | Available only to opt-in testing; may change and must never affect source projects automatically |
| **Not qualified** | The code may be portable or architected for this route, but no user-facing capability claim can be made |
| **Unavailable** | Blocked by a device/host/OS limitation or intentionally disabled for safety |

## Release gates

A matrix entry moves from **not qualified** to **qualified** only after every relevant gate below has passed.

| Gate | Required evidence |
|---|---|
| Identity | Device model, firmware, OS version, DAW edition/version, driver, and connection method are recorded |
| Safety | Source project remains byte-for-byte unchanged; external backup and recovery tests pass |
| Transfer | Test fixtures and at least one real authorized project complete through the advertised route |
| Timing | Audio/MIDI timing behavior is measured for the route; any compensation claims are reproduced |
| Reopen | DAW/plug-in/session state restores the correct bridge identity and never chooses the wrong source project |
| Failure | Disconnect, cancelled transfer, low disk space, missing asset, unreadable file, and invalid project cases are handled transparently |
| Regression | The route passes virtual-device/fixture tests and the hardware-in-the-loop suite after relevant code changes |
| Documentation | UI and documentation describe actual limitations, fallbacks, and user recovery steps |

## Device integration tiers

| Tier | Device capability | Correct bridge behavior |
|---|---|---|
| A — Deep integration | Documented project data, MIDI, parameter state, transport, and multichannel audio | Near-real-time mirror only after full timing, recovery, and write-back qualification |
| B — Project + MIDI + stereo audio | Project data/asset access, MIDI, and stereo USB or analogue audio | Reconstruction plus safely qualified sequential/assisted capture; no claim of simultaneous multi-output audio |
| C — MIDI + analogue audio | MIDI control and physical audio output but limited project access | Capture with manual or controlled part isolation, MIDI hand-off, and timing calibration |
| D — Legacy/storage mode | Storage/project files and perhaps traditional MIDI/audio only | Project parsing, asset conversion, offline exchange, and conservative legacy workflows |

The MPC Sample reference profile is modeled as **Tier B** for planning. The executable prototype currently performs only the non-destructive storage/project-folder portion of that tier; real USB/MIDI/audio behavior remains unqualified until tested on the exact device/firmware and Windows configuration.

## Roadmap sequence

The user’s Windows/Cubase/Reason workflow remains the acceptance reference. Platform expansion follows the same core and changes only platform and host adapters.

| Priority | Platform scope | First practical deliverable |
|---:|---|---|
| 1 | Windows desktop | Local device service, MPC Sample capability probe, MIDI/audio diagnostic, Cubase/Reason exchange and VST3 connection proof |
| 2 | macOS desktop | Core build, storage intake, qualified audio/MIDI backend, VST3/AU host capability matrix |
| 3 | Linux desktop | Core build, distribution packaging, audio/MIDI backend choice, qualified DAW/device routes |
| 4 | Android and ChromeOS | Companion session browser and diagnostics; USB-C direct-host feasibility testing |
| 5 | iPadOS and iOS | Companion session browser and diagnostics; native audio/MIDI/host feasibility testing |
| 6 | Multi-device expansion | Modern MPC family, SP-404, Elektron, Maschine, Circuit, then signed community profiles |

The architecture preserves the requested desktop, tablet, and mobile ambition, but it puts the producer’s current Windows system first so every new core feature is tested in a real music-production workflow rather than only in an abstract capability model.

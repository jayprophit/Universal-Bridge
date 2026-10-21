# Platform Capability Contract

This document defines the hard-coded operating-system states used by the Universal Bridge prototype. It preserves the requested universal platform direction while differentiating **implemented code paths**, **portable core targets**, **planned native host modes**, and **unavailable features**.

> A platform identifier is not a promise of full device, audio, MIDI, plug-in, or DAW compatibility. It is an input to capability negotiation.

## Supported identifiers

| Identifier | Product family | Primary form factor | Prototype state |
|---|---|---|---|
| `windows` | Microsoft Windows | Desktop / tablet | Reference desktop route |
| `macos` | macOS | Desktop / laptop | Portable-core target |
| `linux` | Linux | Desktop / laptop | Portable-core target |
| `android` | Android | Phone / tablet | Future companion/direct-host target |
| `chromeos` | ChromeOS | Laptop / tablet | Future companion/direct-host target |
| `ipados` | iPadOS | Tablet | Future companion/direct-host target |
| `ios` | iOS | Phone | Future companion/direct-host target |

## Platform states

| State | Meaning | What the prototype permits |
|---|---|---|
| `reference_desktop` | Initial product workflow and current distribution/build guidance | Project-folder intake, backup, exchange package, Cubase/Reason target selection |
| `portable_core_target` | Shared C++ core is intentionally written to be portable, but platform I/O is not qualified | Project-folder intake and exchange package only when locally compiled; all hardware routes remain disabled |
| `future_mobile_host` | Intended mobile/tablet platform; native runtime has not been built or certified | Session records, profile declarations, and package descriptions only; no claim that this executable runs on the device |

## Capability policies

The capability engine adds restrictions, never invented access. Every platform profile begins with conservative defaults. A device profile and DAW profile can only enable a route when their requirements and the platform’s verified access methods overlap.

| Capability | Windows | macOS / Linux | Android / ChromeOS / iPadOS / iOS |
|---|---|---|---|
| Folder-based preflight and exchange description | Enabled reference workflow | Coded portable-core target | Described in session only; not distributed as a native mobile app |
| Physical device discovery | Disabled in v0.2.0 pending real-device service | Disabled | Disabled |
| USB MIDI/audio | Profile-visible but not active | Profile-visible but not active | Profile-visible but not active |
| Direct DAW project creation | Disabled | Disabled | Disabled |
| VST3 client | Architectural route only | Architectural route only | Not applicable as a generic mobile route |
| Companion session control | Planned | Planned | Planned |
| Hardware write-back / two-way sync | Disabled | Disabled | Disabled |

## Compatibility guarantee

A future public release may move a platform/profile pair from `portable_core_target` or `future_mobile_host` to `qualified` only after the exact device × OS × DAW × connection combination passes the project-safety, transfer, timing, reconnection, and regression gates in `CAPABILITY_MATRIX.md`.

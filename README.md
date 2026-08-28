# Universal Hardware Session Bridge

> **v0.5 pre-release production foundation.** This public repository is proprietary, not open source. See [LICENSE](LICENSE), the legacy boundary in [licenses/LEGACY-LICENSING.md](licenses/LEGACY-LICENSING.md), and the exact implementation status in [docs/PRODUCTION_STATUS.md](docs/PRODUCTION_STATUS.md).

**Version:** 0.5.0 pre-release production foundation
**Reference workflow:** Akai MPC Sample → Windows → Cubase or Reason  
**Hard-coded platform profiles:** Windows, macOS, Linux, Android, ChromeOS, iPadOS, and iOS  
**Design posture:** local-first, source-project safe, capability-aware

Universal Hardware Session Bridge is the beginning of a cross-manufacturer bridge between music hardware and DAWs. It is deliberately **not** presented as a finished MPC controller, stem recorder, DAW-project writer, or live two-way synchronizer. The working prototype builds the foundation required for those capabilities: it inventories a project folder, creates a versioned neutral session, backs up the source outside the source directory, produces an auditable audio/MIDI exchange package, and makes every current limitation explicit.

> The bridge never writes into the selected source project folder. It writes outputs, backups, manifests, and exchange files only to the destination folder you choose.

## What this prototype does

The `ubridge` command accepts a complete project folder and a target of **Cubase** or **Reason**. It discovers recognized project, audio, and MIDI files; creates a stable project fingerprint; records the effective MPC Sample profile; copies supported audio/MIDI assets into a separate exchange package; and generates a report that tells the producer what was found, copied, limited, or not yet supported.

| Capability | Prototype status |
|---|---|
| Non-destructive project-folder intake | Implemented |
| Recognized project, audio, and MIDI inventory | Implemented |
| Versioned neutral-session manifest (`.ubridge.json`) | Implemented |
| Asset fingerprinting and exchange-package copying | Implemented |
| Separate backup copy outside the source tree | Implemented by default |
| Cubase and Reason exchange guides | Implemented |
| Source-safety guard against output inside the source folder | Implemented |
| Hard-coded operating-system capability profiles | Implemented for Windows, macOS, Linux, Android, ChromeOS, iPadOS, and iOS |
| Canonical tracks, pads, clips, arrangement, mix/FX, routing, automation, and performance model | Implemented and fixture-tested |
| Project health, missing/duplicate/unused asset analysis, and portable archive manifest | Implemented and fixture-tested |
| Timing/drift calculations, stem validation, MIDI route checks, controller scaling, mixer/automation plans | Implemented and fixture-tested |
| Profile validation, compatibility report, workflow/archive/report serialization, and local service safe mode | Implemented and fixture-tested |
| Direct Cubase/Reason project-file generation | Intentionally unavailable |
| Proprietary MPC project parsing | Intentionally unavailable |
| Read-only Windows USB identity discovery | Experimental inventory for observed MPC Sample `VID 09E8` / `PID 205C`; no interface is opened |
| Live audio capture, MIDI routing, device control, or protocol integration | Architectural interfaces/scaffolding only; not implemented or qualified |
| Hardware write-back or live two-way parameter synchronization | Intentionally unavailable |

The prototype uses a transparent exchange-package fallback because VST3 is a plug-in interface for real-time audio components rather than a universal permission to create proprietary DAW project files. VST3 also exposes host-dependent optional interfaces, so each host integration must be verified before it is enabled.[1] Reason supports VST3 plug-ins in its standalone music-making software, making a shared VST3 client a practical future evaluation path, but that does not by itself establish project-writing or full session-control capability.[2]

## Repository layout

```text
universal-bridge/
├── CMakeLists.txt                 # Portable C++20 build
├── src/main.cpp                   # Safe preflight and exchange-bundle implementation
├── profiles/                      # Device and DAW capability declarations
├── fixtures/mpc_sample_demo/      # Sanitized regression fixture; not a real MPC project
├── tests/validate_outputs.py      # Deterministic safety/output checks
├── examples/Run-UniversalBridge.ps1
└── docs/                          # Architecture, capability, and roadmap documents
```

## Windows build and run

The primary reference operating system is Windows. Install a supported Visual Studio Build Tools or Visual Studio Community installation with the **Desktop development with C++** workload and CMake support. The checked-in MSVC presets target Visual Studio 2026. The PowerShell launcher automatically selects Visual Studio 2026 when present and falls back to Visual Studio 2022, then configures, builds, and executes the reference preflight.

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\examples\Run-UniversalBridge.ps1 `
  -ProjectFolder "D:\Music\MPC Projects\My Beat" `
  -Daw cubase `
  -TargetOs windows `
  -OutputFolder "D:\Music\Bridge Output\My Beat"
```

For Reason, change `-Daw cubase` to `-Daw reason`. The `-TargetOs` argument accepts `windows`, `macos`, `linux`, `android`, `chromeos`, `ipados`, or `ios`. Select **Windows** for the qualified reference workflow. The other identifiers create a hard-coded capability record for that target but do not claim a native app, hardware route, or DAW integration is ready on that platform. The output folder must be different from, and outside, the project folder. Before working with an irreplaceable project, keep an independent backup and run the tool on a copy first. The prototype creates a second copy inside the chosen output folder unless `-NoBackup` is supplied.

You can also build and test with the checked-in CMake presets. Install CMake, Ninja, and either Visual Studio C++ Build Tools, LLVM/Clang, or a current MinGW-w64 GCC toolchain, then run:

```powershell
cmake --preset dev
cmake --build --preset dev
ctest --preset dev

cmake --preset release
cmake --build --preset release
ctest --preset release
cmake --build --preset release --target package

.\build\release\bin\ubridge.exe devices
.\build\release\bin\ubridge.exe preflight `
  --project "D:\Music\MPC Projects\My Beat" `
  --daw cubase `
  --target-os windows `
  --output "D:\Music\Bridge Output\My Beat"
```

On macOS or Linux, the portable command-line core can be built using CMake and a C++20 compiler. That establishes developer portability only; it does **not** certify hardware, DAW, audio, MIDI, plug-in, or packaging support on those platforms.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/ubridge preflight \
  --project ./fixtures/mpc_sample_demo \
  --daw cubase \
  --output ./examples/output-cubase
```

## Generated package

A successful run creates the following structure in the selected output folder.

| Generated item | Purpose |
|---|---|
| `session.ubridge.json` | Neutral session record: version, project fingerprint, target route, capabilities, inventory, and findings |
| `diagnostics.json` | Machine-readable safety and preflight information |
| `preflight-report.md` | Human-readable summary of inventory, capabilities, warnings, and import route |
| `Backup/<source-project>/` | Separate copy of the input project, unless backup was disabled |
| `Exchange/Audio/` | Copied recognized audio assets, retaining relative folders |
| `Exchange/MIDI/` | Copied recognized MIDI assets, retaining relative folders |
| `Exchange/IMPORT_CUBASE.md` or `Exchange/IMPORT_REASON.md` | Target-specific safe import procedure |

The `.ubridge` session is not a substitute for the original hardware project. It is a versioned, local record of what the bridge read, what it exported, and what it could not yet translate. Its declared capabilities are intentionally conservative: **project read is true**, while **project write, native DAW-project generation, sequential stem capture, and bidirectional parameters are false**.

## Capability model

The long-term product determines an effective feature set from four independent inputs rather than presenting a vague global compatibility claim.

| Dimension | Prototype example | Why it matters |
|---|---|---|
| Device | MPC Sample | Determines available project, MIDI, audio, storage, and control functions |
| Operating system | Windows | Determines USB, audio/MIDI backend, local process, and plug-in requirements |
| DAW/host | Cubase or Reason | Determines which import, parameter, plug-in, and state-persistence routes are permitted |
| Connection | Project folder / future USB MIDI / future stereo audio | Determines what can actually be read, routed, or captured |

This model now has hard-coded profiles for Windows, macOS, Linux, Android, ChromeOS, iPadOS, and iOS. It provides a safe expansion path but does not equate portability of the source code with equivalence of runtime features. The command reports each profile as `reference_desktop`, `portable_core_target`, or `future_mobile_host`, and disables unqualified hardware/host routes rather than pretending they are available. On mobile and tablet platforms, the future bridge will support three separately qualified modes: **direct bridge host**, **DAW integration through the available audio/MIDI/plug-in route**, and **companion control of a desktop host**.

## Safety model

The bridge is designed to earn trust before it attempts deeper synchronization. The source project folder is treated as immutable. The command refuses an output folder inside the source directory, prevents accidental recursive backups, creates an external backup by default, and records whether backup creation succeeded. It does not use undocumented device protocols, does not send commands to hardware, and does not modify DAW project files.

The current fingerprints use FNV-1a 64-bit values to detect ordinary fixture and file changes efficiently. They are **not cryptographic integrity proofs**. Before production synchronization, this implementation must move to a cryptographic hash (such as SHA-256 or BLAKE3), persistent transaction journals, source/destination revision vectors, cancellation recovery, and independently testable conflict-resolution policies.

## Testing

The fixture directory contains simple sanitized placeholder files and must never be confused with a valid proprietary MPC project. It verifies inventory, copy behavior, JSON validity, Cubase/Reason output generation, backup creation, invalid-DAW rejection, output-in-source rejection, and byte-for-byte source preservation.

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
./build/dev/bin/ubridge_test_lab
UBRIDGE_BINARY=./build/dev/bin/ubridge python3 tests/validate_outputs.py
```

Visual Studio Community/Build Tools 2026 users can independently validate the Microsoft compiler and Windows SDK path with the multi-configuration presets:

```powershell
cmake --preset msvc-dev
cmake --build --preset msvc-dev
ctest --preset msvc-dev

cmake --preset msvc-release
cmake --build --preset msvc-release
ctest --preset msvc-release
cmake --build --preset msvc-release --target package
```

VS Code uses `CMakePresets.json` through the recommended Microsoft CMake Tools and C/C++ extensions. Select `msvc-dev` when validating the Microsoft toolchain, or `dev` for the portable Ninja developer path. The repository does not require SQL Server or SQL Server Management Studio.

A passing test suite confirms prototype behavior only. It does not prove compatibility with a physical MPC Sample, a particular MPC firmware version, Cubase or Reason edition, audio interface, USB hub, operating-system driver, or DAW plug-in host configuration.

## Operating-system profiles

| Target OS | Hard-coded state | Current behavior |
|---|---|---|
| Windows | `reference_desktop` | Reference preflight/exchange workflow; physical hardware service still disabled in this prototype |
| macOS, Linux | `portable_core_target` | Preflight/exchange capability record; hardware, audio/MIDI, plug-in, and DAW routes remain unqualified |
| Android, ChromeOS, iPadOS, iOS | `future_mobile_host` | Mobile/tablet bridge and companion pathway recorded; no native mobile executable or direct DAW/hardware integration is enabled |

See [`docs/PLATFORM_CONTRACT.md`](docs/PLATFORM_CONTRACT.md) for the full platform contract and [`docs/CAPABILITY_MATRIX.md`](docs/CAPABILITY_MATRIX.md) for the release gates.

## Complete specification coverage

The complete supplied specification is now converted into a buildable program of work. The safely implementable offline modules have been added in v0.4.0; see [`docs/V0_4_IMPLEMENTED_FOUNDATION.md`](docs/V0_4_IMPLEMENTED_FOUNDATION.md) for the exact implementation/test status and the features that remain consciously gated. Read [`docs/MASTER_REQUIREMENTS_MATRIX.md`](docs/MASTER_REQUIREMENTS_MATRIX.md) for all **100 requirements**, including the responsible engine, delivery stage, current coverage, dependency, and acceptance gate. Read [`docs/BUILDABLE_ARCHITECTURE.md`](docs/BUILDABLE_ARCHITECTURE.md) for the shared C++ core, adapter, platform-service, virtual-device, and workflow structure. Read [`docs/DELIVERY_ROADMAP_AND_GATES.md`](docs/DELIVERY_ROADMAP_AND_GATES.md) for the release order, technical/legal gates, platform implementation path, and v1.0 acceptance scenario.

The repository now compiles a `ubridge_test_lab` executable. It exercises the universal device, DAW, and operating-system profile catalogs; conservative capability negotiation; mobile/desktop safeguards; workflow planning; transaction transitions; and virtual-device disconnect/reconnect behavior without contacting real hardware.

## Next implementation milestones

| Milestone | Outcome | Release gate |
|---|---|---|
| MPC parser feasibility | Read a documented or legally cleared MPC project representation into the neutral session graph | Golden projects parse without modifying sources |
| Windows hardware service | Real device discovery, MIDI I/O, local IPC, and diagnostics | Tested reconnect and permission behavior on the reference MPC Sample |
| Audio/timing engine | Calibrated stereo capture, offset correction, tail handling, and failure detection | Measured alignment and no real-time audio-thread blocking |
| Cubase/Reason host adapters | VST3 client and/or documented interchange/direct host actions | Verified per-host capability matrix and reopen persistence |
| Conflict-aware synchronization | Revision vectors, explicit authority rules, backups, rollback, and user-visible diffs | No silent overwrite across disconnection and cancellation scenarios |
| macOS/Linux desktop ports | Qualified platform backends and packaging | Platform-specific hardware/DAW regression matrices pass |
| Android, ChromeOS, iPadOS, and iOS | Companion first, then direct host capabilities where feasible | Device × OS × DAW × connection route is certified separately |

## References

[1]: https://steinbergmedia.github.io/vst3_dev_portal/pages/Technical%2BDocumentation/Index.html "Steinberg — VST 3 Technical Documentation"
[2]: https://www.reasonstudios.com/press/vst3-support-for-the-reason-music-making-software "Reason Studios — VST3 Support for Reason"
[3]: https://www.steinberg.net/developers/vstsdk/ "Steinberg — About the VST SDK"
[4]: https://developer.reasonstudios.com/documentation/rack-extension-sdk/4.3.0/rack-extension-dev-guide "Reason Studios — Rack Extension Developer Guide"

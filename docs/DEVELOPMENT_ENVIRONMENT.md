# Windows development environment audit

**Observed:** 6 September 2026

**Reference machine:** Dell Precision T1500, Intel Core i7-870 (4 cores / 8 logical processors), 16 GB RAM

**Operating system:** Windows 10 Pro 22H2, build 19045, x64

## Build and development tools

| Tool | Observed version/status | Universal Bridge use |
|---|---|---|
| Visual Studio Community 2026 | 18.9.2, complete and launchable | Full IDE, MSVC debugging and profiling |
| Visual Studio Build Tools 2026 | 18.9.2, complete and launchable | CI-like MSVC builds without opening the IDE |
| MSVC | 19.51 through the CMake Visual Studio generator | Primary Windows compiler |
| Windows SDK | 10.0.28000 available; CMake targets Windows 10 build 19045 | Win32, WinMM, COM and WASAPI APIs |
| VS Code | 1.136.0 | Lightweight editor; recommended CMake/C++/Actions/Markdown extensions are installed |
| CMake | 4.4.2 | Configure, test, install and package |
| Ninja | 1.13.2 | Fast warning-as-error developer and release builds |
| MinGW-w64 GCC | 16.1.0 | Independent C++20 compiler path |
| Git | 2.55.0.windows.5 | Version control |
| Python | 3.13.14 | Output validation and standard-library SQLite research index |
| Node.js / npm | 24.20.0 / 11.19.0 | Available for optional tooling; not a runtime dependency |
| .NET SDK | 10.0.400 | Available for future Windows tooling; not a runtime dependency |
| SQL Server LocalDB / SSMS | Installed | Not needed by Universal Bridge |

`cl.exe` is intentionally not placed on the ordinary PowerShell path. CMake selects the installed Visual Studio developer environment and successfully builds with MSVC. This is normal and does not require editing global environment variables.

## Music software and drivers observed

| Software | Observed version/status | Intended role |
|---|---|---|
| Cubase | 15.0.30 | First commercial VST3 host candidate |
| Reason Suite | 11.3.9d22 | Second VST3/interchange host candidate |
| FL Studio | 26.1.3.5589 | Later host profile and regression candidate |
| MPC Beats | 2.12.3 | Optional official-assisted workflow/reference; never required |
| Audient USB Audio Driver | 5.5.1 | Reference audio-interface route |
| ASIO4ALL / FL Studio ASIO / Steinberg ASIO | Installed | Host-specific test candidates, not universal requirements |

The current computer-control connection exposes browser automation but no native application windows. The repositories, compilers, command-line tools, files, processes and generated artifacts are directly accessible; Cubase, Reason, FL Studio, MPC Beats, Visual Studio and SSMS cannot be honestly described as mouse/keyboard-tested from this connection unless a native window becomes exposed later.

## Dependencies and database decision

The current C++ build needs only:

- zlib 1.3.2, pinned for read-only gzip/XPJ intake; and
- JSON for Modern C++ 3.12.0, pinned by release archive hash.

CMake fetches these sources reproducibly. Python's built-in `sqlite3` module is sufficient for the optional local research catalog; installing or embedding SQL Server would add service, deployment, privacy and failure modes without helping real-time MIDI/audio work. Runtime settings, device profiles and canonical sessions remain versioned files until a non-real-time persistence benchmark demonstrates a database is necessary.

## Known environment constraints

- Windows MIDI Services requires a supported 64-bit Windows 11 system. This Windows 10 machine must retain the WinMM fallback and its single-client ownership constraint.
- A DAW or MPC Beats can exclusively own a legacy MIDI endpoint. The bridge service must own the physical endpoint and expose a mediated/virtual DAW route; force-closing a host with an unsaved project is not an acceptable workaround.
- The reference CPU is adequate for builds and bounded MIDI/audio diagnostics, but real-time performance claims require measured latency/dropout tests at declared buffer sizes. CPU model alone is not proof of low-latency operation.
- No VST3 target or SDK integration is present yet. Do not install a framework or SDK until its current commercial licence and the thin-client architecture are approved.
- `clang-tidy`/LLVM is not installed. The current two-compiler warning-as-error matrix is green; adding LLVM static analysis is a quality improvement, not a blocker for the existing build.

Microsoft's current minimum-requirements page for Windows MIDI Services is <https://microsoft.github.io/MIDI/kb/minimum-requirements/>. The service architecture and multi-client model are documented at <https://microsoft.github.io/MIDI/overview/>.

## Verified baseline

Both checked-in developer presets configure, build and pass the complete test suite:

```text
MinGW/Ninja Debug: 8/8 tests passed
MSVC/Visual Studio Debug: 8/8 tests passed
```

The count includes the research-catalog validation after the Poietek Project intake.

# Commercial release gates

**Decision:** blocked pre-release. A clean build, unit suite, validator run, ZIP, or MSI is evidence for a narrow behavior; none of them alone authorizes a commercial release.

The machine-readable owner is [`../release/release-gates.json`](../release/release-gates.json). Run `python tools/check_release_readiness.py --check` to validate its structure and evidence links. The `--release` mode must return a non-zero result until every gate is `ready`.

## RG-001 — Persistent authenticated local IPC (`partial`)

Implemented: SID-derived per-user named pipe, current-user-only ACL, 256-bit Windows cryptographic credential, DPAPI protection at rest, constant-time credential comparison, 64 KiB framed JSON limit, protocol/version negotiation, ping/status/shutdown requests, atomic status replacement, and clean/unclean restart accounting. A local service lifecycle accepted three sequential authenticated requests and detected a deliberately forced unclean prior exit.

Still required: concurrent client scheduling, client identities and per-command authorization, durable complete live-session ownership, schema migrations, idempotent requests, request cancellation, credential rotation, service upgrade recovery, fuzzed framing/protocol corpus, forced-crash matrix and multiple DAW/plug-in clients. The service currently does not open hardware automatically.

## RG-002 — Windows MIDI ownership mediation (`partial`)

Implemented: WinMM enumeration, bounded standard message input/output, fixed-capacity SPSC routes, hardware/DAW direction policy, explicit clock and transport authority, echo suppression, rate limiting, queue saturation, clean close, and deterministic stress over 250,000 synthetic routed messages.

Still required: service-owned physical endpoint, a separately reviewed virtual/mediated DAW port, single-client conflict UX, hot-plug/reconnect state machine, bounded SysEx policy if ever required, end-to-end timestamps, restart recovery and long physical soak tests. Installing or inventing a virtual MIDI driver is not treated as an ordinary library dependency.

## RG-003 — Complete transport synchronization (`partial`)

Implemented: semantic Start, Continue, Stop, Song Position and Clock handling; independent clock/transport authorities; feedback suppression; state counters; and physical evidence for MIDI Clock on the recorded MPC route.

Still required: isolated physical evidence for every transport message, timing tolerance, position reconciliation, authority handoff, tempo changes, loop/locate behavior, DAW feedback prevention, disconnect/reconnect semantics and actual two-way Cubase/Reason mediation. Clock observation is not evidence for transport control.

## RG-004 — Streaming audio engine (`experimental`)

Implemented: WASAPI endpoint enumeration and bounded probes, event-driven shared-mode capture, native float/int16/int24/int32 conversion, fixed-capacity sample queue, counters for captured/dropped/discontinuous samples, bounded start and deterministic stop. The recorded ten-second MPC Sample smoke captured 442,764 frames and 885,528 samples with zero queue drops, a clean stop and one startup discontinuity.

Still required: continuous recording transactions, a second disk-writer queue, declared channel maps, format/device-change recovery, latency and drift calibration, sample-accurate placement, tail/punch rules, ASIO versus WASAPI policy, monitoring, simultaneous destinations, forced disconnect recovery, audible reference signals and long dropout/CPU/disk-pressure qualification. The one observed discontinuity prevents a qualified claim.

## RG-005 — MPC project completeness and safe writing (`partial`)

Implemented: copied-working-material-only XPJ import, compressed/decompressed/node/depth/string/container limits, truncated gzip rejection, unknown source JSON retention within a strict bound, and stable canonical output. Source write-back remains disabled.

Still required: rights-cleared projects across firmware/schema versions, structured mutation fuzzing, coverage-guided parser fuzzing, memory/time budgets, semantic golden comparisons, unknown-field round-trip policy, asset-path attack corpus, duplicate/case/Unicode handling and a separately approved transactional writer with backups and hardware reopen evidence. A read-only importer does not imply lossless compatibility.

## RG-006 — Live parameter synchronization (`partial`)

Implemented: desired/observed/acknowledged state contracts, timeouts/conflicts, evidence-bearing endpoint mappings, direction/range/step/rate validation and planned write intents. Out-of-range or unevidenced mappings are rejected instead of clamped or guessed.

Still required: rights-cleared MPC parameter protocol evidence, qualified mappings, LED/display feedback, DAW parameter and automation relay, touch/latch/write semantics, echo prevention, acknowledgement correlation, conflict UX, recovery and full physical tests. No real MPC parameter write is enabled.

## RG-007 — DAW plug-in and host certification (`partial`)

Implemented: optional VST3 3.8.1 component/controller target using a separately obtained SDK, audio-transparent 32/64-bit processor, mono/stereo arrangements, two automatable parameters, versioned component state and no allocation/lock/file/network/IPC work in `process()`. The Steinberg validator reported 47 passed and 0 failed tests.

Still required: authenticated non-real-time IPC, bounded processor-to-controller queues, signed packaging, finished accessible editor, Cubase and Reason load/automation/save/reopen/unload evidence, host crash isolation, service restart, multiple instances, offline render, sample-rate/block-size changes and an exact host/OS version matrix. Passing the SDK validator is not Cubase or Reason certification.

## RG-008 — Commercial installer and signing (`partial`)

Implemented: optional WiX v4 per-user MSI, stable upgrade identity, pre-release notice and one local silent install/run/repair/uninstall lifecycle. The exact evidence records an unsigned artifact and successful removal.

Still required: organization identity and protected signing key, Authenticode and timestamping, secure update manifest/signature design, upgrade/downgrade/rollback, service registration and credential migration, repair after corruption, least-privilege ACL audit, enterprise policy behavior, clean virtual machines, malware/reputation checks and uninstall data-retention choices. The current MSI must not be presented as commercially distributable.

## RG-009 — Release legal work (`blocked_external`)

Repository templates record intended subjects but are not legal approval. Qualified counsel must approve the proprietary/legacy boundary, EULA, privacy notice, consumer and commercial terms, refund/support policy, SDK and third-party obligations, VST trademark use, device/DAW trademarks, clean-room/protocol work, telemetry posture, distribution regions and accessibility commitments. Public visibility does not make the v0.5 code open source, but it does make the source downloadable and reviewable.

## RG-010 — Extended quality qualification (`partial`)

Implemented: MSVC, GCC and Clang-CL warning-as-error paths, 11 deterministic local tests on all three Windows compilers, a passing local Clang-CL AddressSanitizer run, targeted Clang static analysis of the new high-risk sources, concurrent queue stress, synthetic mediation stress, Linux ASan/UBSan CI, CodeQL CI, monthly dependency updates, pinned GitHub Actions, release-evidence validation and source-preservation tests. The local Windows MinGW installation lacks ASan/UBSan runtimes; Windows sanitizer coverage therefore uses Clang-CL AddressSanitizer while Linux CI supplies both AddressSanitizer and UndefinedBehaviorSanitizer.

Still required: a reviewed full-repository analyzer policy, coverage thresholds, parser fuzzing corpus and duration, dependency vulnerability disposition, real-time allocation assertions, latency/dropout/CPU/disk benchmarks, forced-disconnect/recovery tests, multi-hour hardware/DAW soak tests, corrupted-project corpus, reproducibility comparison, accessibility tests and multiple clean Windows machines with exact firmware/drivers/host versions.

## Release rule

No gate may become `ready` from a name, percentage, prose claim, successful compile, or caller-supplied status. Each owner must attach reproducible evidence satisfying the listed exit criteria. Until all ten gates are independently reviewed and ready, every artifact remains an experimental or pre-release development build.

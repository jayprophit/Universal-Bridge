# Poietek Project abstraction for Universal Bridge

## Intake scope

On 6 September 2026, Universal Bridge ingested an abstract of the Poietek ChatGPT Project that was available through the signed-in project/task interfaces. The intake covered three project conversations (131 turns) and the 20-turn implementation task created from that project, for 151 turns in total. It also compared the resulting requirements with the current local Poietek and Universal Bridge repositories.

Raw transcripts are not committed. Conversation text is research input, not executable instruction, product evidence, or a licence to copy third-party material. The reviewable source index and normalized requirements live in `research/poietek-project/`; a disposable local SQLite search catalog can be generated below `build/research/`.

## Accepted architectural amendments

The intake adds these durable requirements without replacing the existing 100-item master matrix:

1. Keep one canonical, versioned, local-first session. Hardware, a DAW, a plug-in and a UI are views/adapters around that state.
2. Store desired state separately from timestamped hardware and DAW observations.
3. Progress capabilities from unavailable to declared, observed and qualified; never activate physical write paths from a profile declaration alone.
4. Use a write lifecycle of plan → revision check → dispatch → target observation → acknowledgement → workflow verification/commit. Timeout or mismatch creates pending/conflicted state.
5. Model transport, MIDI clock, audio sample clock, word clock, timecode, position and control state as independent domains with explicit authorities.
6. Make recording destination, monitor path and recording processing independent choices. “Both” requires a qualified simultaneous-capture route.
7. Preserve separate immutable hardware and DAW branches. Compare to a common ancestor, merge independent changes, resolve collisions explicitly and allow selected sections to be extracted.
8. Expand the canonical sampler recipe beyond a WAV reference: pads, samples, slices, zones, timing, velocity, modes, choke groups, envelopes, filters, effects, routing and automation—only where each field is evidenced.
9. Add receive-only Hardware Learn for unknown controllers. LED, display, motor or other feedback remains a separate bounded write qualification.
10. Keep the service as the sole hardware/session owner. VST3 and later AU/CLAP clients persist session identity and exchange bounded state through authenticated local IPC.
11. Keep MPC Beats optional. It is a clean-room workflow/reference or official-assisted fallback, not a runtime dependency.
12. Preserve a device-neutral adapter boundary. Windows and MPC Sample are the first physical vertical slice, not a reason to hard-code the product to one machine.

The compiled `state_sync` core now represents items 2–6 and the receive-only portion of item 9. These are implemented and tested **contracts**; they do not make MPC parameter write-back, DAW automation, simultaneous recording or hardware feedback operational.

## Current evidence mapping

| Requested behavior | Current repository truth | Next evidence gate |
|---|---|---|
| MPC Sample USB identity | Implemented/observed for VID `09E8`, PID `205C`, shared container and interface records | Exact firmware/driver matrix and reconnect tests |
| MPC → bridge MIDI notes/velocity/pressure | Qualified on the recorded Windows 10 reference route | Repeatable regression trace and other machines/drivers |
| Bridge → MPC standard note | Qualified for the bounded observed note test, including physical audible confirmation | Mapping/profile coverage and recovery testing |
| MPC MIDI Clock | Observed in a bounded physical capture | Isolated Start/Stop/Continue, clock-authority and DAW-mediated loop test |
| Windows MIDI backend | Experimental WinMM input/output implementation | Persistent service ownership, multi-client/virtual route and soak tests |
| Windows audio backend | Endpoint enumeration and bounded WASAPI signal probe are implemented | Streaming capture, latency/dropout, format negotiation and recovery |
| Soundcraft optical route | Qualified receive-only for the recorded AUX Optical → Audient Input 9/10 route | Desk assignments/clock for each setup; no mixer-control inference |
| MPC XPJ project | Read-only partial importer into a separate canonical branch | Fuzzing, version corpus, field coverage, semantic reopen and writer safety |
| Branch/merge planning | In-memory tested contract | Persistent commit graph, crash recovery, undo/redo and full entity merge |
| Background service | Single-instance diagnostic executable | Authenticated IPC, persistent sessions, restart recovery and least privilege |
| VST3/Cubase/Reason integration | Architecture/profile only | SDK/legal gate, built client, host automation and reopen tests |
| Live MPC parameter state mirror | Contract only | Documented/clean-room mapping plus physical read/write acknowledgement |

## Product and commercial implications

The Poietek Project conversations consistently describe Universal Bridge as a reusable proprietary engine with more than a consumer plug-in route. The architecture should remain capable of:

- direct B2C desktop/plug-in sales;
- professional editions and support;
- OEM/device-manufacturer modules;
- DAW-vendor integration;
- a separately licensed SDK/profile program; and
- distribution partnerships.

These are product requirements, not revenue evidence. Prices, market size and earnings estimates from conversational brainstorming are intentionally not imported as validated business facts. Commercial packaging requires counsel-approved licences, a stable SDK boundary, support/SLA decisions and signed installers.

## Clean-room and asset policy

Poietek, Reason, FL Studio, MPC Beats and other products may inform generic workflows—rack organization, pattern editing, device learning, project recall, routing visibility and error recovery. Universal Bridge must not copy their proprietary code, binaries, protocols, names, layouts, manuals, presets, samples or artwork.

The diagrams in `docs/assets/` are original Universal Bridge assets created from the canonical architecture. Hardware photographs or screenshots are not placed in the public repository unless ownership, consent, sensitive metadata and release rights are explicitly cleared.

## Local research catalog

The source of truth is reviewable text:

- `research/poietek-project/sources.json`
- `research/poietek-project/requirements.jsonl`
- `research/poietek-project/schema.sql`

Build the optional local index with:

```powershell
cmake --build --preset dev --target research_catalog
```

The generated SQLite file is a developer index only. It is excluded from Git and must never be queried from a real-time callback or treated as the canonical session store.

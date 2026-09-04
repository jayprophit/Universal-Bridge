# MPC Beats workflow mapping

MPC Beats is an official Akai reference and potential assisted host, not a code
or interface source for Universal Bridge.

## Reusable workflow lessons

- Pad-oriented programs connect samples, performance and sequencing.
- MIDI Learn separates generic controller messages from software parameters.
- Portable MIDI maps can be exported and imported.
- Standalone composition can be exported as MIDI or audio and dragged into a
  different DAW.
- A plug-in instance can keep the MPC workflow inside another DAW host.

## Universal Bridge additions

Universal Bridge keeps a vendor-neutral canonical session above any host. MPC,
MPC Beats and another DAW receive independent revisions. Import never silently
overwrites the other side: changes are compared to a common ancestor and can be
kept, merged or extracted. Hardware and audio routes remain runtime-resolved so
the design is not tied to Akai, one sound card, or one CPU.

## Honest boundary

MPC Beats does not establish that MPC Sample is a Controller Mode device. Akai
documents MPC Sample project import into MPC hardware/software version 3.8 or
later, but explicitly says MPC Sample is not an MPC 3 controller. For MPC Sample,
Universal Bridge uses read-only project intake, standard MIDI/audio paths, and
the documented MPC 3.8+ project-import route as separate capabilities.

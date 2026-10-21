# Universal endpoint binding

Universal Bridge stores a portable **role and match rule**, never a machine-local
WinMM index, WASAPI identifier, CPU identifier, or one manufacturer's driver
path as the canonical route.

At each connection the platform backend enumerates the endpoints that exist on
that computer. The resolver filters them by required direction and channel
capacity, then ranks optional human-readable hints such as `drum`, `MPC`,
`ADAT`, or `optical`. Backend and protocol hints may help rank MIDI endpoints,
but are not mandatory device brands.

If two endpoints have the same best score, the result is ambiguous. The bridge
must ask the user to select one and may remember the resulting portable rule;
it must not silently open an arbitrary endpoint. A runtime endpoint ID may be
used for the current connection only.

This makes the core independent of the current Audient interface, Soundcraft
desk, Akai device, Windows machine, or CPU. A particular device/OS/DAW route is
still only operational after that combination has been observed and qualified.
Portability is an architectural property, not a claim that untested hardware
or proprietary project formats already work.

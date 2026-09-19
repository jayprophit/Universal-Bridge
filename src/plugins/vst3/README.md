# Experimental VST3 client

This optional target is a deliberately thin, audio-transparent VST3 component. It proves that
Universal Bridge can produce a loadable component/controller pair, expose automatable bridge and
bypass parameters, and persist a versioned component state without performing allocation, locking,
file I/O, network I/O, or service IPC in the audio callback.

It does **not** claim Cubase or Reason certification, authenticated service mediation, DAW transport
control, MPC parameter control, a finished editor, signed distribution, or project-file integration.
The SDK is not vendored. Configure `UBRIDGE_VST3_SDK_ROOT` with a separately obtained, reviewed
Steinberg VST3 SDK checkout and explicitly enable `UBRIDGE_BUILD_VST3_CLIENT`.

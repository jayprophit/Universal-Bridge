# Windows installer status

The default package remains a portable ZIP. An **unsigned, per-user development MSI** can be generated with CPack and WiX v4 for install/repair/uninstall testing. It deliberately does not register a persistent service, driver, virtual MIDI port, shell extension, auto-start task, updater or privileged component.

Configure a separate MSVC release tree with `UBRIDGE_BUILD_WINDOWS_MSI=ON` and point `UBRIDGE_WIX_ROOT` at a reviewed WiX v4 command-line installation. Build the project, then run CPack with the `WIX` generator.

The MSI is not commercially releasable until all of these gates pass:

- production EULA and privacy material are approved;
- binaries and installer are Authenticode-signed from a protected signing workflow;
- clean install, upgrade, repair, uninstall and rollback tests pass on supported Windows versions;
- per-user service registration and removal are implemented without leaving credentials or sessions behind;
- SmartScreen/reputation, malware scanning and software-bill-of-materials evidence are retained; and
- no driver or virtual MIDI component is bundled without its own signing, servicing and compatibility plan.

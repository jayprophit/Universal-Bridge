# Security policy

## Supported versions

Universal Bridge is currently a pre-release development project. No version is approved for production or safety-critical use, and no build receives a long-term security-support promise yet.

## Reporting a vulnerability

Do not publish credentials, private projects, device traces, unreleased protocol material, or exploit details in a public issue. Use GitHub's private vulnerability-reporting route at <https://github.com/jayprophit/Universal-Bridge/security/advisories/new> when it is available to you.

Include the affected commit/version, operating system, hardware/driver/DAW route, reproduction steps, impact, and whether a source project or physical device was modified. Remove personal paths and copyrighted project media from diagnostics unless they are essential and you are authorized to provide them.

The maintainers will validate scope before making a disclosure or release commitment. Receiving a report does not imply that an unqualified hardware write path will be activated for reproduction.

## Security boundaries

- Source projects are read-only inputs unless a separately approved transaction and restorable snapshot exist.
- Device discovery does not authorize control.
- Local IPC must remain current-user restricted and credential authenticated.
- Database, file, network, parser, logging, and device-enumeration work is forbidden on real-time audio/MIDI callbacks.
- Third-party binaries and plug-in SDKs require provenance, licence and integrity review before entering the build.
- Signing credentials must never be committed, included in diagnostic bundles, or exposed to pull-request workflows.

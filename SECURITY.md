# Security policy

Universal Bridge v0.5 is a pre-release proprietary foundation. Report suspected vulnerabilities privately to the repository owner through GitHub's private vulnerability reporting. Do not include project files, samples, credentials, device identifiers, or diagnostic archives in a public issue.

The application is local-first. Hardware writes, DAW writes, network transfer, automatic driver installation, and unsigned executable profiles are disabled unless a future implementation explicitly qualifies and gates them. Device discovery is read-only and must not open an interface merely because its VID/PID matches.

Supported security fixes target the current development branch. No public production version is supported yet.

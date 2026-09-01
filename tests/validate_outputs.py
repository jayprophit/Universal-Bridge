#!/usr/bin/env python3
"""Deterministic checks for Universal Bridge prototype outputs.

This script deliberately validates only generated fixtures. It does not inspect or
modify user projects. Run it after CMake builds the `ubridge` executable.
"""

from __future__ import annotations

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
_BINARY_CANDIDATES = (
    ROOT / "build" / "dev" / "bin" / ("ubridge.exe" if sys.platform == "win32" else "ubridge"),
    ROOT / "build" / "bin" / ("ubridge.exe" if sys.platform == "win32" else "ubridge"),
    ROOT / "build" / ("ubridge.exe" if sys.platform == "win32" else "ubridge"),
)
BINARY = Path(os.environ.get("UBRIDGE_BINARY", "")) if os.environ.get("UBRIDGE_BINARY") else next(
    (candidate for candidate in _BINARY_CANDIDATES if candidate.is_file()), _BINARY_CANDIDATES[0]
)
FIXTURE = ROOT / "fixtures" / "mpc_sample_demo"
OUTPUT_ROOT = ROOT / "tests" / "test-output"


def run(*args: str, expect_success: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        [str(BINARY), *args],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if expect_success and result.returncode != 0:
        raise AssertionError(f"Expected success, got {result.returncode}: {result.stderr}")
    if not expect_success and result.returncode == 0:
        raise AssertionError("Expected command to fail but it succeeded")
    return result


def source_snapshot() -> dict[str, str]:
    snapshot: dict[str, str] = {}
    for path in sorted(FIXTURE.rglob("*")):
        if path.is_file():
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            snapshot[str(path.relative_to(FIXTURE))] = digest
    return snapshot


def validate_exchange(daw: str) -> None:
    output = OUTPUT_ROOT / daw
    shutil.rmtree(output, ignore_errors=True)
    before = source_snapshot()
    result = run("preflight", "--project", str(FIXTURE), "--daw", daw, "--output", str(output))
    after = source_snapshot()
    assert before == after, "Preflight modified a source project fixture"
    assert "Preflight completed safely" in result.stdout
    assert (output / "readiness-summary.json").is_file()

    session = json.loads((output / "session.ubridge.json").read_text(encoding="utf-8"))
    diagnostics = json.loads((output / "diagnostics.json").read_text(encoding="utf-8"))
    readiness = json.loads((output / "readiness-summary.json").read_text(encoding="utf-8"))

    assert session["safety_mode"] == "read_only_source"
    assert readiness["product_stage"] == "foundation-pre-release"
    assert readiness["live_bridge_complete"] is False
    assert session["reference_route"]["target_daw"] == daw
    assert session["effective_capability"]["audio_channels"] == 2
    assert session["effective_capability"]["direct_daw_project_generation"] is False
    assert len(session["inventory"]["audio_assets"]) == 2
    assert len(session["inventory"]["midi_assets"]) == 1
    assert diagnostics["source_write_attempted"] is False
    assert diagnostics["source_backup_requested"] is True
    assert diagnostics["source_backup_created"] is True
    assert (output / "Backup" / "mpc_sample_demo" / "DemoBeat.xpj").is_file()
    assert (output / "Exchange" / "Audio" / "ProjectData" / "KICK_DEMO.wav").is_file()
    assert (output / "Exchange" / "MIDI" / "DemoBeat.mid").is_file()
    assert (output / "Exchange" / f"IMPORT_{daw.upper()}.md").is_file()


def validate_platform_contract(platform: str, state: str, local_preflight: bool, qualified: bool) -> None:
    output = OUTPUT_ROOT / f"platform-{platform}"
    shutil.rmtree(output, ignore_errors=True)
    result = run(
        "preflight",
        "--project",
        str(FIXTURE),
        "--daw",
        "cubase",
        "--target-os",
        platform,
        "--output",
        str(output),
    )
    assert "Preflight completed safely" in result.stdout
    session = json.loads((output / "session.ubridge.json").read_text(encoding="utf-8"))
    contract = session["platform_capability"]
    assert session["reference_route"]["target_os"] == platform
    assert contract["state"] == state
    assert contract["local_preflight"] is local_preflight
    assert contract["runtime_qualified"] is qualified
    if state == "future_mobile_host":
        assert contract["mobile_companion_route"] is True
        assert session["effective_capability"]["usb_audio"] is False
        assert any(finding["code"] == "mobile_host_planned" for finding in session["findings"])
    else:
        assert session["effective_capability"]["usb_audio"] is True
        assert session["effective_capability"]["hardware_service_active"] is False


def validate_safety_guards() -> None:
    unsafe_destination = FIXTURE / "unsafe-output"
    shutil.rmtree(unsafe_destination, ignore_errors=True)
    result = run(
        "preflight",
        "--project",
        str(FIXTURE),
        "--daw",
        "cubase",
        "--output",
        str(unsafe_destination),
        expect_success=False,
    )
    assert "outside the source project folder" in result.stderr
    shutil.rmtree(unsafe_destination, ignore_errors=True)

    result = run(
        "preflight",
        "--project",
        str(FIXTURE),
        "--daw",
        "other",
        "--output",
        str(OUTPUT_ROOT / "invalid-daw"),
        expect_success=False,
    )
    assert "Supported DAW targets are:" in result.stderr

    result = run(
        "preflight",
        "--project",
        str(FIXTURE),
        "--daw",
        "cubase",
        "--target-os",
        "unknown-os",
        "--output",
        str(OUTPUT_ROOT / "invalid-platform"),
        expect_success=False,
    )
    assert "Supported target operating systems" in result.stderr


def validate_help_commands() -> None:
    help_result = run("--help")
    assert "Usage:" in help_result.stdout
    assert "ubridge preflight" in help_result.stdout

    preflight_help = run("preflight", "--help")
    assert "Usage:" in preflight_help.stdout
    assert "--project <folder>" in preflight_help.stdout

    route_help = run("route", "--help")
    assert "Usage:" in route_help.stdout
    assert "ubridge route" in route_help.stdout
    assert "Example live runtime: MPC Sample -> Windows 11 -> Cubase" in route_help.stdout

    devices_help = run("devices", "--help")
    assert "Usage:" in devices_help.stdout
    assert "ubridge devices" in devices_help.stdout

    for platform in ("windows", "macos", "linux", "android", "chromeos", "ipados", "ios"):
        route_result = run("route", "--device", "mpc-sample", "--daw", "cubase", "--target-os", platform)
        assert "Target device: mpc-sample" in route_result.stdout
        assert "Host OS: " + platform in route_result.stdout


def validate_multidaw_and_hardware_profiles() -> None:
    for daw in ("cubase", "reason", "fl-studio", "garageband", "ableton-live"):
        output = OUTPUT_ROOT / f"multidaw-{daw}"
        result = run(
            "preflight",
            "--project",
            str(FIXTURE),
            "--daw",
            daw,
            "--device",
            "audient",
            "--output",
            str(output),
        )
        assert "Preflight completed safely" in result.stdout
        session = json.loads((output / "session.ubridge.json").read_text(encoding="utf-8"))
        assert session["reference_route"]["target_daw"] == daw
        assert session["reference_route"]["device_profile"] == "audient"


def main() -> int:
    if not BINARY.is_file():
        raise SystemExit(f"Expected built binary at {BINARY}. Build with CMake first.")
    shutil.rmtree(OUTPUT_ROOT, ignore_errors=True)
    validate_exchange("cubase")
    validate_exchange("reason")
    validate_platform_contract("windows", "reference_desktop", True, True)
    validate_platform_contract("macos", "portable_core_target", True, False)
    validate_platform_contract("linux", "portable_core_target", True, False)
    validate_platform_contract("android", "future_mobile_host", False, False)
    validate_platform_contract("chromeos", "future_mobile_host", False, False)
    validate_platform_contract("ipados", "future_mobile_host", False, False)
    validate_platform_contract("ios", "future_mobile_host", False, False)
    validate_safety_guards()
    validate_help_commands()
    validate_multidaw_and_hardware_profiles()
    print("All Universal Bridge prototype validation checks passed.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

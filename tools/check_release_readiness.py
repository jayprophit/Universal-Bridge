#!/usr/bin/env python3
"""Validate release-gate accounting and optionally enforce a release decision."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "release" / "release-gates.json"
EXPECTED_IDS = {f"RG-{index:03d}" for index in range(1, 11)}
ALLOWED_STATES = {"ready", "partial", "experimental", "blocked_external", "not_started"}


def validate() -> dict:
    document = json.loads(MANIFEST.read_text(encoding="utf-8"))
    if document.get("schema_version") != 1:
        raise ValueError("release manifest must use schema_version 1")
    gates = document.get("gates")
    if not isinstance(gates, list) or len(gates) != 10:
        raise ValueError("release manifest must contain exactly ten gates")
    ids = {gate.get("id") for gate in gates}
    if ids != EXPECTED_IDS:
        raise ValueError(f"release gate IDs differ from the required set: {sorted(ids)}")
    for gate in gates:
        missing = {"id", "name", "status", "owner", "evidence", "exit_criteria"} - set(gate)
        if missing:
            raise ValueError(f"{gate.get('id', 'unknown')} is missing fields: {sorted(missing)}")
        if gate["status"] not in ALLOWED_STATES:
            raise ValueError(f"{gate['id']} has unknown status {gate['status']}")
        if not gate["owner"] or not gate["exit_criteria"] or not gate["evidence"]:
            raise ValueError(f"{gate['id']} must retain an owner, evidence and exit criteria")
        for relative in gate["evidence"]:
            path = (ROOT / relative).resolve()
            if not path.is_relative_to(ROOT.resolve()) or not path.is_file():
                raise ValueError(f"{gate['id']} evidence path is missing or unsafe: {relative}")
    return document


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="validate structure without authorizing a release")
    parser.add_argument("--release", action="store_true", help="fail unless every release gate is ready")
    arguments = parser.parse_args()
    if not arguments.check and not arguments.release:
        parser.error("choose --check or --release")
    document = validate()
    states = {gate["id"]: gate["status"] for gate in document["gates"]}
    print("validated release gates: " + ", ".join(f"{key}={value}" for key, value in states.items()))
    if arguments.release:
        blocked = [key for key, value in states.items() if value != "ready"]
        if blocked:
            print("commercial release blocked by: " + ", ".join(blocked))
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

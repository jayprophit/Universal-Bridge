#!/usr/bin/env python3
"""Validate the reviewable Poietek intake and build a local SQLite search index."""

from __future__ import annotations

import argparse
from contextlib import closing
import json
import sqlite3
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SOURCE_INDEX = ROOT / "research" / "poietek-project" / "sources.json"
DEFAULT_REQUIREMENTS = ROOT / "research" / "poietek-project" / "requirements.jsonl"
DEFAULT_SCHEMA = ROOT / "research" / "poietek-project" / "schema.sql"
DEFAULT_OUTPUT = ROOT / "build" / "research" / "poietek-catalog.sqlite3"

REQUIRED_REQUIREMENT_FIELDS = {
    "id",
    "title",
    "area",
    "delivery",
    "maturity",
    "source_ids",
    "accepted_boundary",
    "evidence_gate",
}


def load_inputs(source_index: Path, requirements_path: Path) -> tuple[dict, list[dict]]:
    source_document = json.loads(source_index.read_text(encoding="utf-8"))
    if source_document.get("schema_version") != 1:
        raise ValueError("sources.json must use schema_version 1")
    sources = source_document.get("sources")
    if not isinstance(sources, list) or not sources:
        raise ValueError("sources.json must contain at least one source")

    requirements: list[dict] = []
    for line_number, line in enumerate(requirements_path.read_text(encoding="utf-8").splitlines(), start=1):
        if not line.strip():
            continue
        try:
            requirements.append(json.loads(line))
        except json.JSONDecodeError as error:
            raise ValueError(f"requirements.jsonl line {line_number}: {error}") from error
    if not requirements:
        raise ValueError("requirements.jsonl must contain at least one requirement")
    return source_document, requirements


def validate(source_document: dict, requirements: list[dict]) -> None:
    source_ids: set[str] = set()
    for source in source_document["sources"]:
        missing = {"id", "kind", "title", "turn_count", "scope"} - set(source)
        if missing:
            raise ValueError(f"source is missing fields: {sorted(missing)}")
        if source["id"] in source_ids:
            raise ValueError(f"duplicate source id: {source['id']}")
        if not isinstance(source["scope"], list) or not source["scope"]:
            raise ValueError(f"source {source['id']} must have a non-empty scope")
        source_ids.add(source["id"])

    requirement_ids: set[str] = set()
    for requirement in requirements:
        missing = REQUIRED_REQUIREMENT_FIELDS - set(requirement)
        if missing:
            raise ValueError(f"requirement is missing fields: {sorted(missing)}")
        if requirement["id"] in requirement_ids:
            raise ValueError(f"duplicate requirement id: {requirement['id']}")
        unknown_sources = set(requirement["source_ids"]) - source_ids
        if unknown_sources:
            raise ValueError(f"requirement {requirement['id']} references unknown sources: {sorted(unknown_sources)}")
        if not requirement["accepted_boundary"] or not requirement["evidence_gate"]:
            raise ValueError(f"requirement {requirement['id']} must state its boundary and evidence gate")
        requirement_ids.add(requirement["id"])


def build_database(output: Path, schema_path: Path, source_document: dict, requirements: list[dict]) -> None:
    output.parent.mkdir(parents=True, exist_ok=True)
    output.unlink(missing_ok=True)
    with closing(sqlite3.connect(output)) as database:
        database.executescript(schema_path.read_text(encoding="utf-8"))
        database.executemany(
            "INSERT INTO catalog_metadata(key, value) VALUES (?, ?)",
            [
                ("schema_version", str(source_document["schema_version"])),
                ("project", source_document["project"]),
                ("captured_on", source_document["captured_on"]),
                ("retention_policy", source_document["retention_policy"]),
                ("raw_conversations_committed", json.dumps(source_document["raw_conversations_committed"])),
            ],
        )
        database.executemany(
            "INSERT INTO source(id, kind, title, turn_count, scope_json) VALUES (?, ?, ?, ?, ?)",
            [
                (source["id"], source["kind"], source["title"], source["turn_count"], json.dumps(source["scope"], sort_keys=True))
                for source in source_document["sources"]
            ],
        )
        for requirement in requirements:
            database.execute(
                "INSERT INTO requirement(id, title, area, delivery, maturity, accepted_boundary, evidence_gate) VALUES (?, ?, ?, ?, ?, ?, ?)",
                (
                    requirement["id"],
                    requirement["title"],
                    requirement["area"],
                    requirement["delivery"],
                    requirement["maturity"],
                    requirement["accepted_boundary"],
                    requirement["evidence_gate"],
                ),
            )
            database.executemany(
                "INSERT INTO requirement_source(requirement_id, source_id) VALUES (?, ?)",
                [(requirement["id"], source_id) for source_id in requirement["source_ids"]],
            )
        database.commit()

        source_count = database.execute("SELECT COUNT(*) FROM source").fetchone()[0]
        requirement_count = database.execute("SELECT COUNT(*) FROM requirement").fetchone()[0]
        link_count = database.execute("SELECT COUNT(*) FROM requirement_source").fetchone()[0]
        if source_count != len(source_document["sources"]) or requirement_count != len(requirements) or link_count == 0:
            raise RuntimeError("database count verification failed")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-index", type=Path, default=DEFAULT_SOURCE_INDEX)
    parser.add_argument("--requirements", type=Path, default=DEFAULT_REQUIREMENTS)
    parser.add_argument("--schema", type=Path, default=DEFAULT_SCHEMA)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--check", action="store_true", help="validate and build a disposable database")
    arguments = parser.parse_args()

    source_document, requirements = load_inputs(arguments.source_index, arguments.requirements)
    validate(source_document, requirements)
    if arguments.check:
        with tempfile.TemporaryDirectory(prefix="ubridge-research-") as temporary:
            build_database(Path(temporary) / "catalog.sqlite3", arguments.schema, source_document, requirements)
    else:
        build_database(arguments.output, arguments.schema, source_document, requirements)
        print(arguments.output)
    print(f"validated {len(source_document['sources'])} sources and {len(requirements)} requirements")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

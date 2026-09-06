# Research intake catalog

This directory contains normalized, non-sensitive abstractions of external research inputs. It does **not** contain raw ChatGPT transcripts, account exports, copyrighted competitor assets, credentials, private URLs, or proof that a requested capability has been implemented.

The Poietek Project intake is stored as reviewable JSON/JSONL and can be indexed locally with Python's standard-library SQLite module:

```powershell
python tools/build_research_catalog.py --check
python tools/build_research_catalog.py
```

The generated database is written below `build/research/` and is intentionally excluded from Git. It is a developer search/index artifact, not runtime product state. Universal Bridge must never query this database from an audio callback, MIDI callback, device driver path, or DAW real-time thread.

Canonical product requirements remain in `docs/MASTER_REQUIREMENTS_MATRIX.md`. Research records may propose additions or clarify acceptance evidence, but they do not silently renumber, delete, or complete an existing requirement.

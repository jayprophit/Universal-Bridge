PRAGMA foreign_keys = ON;

CREATE TABLE catalog_metadata (
    key TEXT PRIMARY KEY,
    value TEXT NOT NULL
);

CREATE TABLE source (
    id TEXT PRIMARY KEY,
    kind TEXT NOT NULL,
    title TEXT NOT NULL,
    turn_count INTEGER NOT NULL CHECK (turn_count >= 0),
    scope_json TEXT NOT NULL
);

CREATE TABLE requirement (
    id TEXT PRIMARY KEY,
    title TEXT NOT NULL,
    area TEXT NOT NULL,
    delivery TEXT NOT NULL,
    maturity TEXT NOT NULL,
    accepted_boundary TEXT NOT NULL,
    evidence_gate TEXT NOT NULL
);

CREATE TABLE requirement_source (
    requirement_id TEXT NOT NULL REFERENCES requirement(id) ON DELETE CASCADE,
    source_id TEXT NOT NULL REFERENCES source(id) ON DELETE RESTRICT,
    PRIMARY KEY (requirement_id, source_id)
);

CREATE INDEX requirement_area_idx ON requirement(area);
CREATE INDEX requirement_maturity_idx ON requirement(maturity);

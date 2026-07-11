-- Faraday SQLite schema (reference copy; the application creates this
-- schema itself via Database::initSchema()).

CREATE TABLE IF NOT EXISTS battery_static (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    instance_name       TEXT NOT NULL UNIQUE,
    name                TEXT,
    manufacturer        TEXT,
    serial_number       TEXT,
    chemistry           TEXT,
    design_capacity_mwh INTEGER,
    design_voltage_mv   INTEGER,
    first_seen_ms       INTEGER NOT NULL,
    last_seen_ms        INTEGER NOT NULL
);

CREATE TABLE IF NOT EXISTS samples (
    id                 INTEGER PRIMARY KEY AUTOINCREMENT,
    ts_ms              INTEGER NOT NULL,
    instance_name      TEXT NOT NULL,
    remaining_mwh      INTEGER,
    full_charged_mwh   INTEGER,
    charge_rate_mw     INTEGER,
    discharge_rate_mw  INTEGER,
    voltage_mv         INTEGER,
    percent            REAL,
    temperature_c      REAL,
    power_online       INTEGER,
    charging           INTEGER,
    discharging        INTEGER,
    cycle_count        INTEGER
);
CREATE INDEX IF NOT EXISTS idx_samples_ts ON samples(ts_ms);
CREATE INDEX IF NOT EXISTS idx_samples_instance ON samples(instance_name, ts_ms);

CREATE TABLE IF NOT EXISTS capacity_history (
    id               INTEGER PRIMARY KEY AUTOINCREMENT,
    start_date       TEXT NOT NULL,
    end_date         TEXT,
    design_mwh       INTEGER,
    full_charge_mwh  INTEGER,
    cycle_count      INTEGER,
    source           TEXT NOT NULL,
    UNIQUE(start_date, source)
);

CREATE TABLE IF NOT EXISTS sessions (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    started_ms           INTEGER NOT NULL,
    ended_ms             INTEGER,
    origin_remaining_mwh INTEGER,
    note                 TEXT
);

-- Database metadata only (schema version etc.).
-- User preferences live in the app-local settings.json, never here
-- and never in the registry.
CREATE TABLE IF NOT EXISTS settings (
    key   TEXT PRIMARY KEY,
    value TEXT
);

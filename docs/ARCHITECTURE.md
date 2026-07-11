# Architecture

## Layer map

```
┌─────────────────────────────────────────────────────────────┐
│ UI (QML, ui/)                                               │
│  Main.qml — shell, sidebar, theme singleton (Theme.qml)     │
│  pages/Dashboard·Monitor·History·Alerts·Calibration·Settings│
│  components/HealthRing, MetricCard (Canvas-based charts)    │
├─────────────────────────────────────────────────────────────┤
│ Application layer (src/app/)                                │
│  BatteryModel   — the single QML bridge (context property)  │
│  Sampler        — worker-thread acquisition loop            │
│  AlertManager   — latched threshold evaluation              │
│  TrayManager    — tray icon, menu, balloon notifications    │
│  Autostart      — Startup-folder shortcut (no registry)     │
│  ChargeCap      — vendor firmware charge-cap probe          │
│  Calibration    — guided conditioning state machine         │
│  Exporter       — CSV / JSON / self-contained HTML report   │
├─────────────────────────────────────────────────────────────┤
│ Core (src/core/)                                            │
│  Metrics        — pure formulas (health, wear, regression…) │
│  HealthVerdict  — rule-based plain-language verdicts        │
├─────────────────────────────────────────────────────────────┤
│ Data (src/data/)                                            │
│  Database       — SQLite via Qt SQL (QSQLITE driver)        │
│  Settings       — JSON file store (never QSettings/registry)│
├─────────────────────────────────────────────────────────────┤
│ Acquisition (src/acquisition/)                              │
│  WmiClient      — COM/WBEM query client (read-only WQL)     │
│  BatteryReader  — merges ROOT\WMI + ROOT\CIMV2 + thermal    │
│  PowercfgReport — runs & parses powercfg /batteryreport /xml│
│  BatteryTypes   — BatterySnapshot / BatteryDevice structs   │
└─────────────────────────────────────────────────────────────┘
```

Everything below the application layer is a plain C++ library (`faradaylib`)
with no QML dependency, linked by both the app and every test binary.

## Threading model

- **UI thread** — QML engine, `BatteryModel`, `TrayManager`, SQLite writes
  (sub-millisecond inserts), alert evaluation.
- **Worker thread** (`faraday-sampler`) — owns `Sampler`, which creates its
  `BatteryReader` *inside* `start()` so the COM apartment belongs to the worker.
  A `QTimer` (VeryCoarse, battery-friendly) fires `sampleNow()`; each snapshot
  crosses to the UI thread through a queued signal carrying a value-type
  `BatterySnapshot` (registered metatype). The UI never blocks on WMI.
- **Thread pool one-shots** — the initial `powercfg` run/parse and the
  charge-cap probe each execute once on `QThreadPool` and marshal results back
  with `QMetaObject::invokeMethod(..., QueuedConnection)` guarded by `QPointer`.

## Data flow

```
 ROOT\WMI ─┐                            ┌→ SQLite (samples, battery_static)
 ROOT\CIMV2├→ BatteryReader → snapshot ─┤→ live series (session buffer)
 MsAcpi ───┘        (worker)            ├→ AlertManager → tray balloons
                                        └→ QML properties (dashboard etc.)

 powercfg /batteryreport /xml → PowercfgReport → capacity_history (SQLite,
     deduplicated by start-date+source) + usage log + habit insights +
     expected-drain model for the live monitor comparison line
```

Every hardware field is `std::optional`; a missing class, a blocked property
(e.g. `BatteryStaticData` without elevation) or a sentinel value (e.g.
`EstimatedRuntime == 0xFFFFFFFF`) becomes "absent", never a fake number, and the
failure reason is surfaced in `BatterySnapshot::unavailable`.

## Persistence

SQLite schema (see `data/schema.sql`): `battery_static`, `samples`,
`capacity_history` (unique on start-date+source so re-ingests are idempotent),
`sessions`, and a `settings` table reserved for DB metadata (schema version).
User preferences live in `settings.json` next to the database.

## Error-handling policy

No exceptions cross module boundaries. COM/WMI calls check every HRESULT and
report through `lastError()`. Database and file operations return success flags
with error strings. The acquisition layer treats "class exists but enumeration
fails" (a real occurrence for `BatteryStaticData`) separately from "class has
zero instances" (a desktop with no battery).

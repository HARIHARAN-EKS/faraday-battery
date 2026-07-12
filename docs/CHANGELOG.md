# Changelog

All notable changes to Faraday are documented here.
Format: [Keep a Changelog](https://keepachangelog.com); versioning: SemVer.

## [1.0.2] — 2026-07-12

AV false-positive response round, driven by measured VirusTotal results
for 1.0.1: application exe **0/70 (clean)**, all 102 installed files
**0 detections**, installer wrapper 1/69 (Trapmine
`Malicious.moderate.ml.score` — a generic ML bucket, no named family).
Baseline with hashes: docs/VIRUSTOTAL_BASELINE.md.

### Added
- **Portable mode** — a `portable.txt` marker beside `faraday.exe` (shipped
  in the ZIP) keeps all settings and history in an app-local `data` folder,
  so the app runs from any path — USB drives and folders with spaces
  included — with zero install and zero registry/AppData footprint.
  Defaults now persist on first run so the folder is complete immediately.
  The ZIP is the documented, recommended channel for anyone whose AV
  objects to the installer (docs/PORTABLE.md); 16th test suite covers it.
- **docs/VIRUSTOTAL_BASELINE.md** — verbatim scan baseline (hashes, vendor,
  signature name, PE metadata, comparison protocol).
- **docs/FALSE_POSITIVE_RESPONSE.md** — factual page for users and vendors
  with hashes, links and FP-reporting instructions.

### Changed
- **Installer heuristic-surface reduction** (hygiene only — no packing, no
  evasion, still unsigned and says so): Modern UI 2 standard page flow;
  full VersionInfo now including `OriginalFilename`/`InternalName`;
  `ManifestDPIAware` + `ManifestSupportedOS all`; branding text; richer
  standard HKCU uninstall metadata (`QuietUninstallString`,
  `EstimatedSize`); confirmed zero `Exec`/`ExecShell` anywhere; stock
  NSIS 3.12 stubs with standard `/SOLID lzma`.

## [1.0.1] — 2026-07-12

Verification & hardening round.

### Fixed
- **`BatteryStaticData` 0x80041001 root-caused** — the failure is a
  transient WMI performance-adapter hiccup, *not* an elevation requirement
  (the legacy DCOM path reads the class unelevated). The WMI client now
  retries via `CreateInstanceEnum`, and the reader caches the last good
  static rows per boot, so pack identity survives transient failures.
- **Design-capacity precedence corrected** — powercfg now wins over
  `BatteryStaticData` and `Win32_PortableBattery` (the three disagree on
  real hardware: 42401 / 42581 / 41040 mWh), resolved per pack.
- **Windowless-zombie failsafe** — when the QML runtime cannot come up the
  engine yields zero root objects without `objectCreationFailed`; the app
  now exits with code 1 instead of lingering as a background process.

### Added
- **Battery details panel** — serial number, manufacturer, manufacture
  date, chemistry, unique ID and design capacity, each with a source tag
  resolved through a documented per-field precedence chain; unresolvable
  fields honestly render "Not reported by this hardware".
- **Chemistry decoding** — `BatteryStaticData.Chemistry` FourCC and
  powercfg tokens ("LIon", "LiP", …) normalize to one display vocabulary;
  `ManufactureDate` CIM datetimes are parsed (all-wildcard = not reported).
- **Temperature honesty** — system-zone estimates are unmistakably labeled
  (card title, sub-label, tooltip, source text); the high-temperature alert
  only arms against a true `*BAT*` thermal sensor and is visibly disabled
  with the reason on estimate-only machines.
- **Synthetic degradation harness** (test-only) — 15th suite driving wear
  tiers 0/15/30/40/70 %, amber/red ring states, verdict text per tier,
  degradation regression + EOL projection, calibration-drift trip/no-trip,
  per-state drain, degraded multi-pack aggregation and ghost-VM paths.

### Changed
- **Distribution trimmed** — TLS backends, network-information plugin, QML
  debug transports (incl. the TCP one), TUIO touch plugin and unused SQL
  drivers no longer ship. `Qt6Network.dll` removal was attempted and proven
  impossible (hard static import of `Qt6Qml.dll`/`Qt6Quick.dll`; the loader
  faults pre-`main`) — documented in AV_HARDENING.md; no deployed code
  calls it anymore.

## [1.0.0] — 2026-07-12

First release.

### Added
- **Acquisition layer** — read-only WMI readers for `BatteryStaticData`,
  `BatteryFullChargedCapacity`, `BatteryCycleCount`, `BatteryStatus`,
  `BatteryRuntime`, `MsAcpi_ThermalZoneTemperature`, `Win32_Battery` and
  `Win32_PortableBattery`, with per-field fallbacks, sentinel filtering,
  multi-battery enumeration and VM/no-battery detection; `powercfg
  /batteryreport /xml` runner and parser (design/full-charge capacity, cycles,
  weekly capacity history, usage log).
- **Metrics engine** — health %, wear %, net power, V·I helper, time-to-full,
  instantaneous + trend-extrapolated time-to-empty, least-squares regression,
  degradation curve, end-of-life projection, calibration drift, per-state
  drain; rule-based plain-language health verdicts and charging-habit insights.
- **Persistence** — SQLite (via Qt SQL) schema `battery_static`, `samples`,
  `capacity_history`, `sessions`, `settings`; idempotent powercfg history
  ingest; JSON settings file (no registry); worker-thread sampling loop with
  configurable interval.
- **Dashboard** — color-coded health ring, live status, metric cards
  (capacity, health, wear, cycles, temperature, voltage, power, sampling),
  verdict + insights panel, multi-battery list, advanced raw-sensor drawer
  with per-source unavailability reporting, dark & light themes.
- **Live monitor** — real-time charge graph, extrapolated trend line,
  expected-discharge comparison from usage history, critical-draw markers,
  interval/zoom/session-origin controls.
- **History** — capacity-decline chart vs design, cycle-count chart,
  powercfg usage log, date-range zoom, degradation trend + EOL readout.
- **Alerts** — latched thresholds (level/temperature/voltage), unplug-at-full
  and charge-below reminders, tray notifications, minimize-to-tray, opt-in
  Startup-folder autostart, vendor charge-cap section (auto-hidden when the
  firmware exposes none).
- **Calibration & reports** — gauge-drift detection, guided 4-step
  conditioning workflow, CSV/JSON sample export, self-contained HTML health
  report with inline SVG chart.
- **Settings & i18n** — theme, °C/°F, mWh/Wh, sample interval, data-folder
  access; Qt Linguist scaffold with the full English catalog embedded.
- **Release engineering** — version-info resource, application icon,
  `asInvoker` manifest, per-user NSIS installer with working uninstaller,
  portable ZIP, AV-hardening verification (no packer, no network imports,
  no registry writes, never elevated).

### Test coverage
14 QtTest suites / 100+ assertions gate every phase: powercfg parsing, WMI
client failure modes, reader helpers, every metric formula, verdict rules,
database, settings, sampler threading, model aggregation, alerts latching,
autostart, exporter (CSV/JSON/HTML self-containment) and the calibration
state machine.

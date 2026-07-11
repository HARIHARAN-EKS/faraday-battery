# Changelog

All notable changes to Faraday are documented here.
Format: [Keep a Changelog](https://keepachangelog.com); versioning: SemVer.

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

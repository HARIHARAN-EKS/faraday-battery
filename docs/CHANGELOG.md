# Changelog

All notable changes to Faraday are documented here.
Format: [Keep a Changelog](https://keepachangelog.com); versioning: SemVer.

## [1.0.4] â€” 2026-07-12

Field-failure release: a bare `faraday.exe` run outside its runtime folder
produced the raw Windows loader dialog ("Qt6Gui.dll was not found") â€” a
failure no code inside that process could catch (static PE imports resolve
before `main()`). Packaging itself was verified complete (D1).

### Added
- **Launcher stub.** `faraday.exe` is now a 63 KB statically linked
  Win32 stub (imports: KERNEL32, USER32, msvcrt only â€” starts anywhere).
  It validates the runtime against the packaging-generated
  `runtime.manifest`, then starts the real application
  (`faraday-app.exe`) with loader error boxes suppressed and watches its
  first seconds. Every failure mode ends in a clear "extract the ENTIRE
  archive / reinstall" message â€” never a raw loader dialog. No packing,
  no network, no registry, asInvoker, full VersionInfo on both binaries.
- **runtime.manifest** generated at packaging time (every runtime file,
  129 entries) and shipped in both the ZIP and the installer.
- **ZIP root README.txt** with explicit extract-the-whole-folder
  instructions; archive still extracts to a single top-level `Faraday\`.
- **Installer payload verification**: aborts loudly if any critical file
  failed to land; shortcuts explicitly start in the install directory.
- 26th suite `test_launcher` (139 cases): bare exe, each of the 129
  manifest entries deleted in turn, missing platforms/QML directories,
  truncated DLL, foreign working directory (â‰¡ wrong-"Start in" shortcut),
  spaced paths â€” plus scripted Explorer-extraction and non-system-drive
  verifications.

## [1.0.3] â€” 2026-07-12

Field-defect release: the first deployment on a second machine (MSI
laptop, genuinely worn battery) surfaced real defects, all fixed here.
Ships together with the deep-verification round's hostile-input fixes
below. Field record: docs/FIELD_TESTING.md.

### Fixed (field defects, each with a proving test)
- **F1 (high): cycle-count sentinel treated as a measured zero.** Firmware
  that does not track cycles reports 0 through `BatteryCycleCount`/
  powercfg; 1.0.2 rendered "0 cycles" and reasoned *"still early in its
  life"* against a battery it simultaneously measured at 33 % wear. Zero
  is now a sentinel end to end (reader, model, history chart, HTML
  report â†’ "Not reported by this hardware"), the verdict engine makes no
  cycle-derived claim without a measured non-zero count, and low-measured-
  cycles wording switches to calendar-aging phrasing on packs below 80 %
  health â€” the self-contradiction class is structurally impossible now.
  The audit sweep (docs/METRICS.md sentinel table) also caught voltage-0
  firmware stubs rendering as "0.00 V"; same fix.
- **F2 (medium):** the dashboard header's unlabeled bar (charge) sat next
  to the health ring with no distinction. Ring now captioned "BATTERY
  HEALTH", bar captioned "CURRENT CHARGE" with its own value, and the bar
  no longer borrows the health-grade color.
- **F3 (low):** raw ACPI object identifiers ("BIF0_9") were presented as
  the device name. Internal identifiers are demoted from the identity
  panel (honest "Not reported by this hardware"); raw values stay in the
  Advanced drawer, where the Unique ID row now lives permanently.
- **F4 (info):** recorded â€” neither field machine exposes a battery
  thermal zone; the temperature alert is effectively unavailable on most
  consumer hardware, and its greyed/disabled presentation is the correct
  behavior (confirmed in the field).

## [Deep-verification round] â€” 2026-07-12 (ships in 1.0.3)

Full test/performance/reliability/security audit of the shipped 1.0.2;
these changes were held in source and ship now with 1.0.3.

### Fixed (each with a proving test)
- `metrics::linearRegression` returned `valid=true` with a NaN slope when
  any input point was non-finite; now rejects non-finite points.
- `metrics::degradationCurve` let NaN capacities through its `<= 0` guard
  into the regression; now filters non-finite values explicitly.
- `metrics::endOfLifeProjection` reached undefined behavior
  (`static_cast<qint64>(NaN)` in `addDays`) for non-finite design/threshold
  inputs; now guarded.

### Added
- 8 new test suites (24 total, **3593 real cases**): metrics property/
  boundary suite (1816 generated cases with invariant gates), powercfg XML
  fuzzing (XXE/entity-bomb inert, byte mutations, truncations, 5 MB
  reports), WMI row fuzzing via new BatteryReader row-application seams,
  settings/SQLite abuse, clock-jump handling, performance benchmarks with
  regression gates, model-path coverage incl. forced AC-transition cycle,
  TrayManager coverage. Line coverage 77 % â†’ 86 %, branch 46 % â†’ 54 %.
- `Database::insertSamplesBulk` â€” transaction-wrapped bulk insert
  (33.8k rows/s vs 158/s autocommit), rollback-safe.
- Engineering records: tests/COVERAGE_MAP.md, tests/PERF_RESULTS.md,
  docs/SECURITY_AUDIT.md, docs/RELIABILITY_RESULTS.md.

## [1.0.2] â€” 2026-07-12

AV false-positive response round, driven by measured VirusTotal results
for 1.0.1: application exe **0/70 (clean)**, all 102 installed files
**0 detections**, installer wrapper 1/69 (Trapmine
`Malicious.moderate.ml.score` â€” a generic ML bucket, no named family).
Baseline with hashes: docs/VIRUSTOTAL_BASELINE.md.

### Release close-out (measured 1.0.2 scans + audit)
- VirusTotal, 1.0.2 artifacts: application exe **0/70 (stayed clean â€”
  Trapmine itself reports Undetected)**; portable ZIP **0 detections**
  (whole archive and every bundled file); installer **1/68** â€” the same
  single Trapmine ML-bucket hit, unmoved by the full hygiene pass, which
  empirically confirms the unsigned-stub structural limit.
- Ship-readiness audit (docs/SHIP_READINESS.md): 16/16 suites; automated
  click-through of all six screens on both the release and portable
  builds with zero QML errors/warnings; live-value and
  honest-degradation states verified intact. **Shipped as-is; binaries
  unchanged so the scanned hashes remain authoritative.**
- Docs: two-round comparison in VIRUSTOTAL_BASELINE.md; ready-to-send FP
  report in FP_SUBMISSIONS/Trapmine.md; README now leads with the
  portable ZIP as the primary download.

### Added
- **Portable mode** â€” a `portable.txt` marker beside `faraday.exe` (shipped
  in the ZIP) keeps all settings and history in an app-local `data` folder,
  so the app runs from any path â€” USB drives and folders with spaces
  included â€” with zero install and zero registry/AppData footprint.
  Defaults now persist on first run so the folder is complete immediately.
  The ZIP is the documented, recommended channel for anyone whose AV
  objects to the installer (docs/PORTABLE.md); 16th test suite covers it.
- **docs/VIRUSTOTAL_BASELINE.md** â€” verbatim scan baseline (hashes, vendor,
  signature name, PE metadata, comparison protocol).
- **docs/FALSE_POSITIVE_RESPONSE.md** â€” factual page for users and vendors
  with hashes, links and FP-reporting instructions.

### Changed
- **Installer heuristic-surface reduction** (hygiene only â€” no packing, no
  evasion, still unsigned and says so): Modern UI 2 standard page flow;
  full VersionInfo now including `OriginalFilename`/`InternalName`;
  `ManifestDPIAware` + `ManifestSupportedOS all`; branding text; richer
  standard HKCU uninstall metadata (`QuietUninstallString`,
  `EstimatedSize`); confirmed zero `Exec`/`ExecShell` anywhere; stock
  NSIS 3.12 stubs with standard `/SOLID lzma`.

## [1.0.1] â€” 2026-07-12

Verification & hardening round.

### Fixed
- **`BatteryStaticData` 0x80041001 root-caused** â€” the failure is a
  transient WMI performance-adapter hiccup, *not* an elevation requirement
  (the legacy DCOM path reads the class unelevated). The WMI client now
  retries via `CreateInstanceEnum`, and the reader caches the last good
  static rows per boot, so pack identity survives transient failures.
- **Design-capacity precedence corrected** â€” powercfg now wins over
  `BatteryStaticData` and `Win32_PortableBattery` (the three disagree on
  real hardware: 42401 / 42581 / 41040 mWh), resolved per pack.
- **Windowless-zombie failsafe** â€” when the QML runtime cannot come up the
  engine yields zero root objects without `objectCreationFailed`; the app
  now exits with code 1 instead of lingering as a background process.

### Added
- **Battery details panel** â€” serial number, manufacturer, manufacture
  date, chemistry, unique ID and design capacity, each with a source tag
  resolved through a documented per-field precedence chain; unresolvable
  fields honestly render "Not reported by this hardware".
- **Chemistry decoding** â€” `BatteryStaticData.Chemistry` FourCC and
  powercfg tokens ("LIon", "LiP", â€¦) normalize to one display vocabulary;
  `ManufactureDate` CIM datetimes are parsed (all-wildcard = not reported).
- **Temperature honesty** â€” system-zone estimates are unmistakably labeled
  (card title, sub-label, tooltip, source text); the high-temperature alert
  only arms against a true `*BAT*` thermal sensor and is visibly disabled
  with the reason on estimate-only machines.
- **Synthetic degradation harness** (test-only) â€” 15th suite driving wear
  tiers 0/15/30/40/70 %, amber/red ring states, verdict text per tier,
  degradation regression + EOL projection, calibration-drift trip/no-trip,
  per-state drain, degraded multi-pack aggregation and ghost-VM paths.

### Changed
- **Distribution trimmed** â€” TLS backends, network-information plugin, QML
  debug transports (incl. the TCP one), TUIO touch plugin and unused SQL
  drivers no longer ship. `Qt6Network.dll` removal was attempted and proven
  impossible (hard static import of `Qt6Qml.dll`/`Qt6Quick.dll`; the loader
  faults pre-`main`) â€” documented in AV_HARDENING.md; no deployed code
  calls it anymore.

## [1.0.0] â€” 2026-07-12

First release.

### Added
- **Acquisition layer** â€” read-only WMI readers for `BatteryStaticData`,
  `BatteryFullChargedCapacity`, `BatteryCycleCount`, `BatteryStatus`,
  `BatteryRuntime`, `MsAcpi_ThermalZoneTemperature`, `Win32_Battery` and
  `Win32_PortableBattery`, with per-field fallbacks, sentinel filtering,
  multi-battery enumeration and VM/no-battery detection; `powercfg
  /batteryreport /xml` runner and parser (design/full-charge capacity, cycles,
  weekly capacity history, usage log).
- **Metrics engine** â€” health %, wear %, net power, VÂ·I helper, time-to-full,
  instantaneous + trend-extrapolated time-to-empty, least-squares regression,
  degradation curve, end-of-life projection, calibration drift, per-state
  drain; rule-based plain-language health verdicts and charging-habit insights.
- **Persistence** â€” SQLite (via Qt SQL) schema `battery_static`, `samples`,
  `capacity_history`, `sessions`, `settings`; idempotent powercfg history
  ingest; JSON settings file (no registry); worker-thread sampling loop with
  configurable interval.
- **Dashboard** â€” color-coded health ring, live status, metric cards
  (capacity, health, wear, cycles, temperature, voltage, power, sampling),
  verdict + insights panel, multi-battery list, advanced raw-sensor drawer
  with per-source unavailability reporting, dark & light themes.
- **Live monitor** â€” real-time charge graph, extrapolated trend line,
  expected-discharge comparison from usage history, critical-draw markers,
  interval/zoom/session-origin controls.
- **History** â€” capacity-decline chart vs design, cycle-count chart,
  powercfg usage log, date-range zoom, degradation trend + EOL readout.
- **Alerts** â€” latched thresholds (level/temperature/voltage), unplug-at-full
  and charge-below reminders, tray notifications, minimize-to-tray, opt-in
  Startup-folder autostart, vendor charge-cap section (auto-hidden when the
  firmware exposes none).
- **Calibration & reports** â€” gauge-drift detection, guided 4-step
  conditioning workflow, CSV/JSON sample export, self-contained HTML health
  report with inline SVG chart.
- **Settings & i18n** â€” theme, Â°C/Â°F, mWh/Wh, sample interval, data-folder
  access; Qt Linguist scaffold with the full English catalog embedded.
- **Release engineering** â€” version-info resource, application icon,
  `asInvoker` manifest, per-user NSIS installer with working uninstaller,
  portable ZIP, AV-hardening verification (no packer, no network imports,
  no registry writes, never elevated).

### Test coverage
14 QtTest suites / 100+ assertions gate every phase: powercfg parsing, WMI
client failure modes, reader helpers, every metric formula, verdict rules,
database, settings, sampler threading, model aggregation, alerts latching,
autostart, exporter (CSV/JSON/HTML self-containment) and the calibration
state machine.

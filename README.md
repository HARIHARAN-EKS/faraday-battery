# Faraday — Battery Intelligence Suite

Faraday is a free, fully offline Windows application that tells you the truth about your laptop battery: how much capacity it has actually lost, how fast it is degrading, and when it will need replacing. It reads real firmware telemetry through public read-only Windows interfaces — no drivers, no elevation, no network, no telemetry of any kind.

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg)](LICENSE)
![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![Tests](https://img.shields.io/badge/tests-26%20suites%20%2F%203751%20cases-brightgreen)

---

## Screenshots

<!-- Add images here. Suggested set:
     docs/images/dashboard.png     — health ring, metric cards, verdict
     docs/images/monitor.png       — live graph with trend + expected-drain lines
     docs/images/history.png       — capacity-decline chart
     docs/images/calibration.png   — guided conditioning workflow
-->

| Dashboard | Live monitor |
|---|---|
| _screenshot placeholder_ | _screenshot placeholder_ |

| History | Calibration & reports |
|---|---|
| _screenshot placeholder_ | _screenshot placeholder_ |

---

## Download

**Requires Windows 10/11 (x64).**

### 1. Portable ZIP — recommended

**[`Faraday-1.0.5-portable-win64.zip`](../../releases/latest)** — no installer, no admin rights, nothing written outside its own folder. **0 detections on VirusTotal**, whole archive and every file inside it.

```
SHA-256  f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe
```

### 2. Installer — alternative

**[`Faraday-1.0.5-setup-win64.exe`](../../releases/latest)** — per-user install, never elevates, clean uninstaller.

```
SHA-256  8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d
```

Verify any download with `Get-FileHash <file>` in PowerShell before running it.

### How to run

1. **Extract the ENTIRE folder** from the ZIP (right-click → Extract All).
2. Run **`Faraday.exe`** from inside the extracted `Faraday` folder.

That is the only thing to click. The folder looks like this:

```
Faraday\
  Faraday.exe     <- run this
  README.txt
  app\            <- the program's runtime; not meant to be opened
  data\           <- your settings and battery history (created on first run)
```

`Faraday.exe` is a small launcher; the application itself lives in `app\`. If you copy `Faraday.exe` out on its own it cannot work — and it will tell you so in plain language rather than failing with a confusing Windows error. To uninstall the portable version, delete the folder.

---

## Antivirus note — read this, it is honest

Faraday is **unsigned**, because a code-signing certificate costs money this project does not have. That has a measurable, disclosed cost, and here is exactly what it is (VirusTotal, 2026-07-12):

| File | Result | Flagged by |
|---|---|---|
| **`faraday-core.exe`** — the actual application | **0 detections** | — |
| **Portable ZIP** — the recommended download | **0 detections** | — (every bundled file 0/N) |
| `Faraday.exe` — the launcher stub | 1 / 70 | Arctic Wolf: `Unsafe` |
| `Faraday-1.0.5-setup-win64.exe` — the installer | 2 / 69 | Elastic: `Malicious (high Confidence)`; Trapmine: `Suspicious.low.ml.score` |

**Every engine that flags a wrapper clears its contents.** Arctic Wolf reports the application it launches as clean. Elastic and Trapmine report every single file the installer writes to disk — and the identical payload delivered as a ZIP — as clean. All three labels are **generic ML/heuristic scores; none names a malware family.**

Why the wrappers score at all: the launcher is a tiny (130 KB), statically linked binary that starts a child process — the classic silhouette ML models treat as a "dropper", even though its entire job is to check that the runtime is present and then run it. VirusTotal's own sandbox trace confirms the behaviour: **"Network comms: NOT FOUND"**, no dropped files, no persistence, no registry writes.

**VirusTotal permalinks:**
- [Application `faraday-core.exe` — 0 detections](https://www.virustotal.com/gui/file/f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b)
- [Portable ZIP — 0 detections](https://www.virustotal.com/gui/file/f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe)
- [Launcher `Faraday.exe`](https://www.virustotal.com/gui/file/a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1)
- [Installer](https://www.virustotal.com/gui/file/8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d)

If your AV objects, **use the portable ZIP** — it has no installer stub and scans clean end to end. Full analysis, the scan reports themselves ([`VirusTotal/3/`](VirusTotal/3)), and ready-to-send false-positive reports for each vendor are in [`docs/`](docs). Faraday's hardening is documented and verifiable: no packing, no obfuscation, zero network code, `asInvoker` manifest (never elevates), no registry writes by the application, read-only hardware access.

---

## Features

- **Dashboard** — color-coded health ring, live status, and metric cards (capacity, health, wear, cycles, temperature, voltage, power, sampling interval), plus a plain-language verdict and charging-habit insights.
- **Live monitor** — real-time charge graph with an extrapolated trend line, an expected-discharge comparison drawn from your own usage history, critical-draw markers, and interval/zoom/session controls.
- **History** — capacity-decline chart against the design capacity, cycle-count chart, the powercfg usage log, and an end-of-life projection.
- **Alerts** — latched thresholds (low battery, high temperature, low voltage) with hysteresis, unplug-at-full and charge-below reminders, tray notifications, minimize-to-tray, and opt-in autostart via a Startup-folder shortcut (never a registry Run key).
- **Calibration** — fuel-gauge drift detection and a guided 4-step conditioning workflow (charge → rest → discharge → recharge).
- **Reports** — CSV and JSON sample exports, plus a self-contained HTML health report with an inline SVG chart.
- **Settings** — dark/light themes, °C/°F, mWh/Wh, sample interval, data-folder access, i18n scaffold.
- **Advanced raw-sensor drawer** — every raw value Faraday read, with its source class, and an explicit list of what your hardware does *not* report.

**Honest by design.** A value the hardware does not report is shown as *"Not reported by this hardware"* — never as a zero, never guessed, and never fed into the health verdict. If your machine has no battery thermal sensor (most consumer laptops don't), the temperature is labeled a system-zone *estimate* and the temperature alert is disabled with the reason given, rather than firing against a number that isn't your battery's.

---

## Metrics and where they come from

| Metric | Source | Measured or derived |
|---|---|---|
| Design capacity | `powercfg /batteryreport` → `BatteryStaticData` (ROOT\WMI) → `Win32_PortableBattery` | measured (precedence chain; powercfg wins) |
| Full-charge capacity | `BatteryFullChargedCapacity` (ROOT\WMI) → powercfg | measured |
| Remaining capacity | `BatteryStatus` (ROOT\WMI) | measured |
| **Health %** | — | **derived**: `100 × full-charge / design` |
| **Wear %** | — | **derived**: `100 − health` |
| **Charge %** | `BatteryStatus`; falls back to `Win32_Battery.EstimatedChargeRemaining` | derived (coulometric) / measured (fallback) |
| Charge & discharge rate (mW) | `BatteryStatus` | measured |
| **Net power (W)** | — | **derived**: `(charge − discharge) / 1000` |
| Voltage | `BatteryStatus` (zero treated as a firmware stub) | measured |
| Cycle count | `BatteryCycleCount` (ROOT\WMI) → powercfg (**zero = "not reported"**) | measured |
| Firmware runtime estimate | `BatteryRuntime` (0xFFFFFFFF sentinel filtered) | measured |
| **Time to full / empty** | — | **derived** from current rates, capped at 7 days |
| **Time to empty (trend)** | — | **derived**: least-squares fit over the session's samples |
| Temperature | `MsAcpi_ThermalZoneTemperature` (ACPI, via WMI); stub zones filtered | measured — a `*BAT*` zone is a true sensor, otherwise a labeled **system estimate** |
| Chemistry / manufacturer / serial | `BatteryStaticData` → powercfg → `Win32_Battery`/`Win32_PortableBattery` | measured |
| Capacity history | `powercfg /batteryreport` weekly buckets | measured |
| **Degradation curve** | — | **derived**: regression of capacity history (mWh/day) |
| **End-of-life projection** | — | **derived**: date the fit crosses 80 % of design |
| **Calibration drift** | — | **derived**: reported % − coulometric % |
| **Per-state drain** | powercfg usage log | **derived**: mean mW per state (Active/Suspend/…) |
| Status / AC state | `BatteryStatus` flags → `Win32_Battery.BatteryStatus` enum | measured |

Full detail, including the absent-vs-zero policy for every field: [`docs/METRICS.md`](docs/METRICS.md) and [`docs/DATA_SOURCES.md`](docs/DATA_SOURCES.md).

---

## Build from source

**Toolchain:** Qt 6.5+ (Quick, Controls, Sql, Widgets, LinguistTools), CMake ≥ 3.21, Ninja, MinGW-w64 (or MSVC), NSIS 3.x for the installer.

```powershell
# One-time setup (free tooling; no Qt account needed)
winget install Kitware.CMake Ninja-build.Ninja NSIS.NSIS Python.Python.3.12
python -m pip install aqtinstall
python -m aqt install-qt   windows desktop 6.8.2 win64_mingw -O C:\Qt
python -m aqt install-tool windows desktop tools_mingw1310    -O C:\Qt

# Build and run the full test suite
powershell -ExecutionPolicy Bypass -File build.ps1 -Test

# Package the shipped folder tree, the portable ZIP and the installer
powershell -ExecutionPolicy Bypass -File package.ps1 -Zip -Installer
```

MSVC works too (`/W4 /WX` is configured for it). Warnings are errors on both toolchains. Details: [`docs/BUILD.md`](docs/BUILD.md).

---

## Testing

Faraday is verified far past the usual bar for a desktop utility:

- **26 test suites / 3751 real test cases**, all green — including ~1800 property-based and boundary cases with invariant gates, and ~1600 fuzz cases.
- **Fuzzing**: the powercfg XML parser (byte mutations, truncations, XXE and entity-bomb attempts — both inert, 5 MB reports), the WMI row layer (wrong types, garbage, partial rows), the settings loader, and the SQLite layer (corrupt, read-only, locked, missing). **Zero crashes, zero hangs.**
- **4-hour soak** at the default interval: no crashes, no memory leak (asymptotic plateau), exact 30 s sampling cadence, `PRAGMA integrity_check = ok`.
- **20 hard-kill/restart cycles mid-write**: zero database corruptions.
- **Launcher negative matrix**: bare exe, every one of the 129 runtime files deleted in turn, missing plugin/QML folders, truncated DLL, wrong working directory, Explorer extraction, USB drive — **no path produces a raw Windows loader error**.
- **Coverage**: 86 % line, 54 % branch over `src/`.
- Performance: 0.42 % of one CPU core at the default interval; the app's own battery draw is below the pack gauge's noise floor.

Records: [`tests/COVERAGE_MAP.md`](tests/COVERAGE_MAP.md), [`tests/PERF_RESULTS.md`](tests/PERF_RESULTS.md), [`docs/RELIABILITY_RESULTS.md`](docs/RELIABILITY_RESULTS.md), [`docs/SECURITY_AUDIT.md`](docs/SECURITY_AUDIT.md).

---

## Known limitations

Faraday has been field-tested on two real laptops (an HP and an MSI, the latter with a genuinely worn battery). These paths are implemented and unit-tested but **have never run on real hardware**, because I do not have access to it:

- **Lenovo firmware charge-cap** (the `Lenovo_BiosSetting` probe and its UI section) — no Lenovo machine tested. On all other hardware the section is correctly hidden.
- **A true `*BAT*` battery thermal sensor** — 0 of 2 field machines expose one, so the battery-sensor path and the live temperature alert have never fired in the field. Faraday shows a labeled system estimate instead and disables the alert.
- **Multi-pack systems** (2+ batteries) — aggregation is unit-tested with synthetic packs only.
- **Genuinely high-cycle packs (500+)** and the accuracy of end-of-life projection over months of real history.
- **A desktop/VM with no battery** — the no-battery screen is unit-tested, not field-run.

Reports from any of these configurations are very welcome — see [`docs/FIELD_TESTING.md`](docs/FIELD_TESTING.md).

---

## License and author

**MIT** — see [`LICENSE`](LICENSE). Built with Qt 6 (LGPLv3, dynamically linked) and SQLite (public domain).

Created by **E K S Hariharan**.

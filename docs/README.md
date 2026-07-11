# Faraday — Battery Intelligence Suite

Faraday is a free, open, **fully offline** Windows desktop application that turns the
battery telemetry Windows already collects into clear answers: how healthy is the
battery, how fast is it wearing, how long will it really last, and what habits are
shortening its life.

![](../resources/faraday.png)

## Feature overview

- **Dashboard** — large color-coded health ring (green/amber/red), live status,
  metric cards for capacity, cycles, temperature, wear, voltage and watts, a
  plain-language health verdict, charging-habit insights, and an *Advanced*
  drawer exposing every raw sensor stream and every source that is unavailable
  on the current machine.
- **Live monitor** — real-time charge graph with an extrapolated trend line, a
  comparison line built from your historical average drain, critical-draw
  markers, configurable sample interval, zoom and session-origin reset.
- **History & timeline** — capacity-decline curve against design capacity,
  cycle-count history and a usage log stitched from `powercfg`, with date-range
  zoom and an end-of-life projection.
- **Alerts** — thresholds for level, temperature and voltage, unplug-at-full and
  charge-below reminders, delivered as tray notifications with anti-spam latching.
- **Tray & autostart** — minimize-to-tray, live tooltip, and *opt-in*
  launch-with-Windows implemented as a Startup-folder shortcut (never the registry).
- **Calibration & reports** — gauge-drift detection, a guided 4-step conditioning
  workflow, CSV/JSON raw-sample export and a self-contained HTML health report.
- **Settings** — dark/light theme, °C/°F, mWh/Wh, sampling interval; fully
  translation-ready (Qt Linguist).

## Installing

**Installer** — run `Faraday-1.0.0-setup-win64.exe`. It installs per-user into
`%LOCALAPPDATA%\Programs\Faraday` (no admin prompt), creates Start-menu shortcuts
and a working uninstaller.

**Portable** — unzip `Faraday-1.0.0-portable-win64.zip` anywhere and run
`Faraday\faraday.exe`. Nothing is written outside the app-data folder below.

**Data location** — settings (JSON) and the sample database (SQLite) live in
`%APPDATA%\Faraday Project\Faraday`. Faraday never writes to the Windows registry.

## Requirements

- Windows 10 or 11, 64-bit.
- A battery (laptop/tablet). On desktops and most virtual machines Faraday runs
  fine and shows a clean "No battery detected" screen.
- No admin rights, no internet connection — ever.

## Usage notes

- The first health picture appears within seconds; the degradation curve and
  end-of-life projection need the multi-week history Windows keeps, which
  Faraday ingests automatically from `powercfg /batteryreport`.
- Some sensors are firmware-dependent. Anything a machine does not expose is
  listed under *Dashboard → Advanced → Unavailable on this system* rather than
  shown as fake data.
- The firmware charge-cap (80%) section appears only on machines whose firmware
  exposes such a control (e.g. Lenovo BIOS WMI); it is hidden elsewhere.

## Privacy & safety

Faraday makes **zero network calls**, contains no telemetry or update checker,
reads hardware strictly read-only, runs `asInvoker` (never elevated) and writes
no registry keys. See [AV_HARDENING.md](AV_HARDENING.md) for the complete list
of measures and what to do if an antivirus product mis-flags an unsigned build.

## Documentation map

| File | Contents |
|------|----------|
| [ARCHITECTURE.md](ARCHITECTURE.md) | Layers, threading model, data flow |
| [METRICS.md](METRICS.md) | Every metric, its source and its formula |
| [DATA_SOURCES.md](DATA_SOURCES.md) | WMI classes, powercfg, ACPI thermal |
| [BUILD.md](BUILD.md) | Toolchain setup and release steps |
| [AV_HARDENING.md](AV_HARDENING.md) | Antivirus posture and guidance |
| [CHANGELOG.md](CHANGELOG.md) | Version history |

## License

MIT. Built with Qt 6 (LGPLv3, dynamically linked), SQLite (public domain,
via Qt SQL) and NSIS (zlib license, installer only).

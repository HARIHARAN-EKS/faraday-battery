# Ship-readiness audit — 1.0.2 (2026-07-12)

Final engineering audit before release close-out. The shipped binaries are
hash-pinned to the VirusTotal scans (VIRUSTOTAL_BASELINE.md), so this audit
was deliberately non-invasive: verify everything, change nothing unless
broken. Nothing was broken; nothing was changed.

## Test suites

**16 / 16 passing** (`build.ps1 -Test`): acquisition (WMI client incl.
CreateInstanceEnum fallback, reader helpers, powercfg parser), metrics,
verdicts, database, settings, sampler threading, model aggregation +
source precedence, alerts latching + temperature gating, autostart,
exporter, calibration state machine, synthetic degradation harness,
portable mode.

## UI walkthrough (release dist exe AND ZIP-extracted portable build)

Automated click-through of all six screens — Dashboard, Live monitor,
History, Alerts, Calibration, Settings — on both builds, with Qt logging
forced to a captured stderr channel (`QT_FORCE_STDERR_LOGGING=1`):

- Every page visited; window alive and responsive throughout on both runs.
- **Zero QML errors, zero warnings, zero binding failures** in the
  captured logs (a positive control with a bogus `QT_QPA_PLATFORM`
  confirmed the capture channel reports errors when they exist).
- The Advanced raw-streams drawer and all page content instantiate at
  startup (StackLayout children are eager); any creation error would have
  appeared in the captured log. None did.

## Live-value spot check (reference HP machine, on battery)

| Metric | Value at audit | Resolution |
|---|---|---|
| Design capacity | 42,581 mWh | powercfg (BatteryStaticData agrees; stale Win32_PortableBattery outranked) |
| Full charge / health | 42,581 mWh → 100 %, Excellent | BatteryFullChargedCapacity |
| Charge | 93 % reported vs 93.6 % computed → drift 0.6 %, below the 5 % calibration threshold | BatteryStatus + Win32_Battery |
| Power | −14.6 W discharging, 12.69 V | BatteryStatus |
| Cycles | 35 | BatteryCycleCount |
| Identity | Hewlett-Packard · 32872 08/21/2025 · Lithium-ion (FourCC "Lion") | BatteryStaticData |
| Manufacture date | all-wildcard CIM datetime → **"Not reported by this hardware"** | honest-absent state intact |
| Temperature | no `*BAT*` zone; TZ1 stub (0.05 °C) filtered; mean of TZ0/TZ01 shown as **system-zone estimate**; **temperature alert disabled and greyed with reason** | intact |
| Firmware charge cap | Lenovo_BiosSetting absent → section hidden | intact |

## Cosmetic items deliberately not changed

1. **DWARF `.debug_*` sections remain in the release exe.** Stripping
   would shrink the file but (a) changes the scanned hash, invalidating
   the 0/70 VirusTotal result, and (b) debug info is transparency, not
   risk. Revisit only at the next planned rebuild.
2. **Window close hides to tray by default** (`minimizeToTray = true`).
   Intentional, user-toggleable in Alerts → Tray & startup, and announced
   by a one-time tray notification.

## Verdict

**Faraday 1.0.2 is ready to ship as-is.**

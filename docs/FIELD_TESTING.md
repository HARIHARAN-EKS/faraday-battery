# Field testing record

Real-hardware results. Synthetic tests predict; field machines decide.

## Machine 1 — HP reference laptop (development machine)

LiIon, 42,581 mWh design, ~100 % health, 35 cycles. Drove the original
fallback design: `BatteryStaticData` transient 0x80041001 (root-caused to
the WMI perf adapter, not elevation), `BatteryRuntime` 0xFFFFFFFF on AC,
`Win32_PortableBattery` stale design capacity **and** garbage manufacturer
string (`333-17-32-A`), thermal stub zone TZ1 at raw 2732, no `*BAT*`
zone, no Lenovo charge-cap class.

## Machine 2 — MSI laptop (first true field deployment, 1.0.2 portable)

Genuinely worn battery: 52,007 mWh design (correctly resolved from
powercfg while full-charge came from WMI — the precedence chain's first
real cross-machine test), **67 % health / 33 % wear**.

### What real hardware validated (regression-guarded in `test_field_defects`)

Amber ring + "Fair" grade at 67 %; wear card 33.0 %; degradation verdict;
per-field source tags; "Not reported by this hardware" on serial and
manufacture date; temperature honestly "—" with "no usable thermal zone";
charging-habit insights; portable mode itself (ran from an extracted ZIP).

### Defects found → all fixed this round

| # | Defect | Fix |
|---|---|---|
| F1 (HIGH) | `BatteryCycleCount` = 0 (untracked firmware) rendered as a real "0" and generated *"With 0 charge cycles, the battery is still early in its life"* — directly contradicting the measured 33 % wear shown above it | Zero-cycle sentinel policy end to end (reader, model, history chart, HTML report); verdict engine suppresses every cycle-derived claim when cycles are absent/zero, and low-measured-cycles wording switches to calendar-aging phrasing on packs below 80 % health, making the contradiction class structurally impossible. Audit of every other field caught voltage-0 stubs rendering as "0.00 V" — same fix |
| F2 (MED) | Header showed an unlabeled bar (charge, 78.2 %) beside the health ring (67 %) — two quantities, one visual block | Ring captioned "BATTERY HEALTH", bar captioned "CURRENT CHARGE" with its value, bar recolored to the accent (no longer grade-colored) |
| F3 (LOW) | Raw ACPI object ids shown as identity: "Device name: BIF0_9", "Unique ID: MSIBIF0_9" | ACPI-identifier heuristic demotes internal ids from the identity panel ("Not reported by this hardware"); raw values remain in the Advanced drawer (incl. a new `DeviceName (raw)` row); Unique ID moved to the drawer permanently |
| F4 (INFO) | No battery thermal zone — on the second machine out of two | Recorded in METRICS.md: the temperature alert is effectively unavailable on most consumer hardware; the greyed/disabled path is the correct honest behavior and was confirmed working in the field |

## Coverage of real-hardware paths

| Path | Status |
|---|---|
| Transient WMI perf-adapter failure + cache | **field-proven (HP)** |
| powercfg-wins design-capacity precedence | **field-proven (both machines, disagreeing sources on both)** |
| Untracked-cycle firmware | **field-proven (MSI, defect F1 → fixed)** |
| Worn-battery amber/Fair rendering | **field-proven (MSI)** |
| No-thermal-zone honesty | **field-proven (both)** |
| Portable ZIP deployment | **field-proven (MSI)** |

## Still untested for lack of hardware access

- **Lenovo firmware charge-cap** (`Lenovo_BiosSetting` probe + UI section)
  — no Lenovo machine seen yet; covered by row-parsing unit tests only.
- **A true `*BAT*` battery thermal zone** — 0 of 2 machines expose one;
  the battery-sensor-preferred path and the live temperature alert have
  never fired on real hardware (unit-tested only).
- **Multi-pack systems** (2+ batteries) — aggregation is unit-tested with
  synthetic packs; never seen live.
- **A VM / desktop without any battery** — the no-battery screen is
  unit-tested; not yet field-run.
- **Genuinely high-cycle packs (500+)** and EOL-projection accuracy over
  months of real history.

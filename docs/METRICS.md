# Metrics reference

Every metric Faraday shows, where it comes from, and the exact formula.
Units: capacities/energies in mWh, rates in mW, voltage in mV unless noted.
All formulas live in `src/core/Metrics.cpp` and are unit-tested in
`tests/test_metrics.cpp`.

## Health & wear

| Metric | Formula | Sources |
|---|---|---|
| **Health %** | `100 × FullChargedCapacity / DesignedCapacity`, clamped to [0, 100] | FCC: `BatteryFullChargedCapacity` (ROOT\WMI) → powercfg → `Win32_PortableBattery`. Design: **powercfg** → `BatteryStaticData` → `Win32_PortableBattery` |
| **Wear %** | `100 − Health %` | same |
| **Grade** | ≥90 Excellent · ≥80 Good · ≥65 Fair · ≥50 Worn · <50 Replace soon | health % |

Multi-battery systems aggregate: `Σ FCC / Σ design`, with each pack resolved
through its own precedence chain (packs are aligned by ordinal between the
WMI enumeration and the powercfg report).

## Static identity — source-precedence chains

Every field on the dashboard's *Battery details* panel resolves through an
explicit, per-field chain (first source that delivered wins; the winning
source is shown as a tag next to the value). A field no source delivered
renders as **"Not reported by this hardware"** — never blank, zero, or a
guess. Implemented in `BatteryModel::resolvedStaticFor()`; asserted in
`tests/test_battery_model.cpp`.

| Field | Precedence chain | Rationale |
|---|---|---|
| **Design capacity** | **powercfg** → `BatteryStaticData` → `Win32_PortableBattery` | powercfg is the firmware figure Windows itself uses; on the reference machine the three sources disagree (42401 / 42581 / 41040 mWh) and `Win32_PortableBattery` is demonstrably stale |
| **Manufacturer** | `BatteryStaticData` → powercfg → `Win32_PortableBattery` | ACPI static data is authoritative when readable |
| **Serial number** | `BatteryStaticData` → powercfg | identical firmware string via two transports |
| **Manufacture date** | `BatteryStaticData` only | no other source carries it; all-wildcard CIM datetimes count as "not reported" |
| **Chemistry** | `BatteryStaticData` (FourCC, e.g. "Lion") → powercfg token (e.g. "LIon") → CIMV2 enum (1–8) | all three normalize to one display vocabulary (`chemistryFourCCToString` / `normalizeChemistryToken`) |
| **Unique ID** | `BatteryStaticData.UniqueID` → powercfg battery `id` | |
| **Device name** | `BatteryStaticData.DeviceName` → `Win32_Battery.Name` → `Win32_PortableBattery.Name` | |

## Live metrics

| Metric | Formula | Sources |
|---|---|---|
| **Charge %** | `100 × RemainingCapacity / FullChargedCapacity` (clamped); falls back to `Win32_Battery.EstimatedChargeRemaining` | `BatteryStatus` (ROOT\WMI) |
| **Net power (W)** | `(ChargeRate − DischargeRate) / 1000` — positive charging, negative discharging | `BatteryStatus` |
| **V·I power** | `(mV × mA) / 10⁶` W — provided for sources exposing current instead of rate | helper only |
| **Voltage (V)** | `BatteryStatus.Voltage / 1000` | `BatteryStatus` |
| **Temperature (°C)** | `raw/10 − 273.15` (raw is tenths of Kelvin). Zones with raw ≤ 2732 (≈0 °C firmware stubs) or > 120 °C are discarded. A zone named `*BAT*` wins and counts as a true battery sensor; otherwise the mean of valid zones is shown as a *system thermal estimate*, flagged as such in the UI (card title, sub-label, tooltip). **The high-temperature alert only arms against a true battery sensor** — on estimate-only machines the threshold is disabled and greyed with the reason | `MsAcpi_ThermalZoneTemperature` |
| **Cycle count** | direct read; powercfg fallback. **Zero is a sentinel** (untracked-cycles firmware, field defect F1) — treated as "not reported", never rendered, never reasoned from | `BatteryCycleCount` → powercfg |

## Time estimates

| Metric | Formula |
|---|---|
| **Time to full** | `(FCC − Remaining) / ChargeRate × 3600` s; rejected if rate ≤ 0 or estimate > 7 days |
| **Time to empty (instantaneous)** | `Remaining / DischargeRate × 3600` s; same guards |
| **Time to empty (trend)** | Least-squares fit of this session's `(elapsed s, remaining mWh)` discharge samples; `t_empty = −fitted_now / slope`. Needs ≥ 3 samples and a negative slope; preferred over the instantaneous figure when available |
| **Runtime fallback** | `BatteryRuntime.EstimatedRuntime` seconds, ignored when `0xFFFFFFFF` (unknown/on AC), `0`, or > 7 days |

## Regression, degradation, projection

- **Linear regression** — ordinary least squares over `(x, y)`:
  `slope = Σ(dx·dy)/Σ(dx²)`, `intercept = ȳ − slope·x̄`, `r² = Σ(dx·dy)²/(Σdx²·Σdy²)`
  (r² defined as 1 for a perfectly flat series). Requires ≥ 2 distinct x.
- **Degradation curve** — regression of `(days since first history point,
  full-charge mWh)` over the powercfg capacity history; slope is mWh/day.
- **End-of-life projection** — date where the fitted line crosses
  `threshold% × design` (default 80%). Suppressed when the slope is flat or
  positive, when the crossing lies in the past, or beyond 30 years (fit noise).
- **Live trend (monitor)** — regression over the session buffer, filtered to
  points matching the current charging/discharging state, reported in %/h.
- **Expected discharge line** — `−100 × (expected drain W × 1000) / FCC` %/h,
  where expected drain is the energy-weighted average *Active on-battery* drain
  from the powercfg usage log.

## Calibration drift

`drift = EstimatedChargeRemaining(%) − 100 × Remaining/FCC`.
|drift| > 5 percentage points ⇒ conditioning recommended. The guided cycle is:
charge to ≥98% on AC → rest 30 min → discharge on battery to ≤10% → recharge
to ≥99% on AC.

## Per-state drain

powercfg usage entries on battery with positive `Discharge` are grouped by
`EntryType` (Active, Suspend, …):
`avg drain (mW) = Σ discharge mWh / (Σ duration s / 3600)`.
Durations arrive in 100 ns ticks and are converted with `÷ 10⁷`.

## Critical-discharge markers

A sample is marked critical when the firmware sets `BatteryStatus.Critical`, or
when discharging faster than `max(25 W, 2 × expected drain)`.

## Verdict & insights (rule-based, deterministic)

The headline follows the grade. Details are appended for: capacity ratio;
cycle count (**only when measured and > 0**: <100 early / <500 moderate /
≥500 high — and the "<100 = early in its life" wording is replaced by a
calendar-aging statement whenever health < 80 %, so no verdict line can
contradict the capacity line above it); temperature > 45 °C; |drift| > 5%;
monthly capacity loss ≥ 1 mWh; and the EOL projection when one exists.
Habit insights: AC-time share; > 40% of recorded time sitting ≥ 95%
charged while plugged in ⇒ charge-cap suggestion; ≥ 25% of on-battery entries
below 15% ⇒ deep-discharge warning.

## Sentinel audit — absent vs. zero, field by field (F5 sweep)

Policy: a value the hardware did not measure must never render as a number
and never feed the verdict, habit, degradation, EOL or drift logic. The
raw CSV/JSON exports are the one deliberate exception: they carry the
unfiltered source values, because they are telemetry dumps, not
interpretations.

| Field (source) | Absent representation | Zero vs. absent | UI when absent | Excluded downstream? |
|---|---|---|---|---|
| Identity: manufacturer, serial, device name, unique ID (`BatteryStaticData`/powercfg/CIMV2) | missing property / empty string | n/a (strings) | "Not reported by this hardware"; ACPI-style ids (`BIF0_9`) additionally demoted to the Advanced drawer (F3) | yes — display-only |
| Manufacture date (`BatteryStaticData`) | all-wildcard CIM datetime → empty | n/a | "Not reported by this hardware" | yes |
| Chemistry (FourCC / powercfg token / CIMV2 enum) | 0 / non-printable / empty / enum 0 | 0 is invalid in every encoding → absent | "Not reported by this hardware" | yes — display-only |
| Design capacity (powercfg → `BatteryStaticData` → `Win32_PortableBattery`) | missing, or **0 excluded at every precedence step** | 0 mWh design is physically impossible → sentinel | "Not reported"; HEALTH card hides its design sub-line | yes — health/EOL need > 0 |
| Full-charge capacity (`BatteryFullChargedCapacity` → powercfg) | missing, or 0 excluded from sums | 0 = pack gone/stub → sentinel | health ring "Unknown", grey | yes — health/charge %/drift guarded > 0 |
| **Cycle count** (`BatteryCycleCount` → powercfg → history buckets) | missing **or 0 (F1 fix)** | **indistinguishable — known limitation**; policy: 0 = untracked (a genuine factory-zero carries no decision value) | "—" + "not reported by this hardware"; zero points dropped from the history chart; omitted from the HTML report | yes — verdict suppresses all cycle claims |
| Remaining capacity (`BatteryStatus`) | missing | **0 is a legitimate measurement** (drained pack) — stub-vs-real not distinguishable; **known limitation**, impact bounded to a 0 % display | "—" on capacity card | charge % clamps; estimates handle 0 |
| Charge/discharge rate (`BatteryStatus`) | missing | 0 is legitimate (idle) — stub indistinguishable; **known limitation**, impact: "idle" shown | power card "—" when both absent | time estimates reject ≤ 0; drain requires > 0 |
| **Voltage** (`BatteryStatus`) | missing **or 0 (F1-audit fix)** | a running pack cannot measure 0 V → sentinel | "—" | low-voltage alert requires > 0 |
| Design voltage (`Win32_Battery`) | missing / 0 not used numerically | stored only | not rendered | yes |
| Estimated runtime (`BatteryRuntime`) | `0xFFFFFFFF`, 0, or > 7 days → absent | 0 = sentinel by spec | not rendered | yes |
| Reported charge % (`Win32_Battery.EstimatedChargeRemaining`) | missing / outside 0–100 rejected | **0 is legitimate** (nearly dead) — a firmware stubbing 0 would skew drift toward recommending calibration; **known limitation** (advisory impact only) | falls back to computed % | drift requires 0–100 |
| Win32 status enum | missing / unmapped values → empty string | enum 0 unmapped → absent | status falls through to flags → Idle | yes |
| Temperature (`MsAcpi_ThermalZoneTemperature`) | no instances, only stubs (raw ≤ 2732), or > 120 °C | raw 2732 ≡ 0.05 °C = documented stub | "—" + "no usable thermal zone"; estimate labeled when non-battery zones exist; **alert disabled + greyed with reason** | yes — alerts need a true `*BAT*` zone |
| powercfg history/usage numerics | missing attribute → absent | duration ≤ 0 and discharge ≤ 0 excluded from drain; FCC ≤ 0 excluded from degradation; cycle 0 dropped (F1) | chart point simply missing | yes |

**Field observation (F4):** neither field-tested machine (HP reference,
MSI) exposes a battery-specific `*BAT*` thermal zone — the temperature
alert is effectively unavailable on most consumer hardware, by honest
design rather than defect. Faraday shows the labeled system estimate when
system zones exist, "—" when none do, and never invents a temperature.

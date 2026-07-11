# Metrics reference

Every metric Faraday shows, where it comes from, and the exact formula.
Units: capacities/energies in mWh, rates in mW, voltage in mV unless noted.
All formulas live in `src/core/Metrics.cpp` and are unit-tested in
`tests/test_metrics.cpp`.

## Health & wear

| Metric | Formula | Sources |
|---|---|---|
| **Health %** | `100 × FullChargedCapacity / DesignedCapacity`, clamped to [0, 100] | FCC: `BatteryFullChargedCapacity` (ROOT\WMI) → powercfg → `Win32_PortableBattery`. Design: `BatteryStaticData` → powercfg → `Win32_PortableBattery` |
| **Wear %** | `100 − Health %` | same |
| **Grade** | ≥90 Excellent · ≥80 Good · ≥65 Fair · ≥50 Worn · <50 Replace soon | health % |

Multi-battery systems aggregate: `Σ FCC / Σ design`.

## Live metrics

| Metric | Formula | Sources |
|---|---|---|
| **Charge %** | `100 × RemainingCapacity / FullChargedCapacity` (clamped); falls back to `Win32_Battery.EstimatedChargeRemaining` | `BatteryStatus` (ROOT\WMI) |
| **Net power (W)** | `(ChargeRate − DischargeRate) / 1000` — positive charging, negative discharging | `BatteryStatus` |
| **V·I power** | `(mV × mA) / 10⁶` W — provided for sources exposing current instead of rate | helper only |
| **Voltage (V)** | `BatteryStatus.Voltage / 1000` | `BatteryStatus` |
| **Temperature (°C)** | `raw/10 − 273.15` (raw is tenths of Kelvin). Zones with raw ≤ 2732 (≈0 °C firmware stubs) or > 120 °C are discarded. A zone named `*BAT*` wins; otherwise the mean of valid zones is shown as a *system thermal estimate* | `MsAcpi_ThermalZoneTemperature` |
| **Cycle count** | direct read; powercfg fallback | `BatteryCycleCount` → powercfg |

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
cycle count (<100 early / <500 moderate / ≥500 high); temperature > 45 °C;
|drift| > 5%; monthly capacity loss ≥ 1 mWh; and the EOL projection when one
exists. Habit insights: AC-time share; > 40% of recorded time sitting ≥ 95%
charged while plugged in ⇒ charge-cap suggestion; ≥ 25% of on-battery entries
below 15% ⇒ deep-discharge warning.

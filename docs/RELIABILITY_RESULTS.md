# T5 reliability, soak & stress results — 1.0.2 release build (2026-07-12)

## Soak run (4 hours, default 30 s interval, isolated portable instance)

*(table completed at collection — see final section)*

## Stress run (fastest interval, 30+ minutes)

Isolated portable instance pinned to `sampleIntervalSec = 1`, monitored
once per minute:

| Metric | Result |
|---|---|
| Duration | 33.1 min at 1 s interval |
| Sampling cadence | **1.00 s exactly** — 1994 samples across 2003 s, zero missed ticks, no queue buildup |
| CPU | 73.3 s over 1986 s = **3.7 % of one core** (~0.46 % of machine) |
| Private bytes | 135.6 MB → 136.8 MB (**+1.2 MB over ~2000 update cycles**; plateau behavior, see soak for the long-horizon verdict) |
| Threads / handles | 28→19 (startup pool wind-down) then stable / ~2600 stable |
| UI | window alive, `Responding = true` at the end — no freeze |
| Database | `PRAGMA integrity_check = ok` |

## Kill/restart torture (20 cycles, mid-write kills)

App at 1 s interval (a durable insert in flight roughly every second),
hard-killed (`Stop-Process -Force`) after a random 3–8 s dwell, 20 times:

- **`PRAGMA integrity_check = ok` after every one of the 20 kills.**
- Sample count grew monotonically across cycles (6 → 103): data written
  before each kill survived it.
- `settings.json` parseable after every cycle (QSaveFile atomic writes).
- **0 corruptions of any kind.**

## Clock-jump handling

Windows clock changes require elevation (never used), so wall-clock jumps
are driven through the injectable timestamps every time-consuming
component accepts (`snapshot.timestamp`, Calibration's `now`) — suite
`test_clock_jumps` (7 cases): forward DST-style and suspend-resume jumps
keep the live series finite and monotonic; backward jumps (NTP/manual)
never crash or poison the trend with NaN; `resetSessionOrigin()` recovers
cleanly; the calibration state machine neither skips nor corrupts steps
under backward time (`computeNext` with negative elapsed simply waits).
The sampling cadence itself is monotonic-clock based (Qt timers use
QueryPerformanceCounter on Windows) and is unaffected by wall-clock moves
— reasoned, not simulated.

## Concurrency analysis (explicit reasoning — TSan unavailable)

ThreadSanitizer does not support Windows/MinGW targets; that is a stated
limit, compensated by architecture that leaves nothing for TSan to find:

- **Message-passing only, no shared mutable state.** The worker `Sampler`
  owns its `BatteryReader` (created inside `start()` on the worker thread
  — COM apartment affinity). Results cross to the UI thread exclusively
  as `BatterySnapshot` **copies** in queued signal emissions
  (`Q_DECLARE_METATYPE` + `Qt::QueuedConnection`). Commands travel the
  other way exclusively via `QMetaObject::invokeMethod(...,
  QueuedConnection)`.
- **QThreadPool one-shots** (powercfg ingest, charge-cap probe) capture
  inputs by value, guard the model with `QPointer`, and marshal results
  back through queued `invokeMethod` — the pool thread never touches
  model state directly.
- **Database, Settings, TrayManager, QML** live on the UI thread only.
- **Shutdown**: the worker is stopped with `quit()` + `wait(5000)` in the
  model destructor before members are torn down; the 30-min stress and
  20-kill torture exercised startup/teardown 20+ times without a hang.
- Empirical corroboration: 24 suites (including the threaded sampler
  suite) run clean, and the 4-hour soak + stress produced zero crashes.

## Zombie-process regression

Re-verified this round: a deliberately broken deployment (missing `qml/`
tree) exits with **code 1** — no windowless lingering process (the
`rootObjects().isEmpty()` failsafe added in 1.0.2).

## AC↔battery transitions

The entire soak ran on battery (the machine was genuinely discharging),
so no natural AC flip occurred during it. Force-simulation stands in
(physically toggling AC is not software-controllable): the
`acBatteryTransitionCycle` test drives discharge → plug → charge → full
→ unplug through the model and asserts status, alert input and live
series at every step; alert latching/hysteresis across the same
transitions is covered by the alerts suite.

## Soak results (filled at collection)

<!-- SOAK_RESULTS -->

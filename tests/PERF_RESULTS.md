# T4 performance measurements — 1.0.2 release build (reference HP laptop, 8 logical cores)

Measured 2026-07-12 with `tests/test_bench_perf.cpp` (in-suite gates) and
process-level scripts. A 4-hour soak instance was idling concurrently at
the default interval (~0.6 % of one core); numbers below are therefore, if
anything, slightly pessimistic.

| Measurement | Value |
|---|---|
| Cold start → window visible | **2406 ms** |
| Warm start → window visible | **1212 ms** |
| WMI full-snapshot read (steady state, n=30) | **mean 34–45 ms, p95 34–56 ms, max 302 ms** (two runs) |
| powercfg generate + parse | **117–234 ms** |
| SQLite insert, autocommit (the live sampler path) | **144–158 rows/s (~7 ms per durable insert)** |
| SQLite insert, transaction bulk (new `insertSamplesBulk`) | **33,841 rows/s** |
| DB bytes per sample row | **113 B** |
| DB growth @ 1 s interval | 9.8 MB/day · 293 MB/30d · **3.6 GB/365d** (flagged below) |
| DB growth @ 30 s (default) | 0.33 MB/day · 9.8 MB/30d · **119 MB/365d** |
| DB growth @ 300 s | 0.03 MB/day · 1.0 MB/30d · 11.9 MB/365d |
| `samplesBetween` (100k-row cap) @ 1k / 10k / 100k / 1M rows | 2 / 17–18 / 207 / **196 ms** |
| `sampleCount` @ 1M rows | 17 ms |
| Steady CPU @ 30 s interval, dashboard | **0.42 % of one core (0.05 % of machine)** |
| Steady CPU @ 1 s interval, dashboard | 3.76 % of one core (0.47 % of machine) |
| Steady CPU @ 1 s interval, live-monitor page (canvas active) | 4.11 % of one core (0.51 % of machine) |
| Working set / private bytes (steady) | 173 MB / 131 MB (dashboard); 178 / 144 MB (monitor) |
| Threads / handles (steady) | 18 / ~2510 (22 threads with monitor canvas) |

## Assessment for a low-end laptop

- **Default configuration (30 s):** 0.05 % machine CPU and one 113-byte
  durable write per tick. The app's own energy draw is **below the
  measurement noise floor** of the pack gauge (which reports in ~10 mW
  steps against a 6–15 W system draw); it cannot be isolated on a live
  system, and at 0.05 % machine CPU it is bounded well under ~10 mW on
  this class of CPU. Not a battery-drain concern.
- **Fastest interval (1 s, opt-in):** ~0.5 % machine CPU — still
  negligible thermally, but the **database grows 3.6 GB/year**; flagged
  as the one number a user should know before pinning 1 s permanently.
  (Default stays 30 s = 119 MB/year.)
- **RAM:** ~173 MB working set is the Qt Quick/QML runtime baseline; flat
  over time (see soak). Acceptable on 4 GB machines, worth knowing.
- **No degradation knee found in DB queries up to 1,000,000 rows**
  (windowed query capped at 100k rows returns in ~200 ms). History-screen
  rendering reads the weekly `capacity_history` table (dozens of rows),
  not `samples`, so UI render time is independent of sample count; the
  1M-row figures cover the export/query path, which is the only consumer
  of bulk sample reads.

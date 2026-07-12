# T1 coverage map — baseline before the deep-verification round

> **T7 closure:** after the round, coverage stands at **line 86 %
> (2481/2861), branch 54 % (2719/5011)** across **24 suites / 3593 real
> cases**. Remaining dark spots, all with stated reasons: `main.cpp`
> (process-level; exercised by the launch/walkthrough/zombie process tests
> outside the instrumented build), `WmiClient` COM-initialization failure
> arms (would need process-level COM fault injection), `ChargeCap` Lenovo
> row-parsing arms (needs Lenovo hardware or a WmiClient DI refactor —
> known limit), parts of `Autostart` COM error arms. Every other gap from
> the table below was closed by T2/T3/T7 suites.

Instrumented build: `build-cov` (MinGW `--coverage`, Debug), report via
gcovr 8.6 over `src/`. Baseline after the existing 16 suites:

- **Line coverage: 77 % (2176 / 2822)**
- **Branch coverage: 46 % (2314 / 4967)**
- Suite wall time: 3.2 s (Release), 9.5 s (instrumented Debug)
- Baseline case count: 158 QTest totals = **126 real cases** (excluding
  the implicit init/cleanupTestCase pair in each of the 16 suites)

## Gap list, ordered by risk (drives T2/T3 test writing)

| # | Area (file, ~missing) | Risk | What is uncovered |
|---|---|---|---|
| 1 | `core/Metrics.cpp` (95 % line, 7 lines) | **metric math** | guard/error branches: regression degenerate cases, EOL rejection paths, drain edge cases (l.82, 152–214) |
| 2 | `acquisition/BatteryReader.cpp` (77 %) | **hardware parsing** | chemistry FourCC non-printable/unknown branches, token normalization arms, CIM-datetime rejection arms, CIMV2 ordinal-merge branches (extra CIMV2 packs, DeviceID fallback), static-cache miss/reuse arms, `read()` composition |
| 3 | `acquisition/WmiClient.cpp` (71 %) | **hardware COM** | `variantToQVariant` type arms (I1/UI2/I8/R4…), connect() failure arms (CoInitialize/Security/Locator/ConnectServer/ProxyBlanket), queryInstances fallback sub-arms. COM-init failures need process-level fault injection — partially untestable, will be reasoned not faked |
| 4 | `acquisition/PowercfgReport.cpp` (88 %) | **parser** | `generate()` QProcess arms (missing exe, timeout, nonzero exit), parse guards l.137–153 |
| 5 | `app/BatteryModel.cpp` (72 %) | model | timeEstimateText arms, usageLog/degradationInfo guards, export error paths, setters (theme/interval/settings), tryToggleChargeCap, initialize double-call, HTML report input |
| 6 | `data/Settings.cpp` (86 %) | robustness | corrupt/truncated/wrong-type JSON arms, clamp fallbacks |
| 7 | `data/Database.cpp` (87 %) | robustness | closed-DB error arms on every query method |
| 8 | `core/HealthVerdict.cpp` (90 %) | verdict rules | detail-line brackets (cycle tiers, drift, decline, EOL) partial |
| 9 | `app/ChargeCap.cpp` (37 %) | vendor probe | Lenovo row parsing + setEnabled arms (non-Lenovo machine never exercises them) |
| 10 | `app/AlertManager.cpp` (89 %) | alerts | ctor/default arms l.13–20 |
| 11 | `app/Exporter.cpp` (89 %) | exports | dir-creation/save failure arms |
| 12 | `app/Autostart.cpp` (78 %) | shell | COM failure arms, override-path arms |
| 13 | `app/Calibration.cpp/.h` (94 %) | workflow | two step-name arms |
| 14 | `app/TrayManager.cpp` (0 %) | UI | needs a QApplication; icon drawing + tooltip + availability testable offscreen |
| 15 | `src/main.cpp` (0 %) | process | covered only by process-level smoke/walkthrough tests outside the instrumented build — by design; noted as a known limit |

Rule for the round: tests are written **at these gaps**, not blindly.

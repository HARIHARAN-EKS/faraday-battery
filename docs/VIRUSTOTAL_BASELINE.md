# VirusTotal baseline — releases 1.0.1, 1.0.2 and 1.0.5

Measured facts from real VirusTotal submissions. Sources: the archived
report PDFs at the repository root (`VirusTotal\`, `VirusTotal\2\`,
`VirusTotal\3\`). **Every 1.0.5 report was verified to match the expected
artifact hash before use — no mismatches.**

## Version-over-version comparison

| Artifact | 1.0.1 | 1.0.2 | **1.0.5** |
|---|---|---|---|
| Application exe | 0 / 70 | 0 / 70 | **0 detections** (now `faraday-core.exe`) |
| **Launcher** `Faraday.exe` (new in 1.0.4) | — | — | **1 / 70 — Arctic Wolf: `Unsafe`** |
| Installer | 1 / 69 — Trapmine `Malicious.moderate.ml.score` | 1 / 68 — Trapmine, same | **2 / 69 — Elastic: `Malicious (high Confidence)` (NEW) + Trapmine: `Suspicious.low.ml.score`** |
| Portable ZIP | not scanned | 0 detections | **0 detections** |

Two movements to state plainly:
1. **The launcher attracted one engine the single-exe never did** (Arctic
   Wolf). Notably Arctic Wolf reports the *application* (`faraday-core.exe`)
   as **Undetected** — it is the launcher's silhouette, not our code, that
   it scores.
2. **The installer picked up Elastic**, while Trapmine's own label
   *softened* (`Malicious.moderate` → `Suspicious.low`).

## 1.0.5 artifacts (scanned 2026-07-12)

### 1. `Faraday.exe` — launcher — **1 / 70**

| Property | Value |
|---|---|
| SHA-256 | `a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1` |
| MD5 / SHA-1 | `04c9c3627cf60ee851d0e6ce0216f98a` / `8069831182a4748f2c3aa5e497b6261e555d300c` |
| VT link | <https://www.virustotal.com/gui/file/a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1> |
| **Flagged by** | **Arctic Wolf — `Unsafe`** |
| Classification | **generic/heuristic verdict label**, not a named malware family (the string is literally the word "Unsafe"; no family, no variant, no technique) |
| File type | PE32+ GUI x86-64, 129.67 KB; DetectItEasy: PE64, GNU linker ld 2.39; **18 sections**, `.text` entropy **5.87** (no packer) |
| Imports | **KERNEL32, msvcrt, USER32 — system only** |
| VersionInfo | present and complete (Product/Description/OriginalFilename `Faraday.exe`/1.0.5.0) |
| Signature | not signed |
| Resources | RT_ICON, RT_GROUP_ICON, RT_MANIFEST, RT_VERSION; icon PNGs entropy 7.53–7.95 (**this is the icon data**) |
| Relations | execution parents: the installer (2/69) and the ZIP (0/64) |

### 2. `faraday-core.exe` — application — **0 detections**

| Property | Value |
|---|---|
| SHA-256 | `f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b` |
| VT link | <https://www.virustotal.com/gui/file/f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b> |
| Verdict | **Undetected by every engine — including Arctic Wolf, Elastic and Trapmine** |
| File type | PE32+ GUI x86-64, 3.49 MB; Qt 6.x / GNU ld 2.39; 22 sections, `.text` entropy 5.96 |
| Imports | Qt6Core/Gui/Qml/Sql/Widgets, libgcc, libstdc++, KERNEL32, msvcrt, ole32, OLEAUT32 — **no network DLLs** |
| VersionInfo | present; Description: *"Faraday internal component - run Faraday.exe instead"* |

### 3. `Faraday-1.0.5-setup-win64.exe` — installer — **2 / 69**

| Property | Value |
|---|---|
| SHA-256 | `8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d` |
| VT link | <https://www.virustotal.com/gui/file/8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d> |
| **Flagged by** | **Elastic — `Malicious (high Confidence)`** (NEW); **Trapmine — `Suspicious.low.ml.score`** |
| Classification | both are **generic ML/confidence scores**, not named families. Elastic's string carries no family/technique; Trapmine's is an explicit ML score bucket — and it *dropped* from "Malicious.moderate" (1.0.1/1.0.2) to "Suspicious.low" |
| File type | PE32, NSIS 3.12 [lzma, solid], 22.38 MB; 5-section stock stub; `.rsrc` entropy 7.91 (icons), LZMA overlay 7.99999 (inherent) |
| Imports | ADVAPI32, SHELL32, ole32, COMCTL32, USER32, GDI32, KERNEL32 |
| VersionInfo | present and complete; not signed |

### 4. `Faraday-1.0.5-portable-win64.zip` — **0 detections**

| Property | Value |
|---|---|
| SHA-256 | `f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe` |
| VT link | <https://www.virustotal.com/gui/file/f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe> |
| Verdict | **0 detections** (ClamAV/Sangfor timeout; Arctic Wolf, Trapmine, SecureAge, TEHTRIS et al. "unable to process file type") |
| Bundled files (115 listed) | `Faraday\Faraday.exe` **1/70** (the Arctic Wolf hit); `Faraday\app\faraday-core.exe` **0/68**; every Qt/MinGW runtime DLL **0/N** (Qt6Core 0/67, Qt6Gui 0/67, Qt6Network 0/65, libstdc++ 0/66, opengl32sw 0/70, D3Dcompiler_47 0/69, …) |
| Dropped files (144) | all **0/N** — QML files, `portable.txt`, `README.txt`, plugins |

## Comparison protocol for future scans

1. Upload the launcher, the core, the installer and the ZIP.
2. Expect: core 0/N; ZIP 0/N; launcher 0–1/N (Arctic Wolf "Unsafe" is now a
   known, recorded generic hit); installer 0–2/N with only generic/ML labels.
3. Escalate — treat as a real problem — only if a **major** engine fires on
   the *core*, more than ~3 engines fire on anything, or any detection names
   a **specific malware family**.
4. Record new hashes and links here. FP submissions live in
   `FP_SUBMISSIONS/` (Arctic Wolf, Elastic, Trapmine).

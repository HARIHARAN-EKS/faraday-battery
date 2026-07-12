# VirusTotal baseline — release 1.0.1

Measured facts from the first real VirusTotal submission of the 1.0.1
artifacts. Source: the report PDFs archived in `VirusTotal\` at the
repository root (summary, detection, details and relations tabs for both
files). Future scans should be compared against this baseline.

## Artifact 1 — faraday.exe (application, 1.0.1)

| Property | Value |
|---|---|
| **Verdict** | **0 / 70 — no security vendor flagged this file** |
| Scan date | 2026-07-12 02:14:25 UTC (first + last submission) |
| SHA-256 | `b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff` |
| SHA-1 | `82565c30be21210d788810b0636f807fc970d9b5` |
| MD5 | `aecb30de3e8ca6f8e2968b12755701bb` |
| VT link | <https://www.virustotal.com/gui/file/b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff> |
| File type | Win32 EXE (PE32+ GUI x86-64), 3.35 MB (3,511,077 bytes) |
| DetectItEasy | PE64 · Library: Qt (6.X) · Linker: GNU linker ld (GNU Binutils) 2.39 [GUI64] |
| TrID | Microsoft Visual C++ compiled executable (generic) 45.6% · Win64 Executable (generic) |
| Signature | not signed (no certificate available) |
| Version info | Copyright (C) 2026 Faraday Project. MIT License. · Faraday Battery Intelligence Suite · 1.0.1.0 — fully populated |
| Resources | RT_ICON ×7, RT_GROUP_ICON ×1, RT_MANIFEST ×1, RT_VERSION ×1 (all ENGLISH US) |
| Imports | Qt6Core, Qt6Gui, Qt6Qml, Qt6Sql, Qt6Widgets, libgcc_s_seh-1, KERNEL32, msvcrt, ole32, OLEAUT32 — **no network DLLs** |
| Relations | PE resource child (XML manifest) 0/60; bundled sections 0/61 |

## Artifact 2 — Faraday-1.0.1-setup-win64.exe (NSIS installer)

| Property | Value |
|---|---|
| **Verdict** | **1 / 69** |
| **Flagging vendor** | **Trapmine** |
| **Detection name** | **`Malicious.moderate.ml.score`** |
| Scan date | 2026-07-12 02:14:54 UTC (first + last submission) |
| SHA-256 | `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99` |
| SHA-1 | `330ec3577f687e999316b1039f583ddb57828796` |
| MD5 | `8260d824503b64cdfb17c0bff61d1c14` |
| VT link | <https://www.virustotal.com/gui/file/aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99> |
| File type | Win32 EXE (PE32 GUI Intel 80386), Nullsoft Installer self-extracting archive, 22.07 MB (23,146,977 bytes) |
| DetectItEasy | PE32 · Installer: Nullsoft Scriptable Install System **(3.12) [lzma, solid]** |
| Signature | not signed (no certificate available) |
| Version info | present (Copyright / Product / Description "Faraday - Battery Intelligence Suite Setup" / FileVersion 1.0.1.0) — `OriginalFilename` and `InternalName` were missing in 1.0.1 |
| Sections | 5 — `.text .rdata .data .ndata .rsrc` (standard NSIS 3.12 stub layout; creation timestamp 2026-04-19 20:38:47 UTC is the stock stub's) |
| Resources | RT_ICON ×7, RT_GROUP_ICON ×1, RT_DIALOG ×5 (classic NSIS UI in 1.0.1), RT_MANIFEST ×1, RT_VERSION ×1 |
| Overlay | the LZMA solid archive: entropy 7.99999 (inherent to any compressed payload; not a packer on the stub) |
| **Dropped files** | **102 scanned individually — 0 detections on every one** |
| Bundled files | 100 listed — 0 detections on every one (Qt DLLs, QML plugins, etc.) |
| Other engines | ClamAV: timeout · Avast-Mobile / BitDefenderFalx / Symantec Mobile Insight / Trustlook: unable to process file type (mobile engines) |

### Sandbox "contacted domains/IPs" note

The relations tab lists `nexusrules.officeapps.live.com` and seven
Microsoft-owned IPs (all AS 8075). That is background traffic of the
Windows/Office image inside VirusTotal's detonation sandbox, not traffic
from Faraday: the application binary imports no network DLLs (see artifact
1) and the installer's only imports are ADVAPI32 / SHELL32 / ole32 /
COMCTL32 / USER32 / GDI32 / KERNEL32.

## Classification of the single detection

`Malicious.moderate.ml.score` is, by its own naming, a **machine-learning
score bucket** ("moderate ML score") — not a named malware family, not a
signature match, and corroborated by zero of the other 68 engines
(including Microsoft, Kaspersky, BitDefender, ESET, Sophos, CrowdStrike,
Elastic and both static-ML engines). Combined with the 0/70 payload
executable and 0 detections across all 102 dropped files, this is a
**generic false positive on the unsigned NSIS wrapper** — exactly the
residual-risk class predicted in AV_HARDENING.md ("1–3 hits from
low-reputation engines are the normal unsigned-binary noise level").

## Comparison protocol for future scans

1. Upload the new `faraday.exe` and `Faraday-<ver>-setup-win64.exe`.
2. Expect: payload exe 0/N; installer 0–1/N with only generic/ML labels.
3. Escalate (treat as a real problem, not noise) only if: a **major**
   engine fires, more than ~3 engines fire, or any detection names a
   **specific malware family**.
4. Record the new hashes and links in this file alongside the baseline.

# VirusTotal baseline — releases 1.0.1 and 1.0.2

Measured facts from real VirusTotal submissions. Sources: the report PDFs
archived at the repository root — `VirusTotal\` (1.0.1 round) and
`VirusTotal\2\` (1.0.2 round). Note for the 1.0.2 round: the saved summary
pages for the exe and the ZIP (`2\1_1.pdf`, `2\3_1.pdf`) are empty files;
their data is fully recoverable from the detection/details tabs, whose
titles and URLs carry the same SHA-256s. Every 1.0.2 report was verified
to match the expected artifact hash before use.

## The headline: 1.0.1 → 1.0.2 comparison

| Artifact | 1.0.1 | 1.0.2 | Movement |
|---|---|---|---|
| **faraday.exe** (payload) | **0 / 70** | **0 / 70 — stayed clean; Trapmine explicitly Undetected** | none — the A2/A3 code changes (portable mode, MUI2 round) did not attract a single engine |
| **NSIS installer** | 1 / 69 — Trapmine `Malicious.moderate.ml.score` | **1 / 68 — Trapmine `Malicious.moderate.ml.score`** | unchanged: same single vendor, same generic ML bucket, no new engines. The full A2 hygiene pass (MUI2, complete VersionInfo, manifests, zero-Exec) did not move Trapmine's ML score — confirming the residual is keyed on the unsigned stub itself, exactly as the structural-limit statement predicted |
| **Portable ZIP** (recommended channel) | not scanned | **0 detections** (67 engines Undetected; ClamAV & Sangfor timeout; 7 mobile/appliance engines unable to process; Trapmine itself *unable to process file type*) | first scan — clean. Load-bearing result for the recommended channel |

Classification of the sole remaining detection: `Malicious.moderate.ml.score`
is a machine-learning score bucket, not a named malware family, and is
corroborated by none of the other engines. **Generic ML false positive on
the unsigned NSIS wrapper.** Everything Faraday actually ships executes
with 0 detections.

## Artifact details — 1.0.2 round (scanned 2026-07-12)

### faraday.exe 1.0.2 — 0/70, scanned 02:55:23 UTC

| Property | Value |
|---|---|
| SHA-256 | `3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16` |
| SHA-1 / MD5 | `854a882f920fb21c98b784189aa0b273d97b2197` / `62c57889762c3ef0a24d6efcf9fad634` |
| VT link | <https://www.virustotal.com/gui/file/3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16> |
| File type | PE32+ GUI x86-64, 3.35 MB (3,513,482 bytes); DetectItEasy: PE64 · Qt 6.X · GNU ld 2.39 |
| Sections | 22 (same layout as 1.0.1: .text/.data/.rdata/… + DWARF debug sections) |
| VersionInfo | detected, fully populated, FileVersion 1.0.2.0 |
| Imports | Qt6Core/Gui/Qml/Sql/Widgets, libgcc, KERNEL32, msvcrt, ole32, OLEAUT32 — no network DLLs |
| Signature | not signed |
| Relations | PE resource child (manifest XML) 0/60 |

### Faraday-1.0.2-setup-win64.exe — 1/68 (Trapmine), scanned 02:55:52 UTC

| Property | Value |
|---|---|
| SHA-256 | `9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457` |
| SHA-1 / MD5 | `7ca2c9d8b8971b449e942e378d50d75691d412bd` / `6396f7b60792c7fea599db80d2580365` |
| VT link | <https://www.virustotal.com/gui/file/9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457> |
| Flagged by | **Trapmine: `Malicious.moderate.ml.score`** (generic ML bucket); all other engines Undetected; ClamAV timeout; 5 mobile engines unable to process |
| File type | PE32 NSIS self-extracting, 22.09 MB (23,159,266 bytes); DetectItEasy: NSIS **3.12** [lzma, solid]; stock 5-section stub (`.text .rdata .data .ndata .rsrc`) |
| VersionInfo | detected — now includes `OriginalFilename` (`Faraday-1.0.2-setup-win64.exe`) and `InternalName` (`faraday-setup`) added in the A2 hygiene pass; RT_DIALOG ×5 (MUI2) |
| Overlay | LZMA solid archive, entropy 7.99999 (inherent to compression) |
| Signature | not signed (no certificate available) |

### Faraday-1.0.2-portable-win64.zip — 0 detections, scanned 02:56:06 UTC

| Property | Value |
|---|---|
| SHA-256 | `83afe5704aaff4d348ef3d917e27f878f6f72c961ccfb9fbc7fd0eae7c69393e` |
| SHA-1 / MD5 | `ad67bf8f1ef8709508aab9a5b01b53815465b550` / `d7bd9e69ecfe3c8b410125e65bcf4539` |
| VT link | <https://www.virustotal.com/gui/file/83afe5704aaff4d348ef3d917e27f878f6f72c961ccfb9fbc7fd0eae7c69393e> |
| Verdict detail | 67 engines Undetected; ClamAV + Sangfor timeout; Arctic Wolf, BitDefenderFalx, Palo Alto, SecureAge, Symantec Mobile Insight, TEHTRIS, **Trapmine**: unable to process file type |
| File type | ZIP (store method at archive level), 30.41 MB; 167 contained files (43 PE, 64 QML, 11 qmltypes, 1 TXT = `portable.txt`, 37 dirs), 79.31 MB uncompressed |
| Bundled-file scans | every listed file 0/N — `Faraday/faraday.exe` **0/70**, stock Qt/MinGW runtime DLLs (Qt6Core 0/67, Qt6Gui 0/67, Qt6Network 0/65, Qt6OpenGL 0/72, libgcc 0/70, libstdc++ 0/66, libwinpthread 0/70, D3Dcompiler_47 0/69, opengl32sw 0/70, …) |

## Baseline — 1.0.1 round (scanned 2026-07-12, first submission)

| Artifact | Verdict | SHA-256 |
|---|---|---|
| faraday.exe 1.0.1 | 0 / 70 | `b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff` |
| Faraday-1.0.1-setup-win64.exe | 1 / 69 — Trapmine `Malicious.moderate.ml.score` | `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99` |
| (all 102 dropped files) | 0 detections each | — |

Full 1.0.1 details (PE metadata, sandbox-noise note about the AS-8075
Microsoft IPs on the relations tab, etc.) are preserved in the archived
PDFs under `VirusTotal\`.

## Comparison protocol for future scans

1. Upload the new exe, installer and ZIP.
2. Expect: exe 0/N; ZIP 0/N; installer 0–1/N with only generic/ML labels
   (Trapmine's ML score is a known, stable false positive on the unsigned
   stub — an FP report template lives in `FP_SUBMISSIONS/Trapmine.md`).
3. Escalate — treat as a real problem — only if a **major** engine fires,
   more than ~3 engines fire, or any detection names a specific malware
   family. The payload exe must always be 0.
4. Record new hashes and links here.

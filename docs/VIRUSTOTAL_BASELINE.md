# VirusTotal baseline — releases 1.0.1, 1.0.2, 1.0.5 and 1.0.6

Every VirusTotal result this project has measured, in one place. Sources: the
archived report exports (`VirusTotal\`, `VirusTotal\2\`, `VirusTotal\3\`,
`VirusTotal\4\`). **Every 1.0.6 report was verified to match the expected
artifact hash before use — no mismatches.**

Nothing here is inferred or carried over. A result appears only if that exact
SHA-256 was actually scanned.

## Version-over-version comparison

| Artifact | 1.0.1 | 1.0.2 | 1.0.5 | **1.0.6** |
|---|---|---|---|---|
| **Application** (`faraday-core.exe`; a single `Faraday.exe` before 1.0.5) | **0 / 70** | **0 / 70** | **0 detections** | **0 / 70** |
| **Portable ZIP** | not scanned | **0 detections** | **0 detections** | **0 detections** |
| **Launcher** `Faraday.exe` (new in 1.0.4) | — | — | 1 / 70 — Arctic Wolf `Unsafe` | **1 / 70 — Arctic Wolf `Unsafe`** |
| **Installer** | 1 / 69 — Trapmine `Malicious.moderate.ml.score` | 1 / 68 — Trapmine, same | 2 / 69 — Elastic `Malicious (high Confidence)` (NEW) + Trapmine `Suspicious.low.ml.score` | **2 / 69 — Elastic `Malicious (moderate Confidence)` + Trapmine `Suspicious.low.ml.score`** |

Read the first two rows across: **the application has never been flagged by any
engine in any version, and the portable ZIP has never been flagged since the
day it started shipping.** Every detection this project has ever received has
landed on a *wrapper* — the installer stub or the launcher stub.

## 1.0.6 artifacts (scanned 2026-07-12)

### 1. `faraday-core.exe` — the application — **0 / 70**

This was the number that mattered this round. 1.0.6 rebuilt the UI as a static
QML module (`faradayui`) linked into the application, which changed the binary
substantially — it is a genuinely new file, and 1.0.5's clean result did not
carry over to it. It had to be re-earned. It was.

| Property | Value |
|---|---|
| SHA-256 | `8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98` |
| VT link | <https://www.virustotal.com/gui/file/8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98> |
| Verdict | **Undetected by every engine — including Arctic Wolf, Elastic and Trapmine** |
| File type | PE32+ GUI x86-64, 3.66 MB; DetectItEasy: Library Qt (6.X), GNU ld 2.39; **22 sections**, `.text` entropy **5.97** (no packer) |
| Evidence of the QML change | new export `_Z26qml_register_types_Faradayv`; `.text` grew to 733 KB |
| Imports | Qt6Core/Gui/Qml/Sql/Widgets, libgcc, KERNEL32, msvcrt, ole32, OLEAUT32 — **no network DLLs** |
| VersionInfo | complete; **File Version `1.0.6.0`**; Description *"Faraday internal component - run Faraday.exe instead"* |
| Signature | not signed |
| Resources | 7 × RT_ICON (one per rendered size), RT_GROUP_ICON, RT_MANIFEST, RT_VERSION |

### 2. `Faraday-1.0.6-portable-win64.zip` — the recommended download — **0 detections**

| Property | Value |
|---|---|
| SHA-256 | `45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115` |
| VT link | <https://www.virustotal.com/gui/file/45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115> |
| Verdict | **0 detections** — every engine Undetected (ClamAV/Google/Sangfor timed out; Arctic Wolf, Trapmine, Palo Alto, SecureAge, TEHTRIS et al. "unable to process file type") |
| Contents | 172 files, 79.78 MB uncompressed; 45 PEs |
| Bundled files (117 scanned) | `Faraday\app\faraday-core.exe` **0 / 70**; every Qt/MinGW runtime DLL **0/N** (Qt6Core 0/67, libstdc++ 0/66, opengl32sw 0/70, D3Dcompiler_47 0/70, …); `Faraday\Faraday.exe` **1 / 70** (the known Arctic Wolf hit on the launcher) |
| Dropped files (147) | all **0/N** — QML files, `portable.txt`, `README.txt`, plugins |

### 3. `Faraday.exe` — the launcher stub — **1 / 70**

| Property | Value |
|---|---|
| SHA-256 | `211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987` |
| VT link | <https://www.virustotal.com/gui/file/211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987> |
| **Flagged by** | **Arctic Wolf — `Unsafe`** |
| Classification | **generic verdict label**, not a named malware family — the string is literally the word "Unsafe": no family, no variant, no technique |
| File type | PE32+ GUI x86-64, 130.17 KB; GNU ld 2.39; 18 sections, `.text` entropy **5.87** (no packer) |
| Imports | **KERNEL32, msvcrt, USER32 — system DLLs only** |
| VersionInfo | complete; **File Version `1.0.6.0`**; OriginalFilename `Faraday.exe` |
| Signature | not signed |
| Relations | execution parents: the ZIP (0 detections) and the installer (2/69) |

Arctic Wolf reports the *application this launcher starts* as **Undetected**.
It is scoring the stub's silhouette, not our code.

### 4. `Faraday-1.0.6-setup-win64.exe` — the installer — **2 / 69**

| Property | Value |
|---|---|
| SHA-256 | `be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b` |
| VT link | <https://www.virustotal.com/gui/file/be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b> |
| **Flagged by** | **Elastic — `Malicious (moderate Confidence)`**; **Trapmine — `Suspicious.low.ml.score`** |
| Classification | both are **generic ML/confidence scores**, not named families. Neither string carries a family, variant or technique. Trapmine's is an explicit `low` score bucket |
| File type | PE32, NSIS 3.12 [lzma, solid], 22.43 MB; stock 5-section stub; `.rsrc` entropy 7.91 (icon data), LZMA overlay 7.99999 (inherent to solid LZMA, not obfuscation) |
| Imports | ADVAPI32, SHELL32, ole32, COMCTL32, USER32, GDI32, KERNEL32 |
| VersionInfo | complete; **File Version `1.0.6.0`**; not signed |
| Bundled/dropped | 100 bundled, 99 dropped — every Qt DLL, QML file and text file **0/N**. The only non-zero entries are our own launcher (1/70) and NSIS's own stock `$PLUGINSDIR/System.dll` (1/70) |

## The four questions this round was run to answer

**Did `faraday-core.exe` stay at 0 detections after the static-QML-module
change? — YES.** 0/70. New binary, new hash, clean result re-earned rather
than inherited.

**Did the portable ZIP — the primary recommended download — stay at 0
detections? — YES.** 0 detections on the archive, and all 117 bundled files
scan 0/N individually.

**Did the icon regeneration attract or shed any engine? — NO.** 1.0.6 rebuilt
every `.ico` from a purpose-drawn mark, rewriting `.rsrc` on all three PEs
(now 7 `RT_ICON` entries each, one per rendered size). Measured `.rsrc`
entropy moved by at most 0.02 (launcher 7.96 → 7.96; core 7.93 → 7.91). No
engine appeared and none disappeared. The resource rework was visually
significant and forensically invisible.

**Arctic Wolf / Elastic / Trapmine — still flagging, and anyone new? — The
same three, and NO new vendor anywhere.** Arctic Wolf still flags only the
launcher; Elastic and Trapmine still flag only the installer. Zero new vendors
on any of the four artifacts.

**One thing improved on its own:** Elastic **softened** from `Malicious (high
Confidence)` (1.0.5) to `Malicious (moderate Confidence)` (1.0.6) — with no
work done to court it. This mirrors 1.0.5, where Trapmine softened from
`Malicious.moderate.ml.score` to `Suspicious.low.ml.score`. Both movements
happened without us aiming at them.

## What four rounds of measurement have established

1. **The detections track the *shape* of the wrappers, not their contents.** A
   small statically-linked stub that starts a child process, and an NSIS
   installer — the two silhouettes ML models associate with droppers. The
   payload both of them deliver scans perfectly clean, every time.
2. **Hygiene work does not move these scores.** Across four releases we have
   kept entropy honest, kept the `asInvoker` manifest, kept full VersionInfo,
   carried zero network imports, and verified no W+X sections. The generic ML
   hits did not move in response to any of it. The two changes that *did*
   occur — Trapmine softening, then Elastic softening — both happened without
   us doing anything aimed at them.
3. **Therefore chasing these scores is not engineering.** The only real fix is
   a code-signing certificate, which this project cannot afford. Everything
   else would mean changing the binary to please a classifier rather than to
   serve a user, and we do not do that. No packing, no obfuscation, no
   evasion — including no evasion of *detection*.

## Behavioural corroboration

VirusTotal detonated `faraday-core.exe` (CAPA, CAPE Sandbox, Zenbox). Full
trace and its honest limits: `docs/SECURITY_AUDIT.md` §5. Headline: **"Network
comms: NOT FOUND"**, no dropped files, no persistence, no services, and
registry keys **opened but never written**.

## Comparison protocol for future scans

1. Upload the launcher, the core, the installer and the ZIP. Verify each
   report's hash against the release before reading it.
2. Expect: core 0/N; ZIP 0/N; launcher 0–1/N (Arctic Wolf `Unsafe` is a known,
   recorded generic hit); installer 0–2/N with only generic/ML labels.
3. Escalate — treat as a real problem — only if a **major** engine fires on the
   *core*, more than ~3 engines fire on anything, or any detection names a
   **specific malware family**.
4. Record new hashes and links here. FP submissions live in `FP_SUBMISSIONS/`
   (Arctic Wolf, Elastic, Trapmine).

---

*Footnote on one number: the 1.0.2 installer is recorded as **1 / 68** in the
archived report for that round; it is sometimes cited as 1/69 from memory. The
engine count VirusTotal reports varies slightly per scan (engines time out or
are added). The archived figure is the one used above.*

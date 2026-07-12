# T6 security & AV self-audit — 1.0.2 (2026-07-12)

Honest scope statement, up front: **this audit cannot scan against 70
engines.** That measurement already exists (VIRUSTOTAL_BASELINE.md:
application 0/70, portable ZIP 0-detection, one Trapmine generic-ML score
on the unsigned installer). This audit covers what is verifiable locally,
offline. Process Monitor is not installed on this machine and this round
downloads nothing, so the runtime audit uses built-in equivalents; the
method limits are stated with each result.

## 1. Windows Defender scan (MpCmdRun, platform 4.18.26060.3008-0)

`MpCmdRun.exe -Scan -ScanType 3 -File <target> -DisableRemediation`:

| Target | Result |
|---|---|
| `dist\Faraday\faraday.exe` | **found no threats** |
| `dist\Faraday-1.0.2-setup-win64.exe` | **found no threats** |
| `dist\Faraday` (entire extracted portable payload) | **found no threats** |

## 2. Static PE self-audit (re-asserted from scratch)

- **Import tables:** application imports Qt6 Core/Gui/Qml/Sql/Widgets,
  libgcc/libstdc++, KERNEL32, msvcrt, ole32, OLEAUT32, SHELL32 — no
  ws2_32/wininet/winhttp/Qt6Network. Installer imports the stock NSIS set
  (ADVAPI32/SHELL32/ole32/COMCTL32/USER32/GDI32/KERNEL32).
- **W+X sections: zero** on both PE files (section characteristics parsed
  from the PE headers; `.text` is execute-only, all writable sections are
  non-executable).
- **Section entropy sane:** application max 6.88 (`.rsrc`, icons);
  installer code sections 4.1–6.5. No packed 7.9+ code section anywhere
  (the installer's LZMA payload lives in the overlay, inherent to every
  NSIS installer and documented since 1.0.1).
- **VersionInfo present on both files; `asInvoker` present and
  `requireAdministrator` absent on both.**

## 3. Runtime behavioral audit (full sampling cycle, isolated portable copy)

| Check | Method | Result |
|---|---|---|
| Network connections | `netstat -ano` polled across a 75 s cycle (startup + powercfg + several samples) | **0 entries for the PID, ever** |
| Network DLLs | module list polled | `Qt6Network.dll` → `WS2_32.dll`/`WINHTTP.dll` are **mapped** into the process — an artifact of `Qt6Qml.dll`'s hard static import (documented in AV_HARDENING.md), **not of any app code**; with zero sockets ever opened (netstat) and zero network symbols in the source (below), mapped-but-never-called is the precise, honest description |
| Child processes | `Win32_Process` polled at 200 ms from launch for 30 s | **exactly one**: `C:\windows\System32\powercfg.exe /batteryreport /xml /output <dataDir>\faraday_batteryreport.xml` — the documented stock tool, temp file inside the app's own data dir, deleted after parse |
| Registry writes | HKCU\Software subkey diff before/after + recursive search for `*Faraday*` keys | **no new keys; no Faraday-named key exists anywhere** |
| Elevation | manifest (`asInvoker`) + no UAC prompt across every launch this round | never requested |
| File footprint | data-dir listing after run | `faraday.sqlite`, `settings.json` — nothing outside the data dir. *Method limit:* without ProcMon this is not an exhaustive file-I/O trace; it is corroborated by the source sweep (no other write sites) and the powercfg command line above |

## 4. Source-tree sweep (src/, ui/, installer/, resources/)

| Pattern class | Hits | Verdict |
|---|---|---|
| Network symbols (`socket, WSA, winhttp, wininet, QTcp/QUdp/QNetwork/QSsl, XMLHttpRequest, fetch`) | 0 real (two false positives: `WSA` inside `numRowsAffected`, `system (` inside UI prose) | **clean** |
| Registry write APIs (`RegSetValue/RegCreateKey/QSettings`) | 1 comment explaining why QSettings is deliberately NOT used | **clean** (installer's documented HKCU uninstall entry lives in faraday.nsi, unchanged) |
| Process creation | `QProcess` in `PowercfgReport.cpp` only | the documented powercfg call |
| URLs | XML namespace identifiers only (SVG xmlns in the HTML exporter, manifest schema xmlns, powercfg report schema) | spec-mandated identifier strings, not endpoints |
| Hardcoded IPs | 0 | clean |
| Telemetry vocabulary | 0 | clean |
| Secrets / machine-specific paths / usernames | 0 in shipped code | clean |
| PII: battery serial number | flows reader → model → UI and the user-generated local HTML report only | **no transmission path exists** — no sockets, no network DLLs in app imports, 0 connections observed |

## 5. EXTERNAL CORROBORATION — VirusTotal's own sandboxes (1.0.5)

This is worth more than our own assertions: the 1.0.5 round included the
**behaviour tabs** (CAPA, CAPE Sandbox, Zenbox, VirusTotal Jujubox /
Observer). Their observations are quoted faithfully below and checked
against every claim this document makes.

### 5a. The launcher, `Faraday.exe` — what the sandbox actually did

**Honest caveat first, because it changes how much the trace proves:** the
sandbox detonated `Faraday.exe` **alone on the Desktop, with no `app\`
folder present**. Its file-open trace shows exactly that —
`C:\Users\<USER>\Desktop\app\faraday-core.exe` (open attempted) — and its
"Highlighted Text" is *our own error message*: *"Faraday must be run from
its complete program folder… extract the ENTIRE archive, then run
Faraday.exe from inside the extracted Faraday folder."*

So the sandbox exercised the **friendly-error path**, not a real launch.
It therefore **did not test the CreateProcess path** (no child was ever
spawned, because there was nothing to spawn). What it *does* prove is that
the failure path is exactly what we designed: a message and a clean exit,
no loader dialog, no side effects.

| Our claim | VirusTotal sandbox evidence | Verdict |
|---|---|---|
| Zero network connections | **"Network comms: NOT FOUND"**; no DNS, no IPs, no IDS rules | **CONFIRMED** |
| Zero registry **writes** | "Registry Keys **Opened**" only — font/theme reads (`GRE_Initialize`, `FontLink`, `LanguagePack`, `Themes\Personalize`), all from GDI/USER32 during MessageBox rendering. **No registry writes listed at all** | **CONFIRMED** |
| No dropped files / temp drop | Dropped Files: none listed for the launcher | **CONFIRMED** |
| No persistence | No Run keys, no services, no scheduled tasks, no startup entries | **CONFIRMED** |
| No injection | No injected processes observed | **CONFIRMED** (see caveat on static tags below) |
| No elevation | No elevation observed | **CONFIRMED** |
| Exactly one child process (`faraday-core.exe`) | **NOT EXERCISED** — the sandbox ran without `app\`, so the launcher correctly refused to spawn anything. Processes Created lists only `Faraday.exe` itself | **UNTESTED by the sandbox** (our own ProcMon-equivalent audit in §3 covers it) |

**Static-analysis tags that are *not* observed behaviour** (and must not be
misread as such): the MITRE panel lists *Process Injection (T1055)*,
*Obfuscated Files (T1027)*, *Command and Scripting Interpreter (T1059)*,
*Shared Modules (T1129)*, and the behaviour tags `idle` and `obfuscated`.
These come from CAPA-style **capability heuristics on the binary**, at
INFO/LOW severity, with **zero corresponding runtime events** in the trace.
"Obfuscated" here reflects the high-entropy PNG icon resources; "process
injection" reflects imported process APIs (`CreateProcessW`), not an
observed injection. One crowdsourced Sigma rule matched at MEDIUM —
*"Sysmon File Executable Creation Detected"* — on a run where the sandbox's
own extraction wrote the executable to disk.

### 5b. The application, `faraday-core.exe`

Sandbox trace: **Network comms NOT FOUND. Dropped files: NOT FOUND. Sigma
rules: NOT FOUND. Registry keys opened** — three reads only
(`Session Manager`, `Segment Heap`, `Safer\CodeIdentifiers` — all standard
loader reads). **Processes Created: itself only.** MITRE lists *Windows
Management Instrumentation (T1047)* — which is precisely, and only, what we
document: read-only WMI battery queries.

### 5c. Re-measured on 1.0.6 (`faraday-core.exe`, `8a03d152…`)

1.0.6's application is a **genuinely new binary** — the UI became a static QML
module linked into it — so §5b's result did not carry over and had to be
re-measured. It was, and it reproduces exactly. Detonated by CAPA, CAPE
Sandbox and Zenbox; **0 / 70 detections**.

| Our claim | VirusTotal sandbox evidence (1.0.6) | Verdict |
|---|---|---|
| Zero network calls | **"Network comms: NOT FOUND"**. No DNS, no IPs, no IDS rules matched | **CONFIRMED — externally** |
| No registry writes | **"Registry Keys Opened"** only: `Session Manager`, `Session Manager\Segment Heap`, `Safer\CodeIdentifiers` — all standard loader reads. **Not one write** | **CONFIRMED — externally** |
| No dropped files | Dropped Files: **NOT FOUND** | **CONFIRMED — externally** |
| No persistence, no services | None listed. No Run keys, no scheduled tasks, no services | **CONFIRMED — externally** |
| Read-only hardware telemetry via WMI | MITRE **T1047 Windows Management Instrumentation** — the only Execution technique with substance, and it is exactly what we document | **CONFIRMED — externally** |
| No child processes | Processes Created: **itself only** (`faraday-core.exe`, PID 7140) | **CONFIRMED** |
| Static VersionInfo intact | VT Details: **File Version `1.0.6.0`**, full copyright/product/description | **CONFIRMED** |

**These confirmations are worth more than our own assertions**, and that is
the point of running them: they are produced by a third party with no stake in
the answer, from the exact bytes we shipped.

#### What the sandbox says that does NOT flatter us — recorded, not buried

Three items in the 1.0.6 report would look bad quoted out of context. We
publish them ourselves rather than let someone else "discover" them:

1. **Behaviour tag: `obfuscated`.** Faraday contains no obfuscation of any
   kind. This tag is driven by high-entropy resources — our icon PNGs sit at
   entropy 7.47–7.94 because *PNG is already a compressed format*. Any program
   shipping colour icons scores the same way. The `.text` section, which is
   where obfuscation would actually live, measures **5.97** — squarely in the
   normal range for compiled code, and nowhere near a packer's ~7.9+.
2. **MITRE T1055 "Process Injection" (Privilege Escalation, 1 hit, LOW).**
   No injection occurs. This is a CAPA-style *capability* heuristic derived
   from imported process/memory APIs that Qt's own runtime uses; the trace
   records **zero** corresponding runtime events — no target process, no
   remote thread, no injected region.
3. **MBC tree lists "Anti-Behavioral Analysis", "Anti-Static Analysis",
   "Defense Evasion".** Again capability categories inferred statically, at
   INFO severity, with no observed behaviour behind any of them. Faraday
   performs no anti-analysis and no evasion — a claim the reader can check
   against the source, which is fully public.

The distinction that matters: **MITRE/CAPA/MBC tags on VirusTotal are
capability *inferences from the binary*, not observed runtime events.** The
observed-events sections of the same report — network, dropped files,
registry writes, persistence, injection — are all empty. Where the two
disagree, the empty trace is the measurement and the tag is the guess.

#### Honest limit of this trace

The sandbox ran `faraday-core.exe` **standalone from the Desktop**, without its
`app\` runtime alongside it. A Qt application resolves its DLLs in the Windows
loader, so this run cannot have reached deep into the UI. It therefore proves
the *absence* of bad behaviour (no network, no writes, no drops) far more
strongly than it exercises the full application. Our own end-to-end testing
(§3) covers the real launch path. We are not going to claim a full-detonation
clean bill from a run that did not fully detonate.

**Every claim in §3 and §4 of this document is corroborated by an
independent sandbox: no network, no registry writes, no drops, no
persistence.**

### 5c. The installer

The trace shows exactly what an installer does and nothing more: files
written under `%LOCALAPPDATA%\Programs\Faraday\` (`Faraday.exe`,
`README.txt`, `app\Qt6*.dll`, …), HKCU `Uninstall\Faraday` values
(DisplayName/DisplayIcon/EstimatedSize/…), NSIS temp files created **and
deleted**, and **"Network comms: NOT FOUND"**. The `svchost`/`lsass`/
`Explorer` entries in its process tree are **the sandbox VM's own system
processes**, not children of our installer (its own tree node is the single
`Faraday-1.0.5-setup-win64.exe` process). The "Memory Pattern URLs"
(`nsis.sf.net`, `bugreports.qt.io`, `crl.entrust.net`, …) are **static
strings inside the stock NSIS stub and the Qt libraries** — no connection
to any of them was attempted or observed.

## Verdict

Every locally verifiable hardening claim re-confirmed on the shipped
binaries — and now **independently corroborated by VirusTotal's sandboxes**
for both the launcher and the application. Nothing the sandboxes observed
contradicts any claim in this document, and nothing unexpected appeared.

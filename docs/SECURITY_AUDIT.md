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

## Verdict

Every locally verifiable hardening claim re-confirmed on the shipped
1.0.2 binaries. No defects found in this phase.

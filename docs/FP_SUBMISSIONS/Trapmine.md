# False-positive report — Trapmine

Ready-to-send submission text for Trapmine (submit via the support/contact
channel at <https://trapmine.com>; attach or reference the hashes below).

---

**Subject:** False positive: `Malicious.moderate.ml.score` on unsigned
NSIS installer of open-source battery utility (Faraday)

**Product flagged:** Faraday — Battery Intelligence Suite, NSIS installer

**Files and hashes:**

| File | SHA-256 | Your verdict | Everyone else |
|---|---|---|---|
| `Faraday-1.0.2-setup-win64.exe` | `9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457` | `Malicious.moderate.ml.score` | 0 of the other 67 responding engines |
| `Faraday-1.0.1-setup-win64.exe` (previous release, same verdict) | `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99` | `Malicious.moderate.ml.score` | 0 of the other 68 |
| `faraday.exe` — the actual application your ML model has seen | `3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16` | **Undetected (your own engine)** | 0/70 overall |

**VirusTotal permalinks:**

- Installer 1.0.2: <https://www.virustotal.com/gui/file/9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457>
- Application 1.0.2 (0/70, including Trapmine): <https://www.virustotal.com/gui/file/3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16>
- Portable ZIP 1.0.2 (0 detections, identical payload): <https://www.virustotal.com/gui/file/83afe5704aaff4d348ef3d917e27f878f6f72c961ccfb9fbc7fd0eae7c69393e>

**What the application is:** a free, open-source, offline Windows battery
health monitor. It reads battery telemetry through public read-only
Windows interfaces only — WMI battery classes (`BatteryStatus`,
`BatteryFullChargedCapacity`, …), the stock `powercfg /batteryreport`
tool, and ACPI thermal zones via WMI. It contains **zero network code**
(verifiable in the PE import tables: no ws2_32/wininet/winhttp/Qt6Network
anywhere in the application), never elevates (`asInvoker` manifest;
per-user `RequestExecutionLevel user` installer), writes no registry keys
from the app (JSON settings file), installs no drivers or services, and
performs no process injection.

**About the flagged file specifically:** stock, unmodified NSIS 3.12
release stubs with standard `/SOLID lzma` compression. Full VersionInfo
resource (CompanyName/ProductName/FileDescription/FileVersion/
OriginalFilename/InternalName). Modern UI 2 standard page flow. The
install script contains **no `Exec`/`ExecShell`/`ExecWait` of any kind** —
it copies files, creates Start-menu shortcuts and the standard HKCU
uninstall entry, nothing else. The complete NSIS script is public in the
project repository (`installer/faraday.nsi`). The file is unsigned because
this is an unfunded open-source project; nothing about it is packed,
obfuscated or hidden.

**Supporting evidence:** every one of the 102 files the installer drops
was scanned individually on VirusTotal — 0 detections on all of them. The
identical payload distributed as a plain ZIP scans 0 detections. Your own
engine returns Undetected for the application executable; the ML hit
applies only to the NSIS wrapper around clean, individually-verified
contents.

**Request:** please review the `Malicious.moderate.ml.score`
classification of the installer hashes above and whitelist/correct them.
We are happy to provide the NSIS script, build instructions for a
reproducible build, or any additional artifacts.

---

*Maintainer note: re-submit for each new release until reputation accrues;
update the hash table above from docs/VIRUSTOTAL_BASELINE.md.*

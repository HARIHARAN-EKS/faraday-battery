# False-positive report — Trapmine

Ready-to-send submission text for Trapmine (submit via the support/contact
channel at <https://trapmine.com>). Trapmine has flagged **only the NSIS
installer wrapper**, across four releases, with a generic ML score — and
clears every binary the installer contains.

Refreshed for **1.0.6** (scanned 2026-07-12).

---

**Subject:** False positive: ML score on the unsigned NSIS installer of an
open-source battery utility (Faraday 1.0.6)

| File | SHA-256 | Your verdict |
|---|---|---|
| `Faraday-1.0.6-setup-win64.exe` (current) | `be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b` | `Suspicious.low.ml.score` |
| `Faraday-1.0.5-setup-win64.exe` | `8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d` | `Suspicious.low.ml.score` |
| `Faraday-1.0.2-setup-win64.exe` | `9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457` | `Malicious.moderate.ml.score` |
| `Faraday-1.0.1-setup-win64.exe` | `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99` | `Malicious.moderate.ml.score` |
| `faraday-core.exe` (the application, 1.0.6) | `8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98` | **Undetected (your own engine)** — 0/70 overall |
| `Faraday.exe` (the launcher, 1.0.6) | `211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987` | **Undetected (your own engine)** |
| `Faraday-1.0.6-portable-win64.zip` (identical payload, no installer) | `45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115` | — (**0 detections overall**) |

VirusTotal permalinks:
- Installer 1.0.6: <https://www.virustotal.com/gui/file/be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b>
- Application 1.0.6 (0/70): <https://www.virustotal.com/gui/file/8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98>
- Portable ZIP 1.0.6 (0 detections): <https://www.virustotal.com/gui/file/45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115>

**Near-universal clean result:** 67 of 69 engines clear this installer, and
all 99 files it writes to disk scan 0/N individually.

**The core of the case:** your engine clears every binary the installer
contains, and the identical payload shipped as a plain ZIP scans 0 detections.
Only the NSIS wrapper is scored. (We note and appreciate that your score
softened from `Malicious.moderate` to `Suspicious.low` and has held there.)

**What the installer is.** Stock, unmodified NSIS 3.12 release stubs,
standard `/SOLID lzma`. Per-user (`RequestExecutionLevel user`) — never
elevates. Full VersionInfo including OriginalFilename/InternalName
(FileVersion `1.0.6.0`). The script contains **no `Exec`/`ExecShell`/
`ExecWait` of anything** — it copies files, writes Start-menu shortcuts and
the standard HKCU uninstall entry, nothing else; there is no "run now"
checkbox. The script is public (`installer/faraday.nsi`). VirusTotal's sandbox
trace shows precisely this, with **"Network comms: NOT FOUND"**.

**What the product is.** A free, open-source, fully offline Windows
battery-health monitor. The launcher validates a file manifest and starts
`app\faraday-core.exe` in its own program folder; the application reads
battery telemetry through public read-only Windows interfaces only — WMI
battery classes, the stock `powercfg /batteryreport` tool, and ACPI thermal
zones via WMI. **Zero network code** (verifiable in the PE import tables), no
elevation, no drivers or services, no registry writes by the application, no
process injection.

**Hardening applied:** no packing or obfuscation (the high-entropy `.rsrc`
is PNG-compressed icons; the high-entropy overlay is the LZMA payload every
NSIS installer carries; `.text` entropy is an ordinary 6.45), full
VersionInfo, `asInvoker`/per-user manifests, no W+X sections, read-only
hardware access. Unsigned only because this is an unfunded open-source
project; nothing is faked, hidden, or evaded. Source and build scripts:
<https://github.com/HARIHARAN-EKS/faraday-battery>

**Request:** please review the ML classification of the installer hashes
above and whitelist/correct them. We can provide the NSIS script, build
instructions, or any additional artifacts.

---

*Maintainer note: re-submit for each new release until reputation accrues;
update the hash table from docs/VIRUSTOTAL_BASELINE.md.*

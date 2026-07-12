# False-positive report — Trapmine

Ready-to-send submission text for Trapmine (submit via the support/contact
channel at <https://trapmine.com>). Trapmine has flagged **only the NSIS
installer wrapper**, across three releases, with a generic ML score — and
clears every binary the installer contains.

---

**Subject:** False positive: ML score on the unsigned NSIS installer of an
open-source battery utility (Faraday)

| File | SHA-256 | Your verdict |
|---|---|---|
| `Faraday-1.0.5-setup-win64.exe` (current) | `8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d` | `Suspicious.low.ml.score` |
| `Faraday-1.0.2-setup-win64.exe` (previous) | `9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457` | `Malicious.moderate.ml.score` |
| `Faraday-1.0.1-setup-win64.exe` | `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99` | `Malicious.moderate.ml.score` |
| `faraday-core.exe` (the application) | `f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b` | **Undetected (your own engine)** |
| `Faraday.exe` (the launcher) | `a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1` | **Undetected (your own engine)** |
| `Faraday-1.0.5-portable-win64.zip` (identical payload, no installer) | `f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe` | — (**0 detections overall**) |

VirusTotal permalinks:
- Installer 1.0.5: <https://www.virustotal.com/gui/file/8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d>
- Application 1.0.5 (0 detections): <https://www.virustotal.com/gui/file/f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b>
- Portable ZIP 1.0.5 (0 detections): <https://www.virustotal.com/gui/file/f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe>

**The core of the case:** your engine clears every binary the installer
contains, and the identical payload shipped as a plain ZIP scans 0
detections. Only the NSIS wrapper is scored. (We note and appreciate that
the score has softened from `Malicious.moderate` to `Suspicious.low`.)

**What the installer is.** Stock, unmodified NSIS 3.12 release stubs,
standard `/SOLID lzma`. Per-user (`RequestExecutionLevel user`) — never
elevates. Full VersionInfo including OriginalFilename/InternalName. The
script contains **no `Exec`/`ExecShell`/`ExecWait` of anything** — it copies
files, writes Start-menu shortcuts and the standard HKCU uninstall entry,
nothing else; there is no "run now" checkbox. The script is public
(`installer/faraday.nsi`). VirusTotal's sandbox trace of it shows precisely
this, with **"Network comms: NOT FOUND"**.

**What the product is.** A free, open-source, fully offline Windows
battery-health monitor. It reads battery telemetry through public read-only
Windows interfaces only — WMI battery classes, the stock
`powercfg /batteryreport` tool, and ACPI thermal zones via WMI. **Zero
network code** (verifiable in the PE import tables), no elevation, no
drivers or services, no registry writes by the application, no process
injection.

**Hardening applied:** no packing or obfuscation (the high-entropy `.rsrc`
is PNG-compressed icons; the high-entropy overlay is the LZMA payload every
NSIS installer carries; `.text` entropy is an ordinary 6.45), full
VersionInfo, `asInvoker`/per-user manifests, no W+X sections, read-only
hardware access. Unsigned only because this is an unfunded open-source
project; nothing is faked or hidden.

**Request:** please review the ML classification of the installer hashes
above and whitelist/correct them. We can provide the NSIS script, build
instructions, or any additional artifacts.

---

*Maintainer note: re-submit for each new release until reputation accrues;
update the hash table from docs/VIRUSTOTAL_BASELINE.md.*

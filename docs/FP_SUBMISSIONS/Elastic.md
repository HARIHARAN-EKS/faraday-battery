# False-positive report — Elastic Security

Ready-to-send submission. Elastic flags **only the NSIS installer wrapper**;
it reports every file that installer contains — including the application —
as Undetected.

Refreshed for **1.0.6** (scanned 2026-07-12). Elastic's confidence **softened**
from `Malicious (high Confidence)` in 1.0.5 to `Malicious (moderate
Confidence)` in 1.0.6 — with no change made to court it.

---

**Subject:** False positive: `Malicious (moderate Confidence)` on the unsigned
NSIS installer of an open-source battery utility (Faraday 1.0.6)

| File | SHA-256 | Your verdict | Everyone else |
|---|---|---|---|
| `Faraday-1.0.6-setup-win64.exe` | `be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b` | **`Malicious (moderate Confidence)`** | 67 of 69 engines clean (only Trapmine also flags, as a *low* ML score) |
| `faraday-core.exe` (the application it installs) | `8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98` | **Undetected (your own engine)** | **0 / 70** |
| `Faraday.exe` (the launcher it installs) | `211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987` | **Undetected (your own engine)** | 1/70 (a different vendor's generic label) |
| `Faraday-1.0.6-portable-win64.zip` (identical payload, no installer) | `45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115` | **Undetected (your own engine)** | **0 detections** |

VirusTotal permalinks:
- Installer: <https://www.virustotal.com/gui/file/be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b>
- Application: <https://www.virustotal.com/gui/file/8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98>
- Portable ZIP (same payload, 0 detections): <https://www.virustotal.com/gui/file/45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115>

**The core of the case:** your engine clears **every single file** the
installer writes to disk (all 99 of them, 0/N each), and clears the identical
payload when it is delivered as a plain ZIP. The verdict applies only to the
NSIS wrapper around contents your own engine considers clean.

**What the installer is.** Stock, unmodified **NSIS 3.12** release stubs
with standard `/SOLID lzma` compression. Per-user install
(`RequestExecutionLevel user`) — it never elevates. Full VersionInfo
resource (CompanyName / ProductName / FileDescription / FileVersion `1.0.6.0` /
OriginalFilename / InternalName). The script contains **no `Exec`,
`ExecShell` or `ExecWait` of anything**: it copies files, creates
Start-menu shortcuts, writes the standard HKCU uninstall entry, and stops.
There is deliberately no "run now" checkbox. The complete NSIS script is
public in the project repository (`installer/faraday.nsi`).

VirusTotal's own sandbox trace shows exactly that and nothing more: files
written under `%LOCALAPPDATA%\Programs\Faraday\`, HKCU `Uninstall\Faraday`
values, NSIS temp files created and deleted, and **"Network comms: NOT
FOUND"**.

**What the product is.** Faraday is a free, open-source, fully offline
Windows battery-health monitor: read-only battery telemetry via WMI battery
classes, the stock `powercfg /batteryreport` tool, and ACPI thermal zones.
Zero network code in any shipped binary (verifiable in the import tables),
no elevation, no drivers or services, no registry writes by the application
itself.

**Hardening applied:** no packing or obfuscation anywhere; the `.rsrc`
entropy (7.91) is PNG-compressed application icons, and the high-entropy
overlay is the LZMA payload every NSIS installer carries; code sections are
ordinary (`.text` entropy 6.45). Full VersionInfo, `asInvoker`/per-user
manifests, no W+X sections, zero network imports. Unsigned only because this
is an unfunded open-source project — nothing is faked, hidden, or evaded.
Source and build scripts: <https://github.com/HARIHARAN-EKS/faraday-battery>

**Request:** please review the `Malicious (moderate Confidence)`
classification of
`be732f69f387412ec8bd79df4499f04ddd6b640115187714c6bf544c3b66406b`. We are
happy to provide the NSIS script, the build recipe, or any additional artifact.

---

*Maintainer note: Elastic first flagged us in 1.0.5 (it cleared the 1.0.1 and
1.0.2 installers) and softened one step in 1.0.6. Re-submit for each release;
update hashes from docs/VIRUSTOTAL_BASELINE.md.*

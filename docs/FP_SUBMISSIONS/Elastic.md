# False-positive report — Elastic Security

Ready-to-send submission. Elastic flags **only the NSIS installer wrapper**;
it reports every file that installer contains — including the application —
as Undetected.

---

**Subject:** False positive: `Malicious (high Confidence)` on the unsigned
NSIS installer of an open-source battery utility (Faraday 1.0.5)

| File | SHA-256 | Your verdict | Everyone else |
|---|---|---|---|
| `Faraday-1.0.5-setup-win64.exe` | `8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d` | **`Malicious (high Confidence)`** | 67 of 69 engines clean (only Trapmine also flags, as a *low* ML score) |
| `faraday-core.exe` (the application it installs) | `f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b` | **Undetected (your own engine)** | 0 detections |
| `Faraday.exe` (the launcher it installs) | `a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1` | **Undetected (your own engine)** | 1/70 (a different vendor's generic label) |
| `Faraday-1.0.5-portable-win64.zip` (identical payload, no installer) | `f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe` | **Undetected (your own engine)** | **0 detections** |

VirusTotal permalinks:
- Installer: <https://www.virustotal.com/gui/file/8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d>
- Application: <https://www.virustotal.com/gui/file/f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b>
- Portable ZIP (same payload, 0 detections): <https://www.virustotal.com/gui/file/f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe>

**The core of the case:** your engine clears **every single file** the
installer writes to disk, and clears the identical payload when it is
delivered as a plain ZIP. The verdict applies only to the NSIS wrapper
around contents your own engine considers clean.

**What the installer is.** Stock, unmodified **NSIS 3.12** release stubs
with standard `/SOLID lzma` compression. Per-user install
(`RequestExecutionLevel user`) — it never elevates. Full VersionInfo
resource (CompanyName / ProductName / FileDescription / FileVersion /
OriginalFilename / InternalName). The script contains **no `Exec`,
`ExecShell` or `ExecWait` of anything**: it copies files, creates
Start-menu shortcuts, writes the standard HKCU uninstall entry, and stops.
There is deliberately no "run now" checkbox. The complete NSIS script is
public in the project repository (`installer/faraday.nsi`).

VirusTotal's own sandbox trace of this installer shows exactly that and
nothing more: files written under `%LOCALAPPDATA%\Programs\Faraday\`, HKCU
`Uninstall\Faraday` values, NSIS temp files created and deleted, and
**"Network comms: NOT FOUND"**.

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
manifests, no W+X sections. Unsigned only because this is an unfunded
open-source project — nothing is faked or hidden.

**Request:** please review the `Malicious (high Confidence)` classification
of `8007b30a8a3f0eac779aa2df6ed5dcf7a751f437bbe030b47afd2e6888ee7a2d`. We
are happy to provide the NSIS script, the build recipe, or any additional
artifact.

---

*Maintainer note: this is a NEW detection in 1.0.5 — Elastic cleared the
1.0.1 and 1.0.2 installers. Re-submit for each release; update hashes from
docs/VIRUSTOTAL_BASELINE.md.*

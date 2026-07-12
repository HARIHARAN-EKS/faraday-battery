# False-positive report — Arctic Wolf

Ready-to-send submission. Arctic Wolf flags **only the launcher stub**, and
reports the actual application as Undetected.

Refreshed for **1.0.6** (scanned 2026-07-12).

---

**Subject:** False positive: `Unsafe` on the 130 KB launcher stub of an
open-source battery utility (Faraday 1.0.6)

| File | SHA-256 | Your verdict | Everyone else |
|---|---|---|---|
| `Faraday.exe` (launcher, 130.17 KB) | `211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987` | **`Unsafe`** | 0 of the other 69 engines |
| `faraday-core.exe` (the actual application) | `8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98` | **Undetected (your own engine)** | **0 / 70** |
| `Faraday-1.0.6-portable-win64.zip` (the full product) | `45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115` | — | **0 detections** (all 117 bundled files 0/N) |

VirusTotal permalinks:
- Launcher: <https://www.virustotal.com/gui/file/211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987>
- Application: <https://www.virustotal.com/gui/file/8a03d15232cad5b3ed40e68398a0f0aeffd7ab25ee5fcdf66327440583018b98>
- Portable ZIP: <https://www.virustotal.com/gui/file/45bba4d0c258a14db5304e8a15cac4c56cfdbd6d14565bb0a6d5aa638a7fb115>

**Near-universal clean result:** 69 of 70 engines — including every major
vendor — report this file as clean, and your own engine reports the
application it launches, and the ZIP containing both, as clean.

**What the flagged binary is.** `Faraday.exe` is a launcher stub. It exists
for one reason: the Qt application's DLL imports are resolved by the Windows
*loader* before `main()` runs, so a user who copied the app exe out of its
folder got a raw *"Qt6Gui.dll was not found"* system error. The stub links
only against KERNEL32/USER32/msvcrt (so it always starts), verifies that the
files listed in `app\runtime.manifest` are present, and then starts
`app\faraday-core.exe` — a fixed-name sibling inside its own program folder.
If anything is missing it shows a plain-language message and exits.

That is its entire behaviour: **read a manifest file, then `CreateProcess`
one known sibling binary.** No shell execution, no temp-file drop, no
download, no injection, no elevation, no registry writes, no network code
of any kind. VirusTotal's own sandbox trace confirms it: **"Network comms:
NOT FOUND"**, no dropped files, no persistence, registry keys **read** only.

**What the product is.** Faraday is a free, open-source, fully offline
Windows battery-health monitor. It reads battery telemetry through public,
read-only Windows interfaces only: WMI battery classes, the stock
`powercfg /batteryreport` tool, and ACPI thermal zones via WMI. It has zero
network code (verifiable in the PE import tables of every shipped binary —
this launcher imports only KERNEL32, msvcrt and USER32), never elevates
(`asInvoker` manifest; per-user installer), installs no drivers or services,
and writes no registry keys.

**Hardening applied:** no packing or obfuscation (plain `-static` MinGW
release build; `.text` entropy **5.87** — the high-entropy `.rsrc` is simply
PNG-compressed application icons), full VersionInfo on every PE (this one
reads `1.0.6.0`), `asInvoker` manifests, no W+X sections, read-only hardware
access. The file is unsigned only because this is an unfunded open-source
project; nothing about it is hidden or misrepresented. Source and build
scripts are public: <https://github.com/HARIHARAN-EKS/faraday-battery>

**Request:** please review the `Unsafe` classification of
`211b530c377e9d6b34e34342fc90a76c2907e7de6747dbdc15c51488f3806987`. Your
own engine already clears the application it launches; the stub performs a
strict subset of that program's behaviour. We are happy to supply the source
of the launcher (it is ~300 lines of plain Win32 C++), the build recipe, or
any additional artifact.

---

*Maintainer note: re-submit for each release; update hashes from
docs/VIRUSTOTAL_BASELINE.md.*

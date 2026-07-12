# False-positive report — Arctic Wolf

Ready-to-send submission. Arctic Wolf flags **only the launcher stub**, and
reports the actual application as Undetected.

---

**Subject:** False positive: `Unsafe` on the 130 KB launcher stub of an
open-source battery utility (Faraday 1.0.5)

| File | SHA-256 | Your verdict | Everyone else |
|---|---|---|---|
| `Faraday.exe` (launcher, 129.67 KB) | `a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1` | **`Unsafe`** | 0 of the other 69 engines |
| `faraday-core.exe` (the actual application) | `f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b` | **Undetected (your own engine)** | 0 detections overall |
| `Faraday-1.0.5-portable-win64.zip` (the full product) | `f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe` | — | 0 detections |

VirusTotal permalinks:
- Launcher: <https://www.virustotal.com/gui/file/a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1>
- Application: <https://www.virustotal.com/gui/file/f92311e9c60f98a2b9128a2f2b5984e184a97cb4a612f379a1a815af339dae7b>
- Portable ZIP: <https://www.virustotal.com/gui/file/f81074358f0d64c2312e120c5ae598af297ebdfd51dce785e305e8971d3029fe>

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
network code (verifiable in the PE import tables of every shipped binary),
never elevates (`asInvoker` manifest; per-user installer), installs no
drivers or services, and writes no registry keys.

**Hardening applied:** no packing or obfuscation (plain `-static` MinGW
release build; `.text` entropy 5.87 — the high-entropy `.rsrc` is simply
PNG-compressed application icons), full VersionInfo on every PE,
`asInvoker` manifests, no W+X sections, read-only hardware access. The file
is unsigned only because this is an unfunded open-source project; nothing
about it is hidden or misrepresented. Source and build scripts are public.

**Request:** please review the `Unsafe` classification of
`a3f6ca32a3889e91842bec8fd8878d1aa8873061b42314852cadc82dbd75f9d1`. Your
own engine already clears the application it launches; the stub performs a
strict subset of that program's behaviour. We are happy to supply the source
of the launcher (it is ~300 lines of plain Win32 C++), the build recipe, or
any additional artifact.

---

*Maintainer note: re-submit for each release; update hashes from
docs/VIRUSTOTAL_BASELINE.md.*

# Antivirus hardening

Faraday ships without a code-signing certificate, so its entire AV posture
rests on being *structurally* unsuspicious. Every measure below is verified in
the release process; several are enforced by tests.

## Measures taken

| # | Measure | Implementation / verification |
|---|---|---|
| 1 | **No packing, no obfuscation** | Plain MinGW `-O3` Release build straight from Ninja. No UPX, no self-extracting stubs, no encrypted sections |
| 2 | **Zero network calls** | No networking code anywhere in the source; no update checker, no telemetry, no crash uploads. Verified on the release exe: the import table contains **no** `ws2_32`, `wininet`, `winhttp` or `Qt6Network` (checked with `objdump -p`). `Qt6Network.dll` is present in the *distribution* only because `Qt6Qml.dll` links it as a framework dependency; the application never loads or calls it |
| 3 | **Read-only hardware access** | WMI WQL `SELECT` queries and `powercfg` output parsing only. No driver installation, no kernel access, no IOCTLs, no process injection, no writes to any device |
| 4 | **Never elevated** | Embedded manifest sets `requestedExecutionLevel level="asInvoker"`; verified in the binary. Nothing in the app requests or requires admin. The NSIS installer is also per-user (`RequestExecutionLevel user`) |
| 5 | **No registry writes by the app** | Settings live in `%APPDATA%\Faraday Project\Faraday\settings.json`; history in SQLite next to it. Autostart is an *opt-in Startup-folder shortcut*, not a Run key. (The installer writes only the standard per-user HKCU uninstall entry that Windows needs to list the uninstaller — the app itself never touches the registry) |
| 6 | **Full version-info resource** | `CompanyName`, `ProductName`, `FileDescription`, `FileVersion`, `OriginalFilename`, `LegalCopyright` embedded via `resources/faraday.rc`, plus a proper multi-size icon. Anonymous, resource-less executables are a common heuristic trigger |
| 7 | **Clean installer** | NSIS with LZMA, no bundled third-party software, no download stubs, standard per-user install path (`%LOCALAPPDATA%\Programs\Faraday`), working uninstaller that removes everything it installed (and the optional autostart shortcut) while preserving user data |
| 8 | **Deterministic, foreground behavior** | One process, no child processes except the stock Windows `powercfg.exe`, no services, no scheduled tasks, no hooks |

## Residual heuristics an unsigned app can still trip

- **WMI usage** — Faraday queries battery classes; some heuristics score any
  WMI activity. The queries are plain `SELECT`s of documented classes.
- **Shortcut creation** — the opt-in autostart writes `Faraday.lnk` into the
  user's Startup folder via `IShellLink`. It only happens when the user flips
  the switch, and the uninstaller removes it.
- **Unsigned binary** — the single biggest factor, unavoidable without a
  certificate.

## VirusTotal guidance

Before distributing a build:

1. Upload both `Faraday-<ver>-setup-win64.exe` and the portable ZIP's
   `faraday.exe` to <https://www.virustotal.com>.
2. Expect 0 detections from major engines; 1–3 hits from low-reputation
   engines ("ML/heuristic", "not-a-virus", "unsafe") are the normal unsigned-
   binary noise level. Investigate anything from Microsoft, Kaspersky,
   Bitdefender, ESET or Sophos seriously before shipping.
3. Record the analysis link in the release notes so users can verify the exact
   hashes they downloaded.

## If a user reports a flag

1. Reproduce: check the file's SHA-256 against the released hash — a mismatch
   means a corrupted or tampered download, not a false positive.
2. Submit the file as a false positive to the specific vendor (all major
   vendors run FP portals: Microsoft via the WDSI submission page, Kaspersky
   via OpenTip, ESET via `samples@eset.com`, etc.).
3. Vendors typically clear clean unsigned utilities within days;
   re-submission of each new release may be needed until the product builds
   reputation.
4. Users can locally whitelist the install folder in the interim; never ask
   them to disable their AV globally.

## Future improvements

- An OV/EV code-signing certificate (or Azure Trusted Signing) would remove
  most residual friction and enable SmartScreen reputation to accrue faster.
- Reproducible builds so third parties can verify the shipped binaries match
  the source.

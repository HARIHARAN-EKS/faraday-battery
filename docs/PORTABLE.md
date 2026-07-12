# Portable distribution (zero-install)

The portable ZIP is a **first-class distribution channel**, not an
afterthought — and it is the **recommended download for anyone whose
antivirus objects to the installer**. The application executable inside it
scanned 0/70 on VirusTotal (see VIRUSTOTAL_BASELINE.md); the single 1/69
generic-heuristic hit on 1.0.1 applied only to the NSIS installer wrapper,
which the ZIP does not use at all.

## How it works

1. Download `Faraday-<version>-portable-win64.zip`.
2. Extract the `Faraday` folder anywhere — `C:\Tools`, a folder with
   spaces, a USB stick, a network share. No installer runs, nothing is
   written outside the folder, no admin rights are needed.
3. Run `faraday.exe`.

The ZIP ships a marker file, **`portable.txt`**, next to the executable.
When that marker is present, ALL application data — `settings.json` and
the `faraday.sqlite` history database — lives in the **`data`** subfolder
beside the exe, so your settings and battery history travel with the
folder (plug the USB stick into another machine and everything is there).

Without the marker (i.e. an installed copy), data lives in the standard
per-user location `%APPDATA%\Faraday Project\Faraday` instead. Delete
`portable.txt` if you prefer that behavior for an extracted copy.

## Uninstalling the portable version

Delete the folder. That is the entire uninstall: Faraday writes no
registry keys, installs no services, and creates no files outside its
folder in portable mode. (If you enabled "Launch with Windows", flip that
switch off first — it creates one shortcut in your Startup folder, which
is also removed by the switch.)

## Notes and limits

- **Read-only media**: the app runs, but settings and history cannot be
  saved. Every failure is handled; nothing crashes.
- **Autostart** from removable media will only work while the drive is
  attached with the same letter — prefer autostart with installed copies.
- Exports (CSV/JSON/HTML) still default to `Documents\Faraday exports`
  on the host machine, since reports are usually meant to stay there.

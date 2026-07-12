# Portable distribution (zero-install)

The portable ZIP is a **first-class distribution channel**, not an
afterthought — and it is the **recommended download for anyone whose
antivirus objects to the installer**. The application executable inside it
scanned 0/70 on VirusTotal (see VIRUSTOTAL_BASELINE.md); the single generic
ML hit seen on earlier releases applied only to the NSIS installer wrapper,
which the ZIP does not use at all.

## How to run it

1. Download `Faraday-<version>-portable-win64.zip`.
2. Extract the **whole** `Faraday` folder anywhere — `C:\Tools`, a folder
   with spaces, a USB stick, a network share. No installer runs, nothing is
   written outside the folder, no admin rights are needed.
3. Run **`Faraday.exe`**. That is the only thing to click.

## What is in the folder

```
Faraday\
  Faraday.exe     <- the launcher: the ONE thing you run
  README.txt
  app\            <- the Qt application (faraday-core.exe) and its runtime
  data\           <- your settings and battery history (created on first run)
```

Only two items ship at the top level. Everything the program needs lives in
`app\`, deliberately out of sight; `data\` appears next to `Faraday.exe` the
first time you run it.

**`Faraday.exe` is a launcher, not the whole program.** It verifies that the
runtime in `app\` is complete and then starts the real application. If you
copy `Faraday.exe` out on its own — or extract the ZIP only partially — it
tells you so in plain language instead of failing with a raw Windows
"DLL was not found" error.

## Where your data lives

With the portable marker present (`app\portable.txt`, shipped in the ZIP),
**all** application data — `settings.json` and the `faraday.sqlite` history
database — lives in the **top-level `data\` folder**, beside `Faraday.exe`.

That location is deliberate: user data stays **out of `app\`**, so it is
never mixed with the runtime, and you can replace `app\` wholesale (e.g. a
newer build) without touching your history. Plug the USB stick into another
machine and everything is there.

Installed copies ship no marker and use the standard per-user location
`%APPDATA%\Faraday Project\Faraday` instead. Delete `app\portable.txt` if
you prefer that behavior for an extracted copy.

## Uninstalling the portable version

Delete the folder. That is the entire uninstall: Faraday writes no registry
keys, installs no services, and creates no files outside its folder in
portable mode. (If you enabled "Launch with Windows", flip that switch off
first — it creates one shortcut in your Startup folder, which the switch
also removes.)

## Notes and limits

- **Read-only media**: the app runs, but settings and history cannot be
  saved. Every failure is handled; nothing crashes.
- **Autostart** from removable media only works while the drive is attached
  with the same letter — prefer autostart with installed copies.
- Exports (CSV/JSON/HTML) still default to `Documents\Faraday exports` on
  the host machine, since reports are usually meant to stay there.

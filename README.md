# Faraday — Battery Intelligence Suite

A free, fully offline Windows battery health suite: dashboard with a
color-coded health ring, live charge monitoring with trend extrapolation,
capacity-decline history, alerts, guided calibration and exportable reports.

## Download

1. **Portable ZIP (recommended):** `dist\Faraday-1.0.5-portable-win64.zip`
   — extract the whole folder and run **`Faraday.exe`**. That is the only
   thing to click. No install step, no admin rights; settings and history
   live in the `data\` folder beside it (USB-friendly), and deleting the
   folder uninstalls it. See [docs/PORTABLE.md](docs/PORTABLE.md).
2. **Installer (alternative):** `dist\Faraday-1.0.5-setup-win64.exe` —
   per-user, never elevates, clean uninstaller. Honest note: the installer
   is unsigned (no certificate is available to this free project), so one
   AV engine's ML heuristic flags the NSIS wrapper — the payload itself
   scans 0/70. Details and hashes:
   [docs/FALSE_POSITIVE_RESPONSE.md](docs/FALSE_POSITIVE_RESPONSE.md).

The extracted folder looks like this:

```
Faraday\
  Faraday.exe     <- run this
  README.txt
  app\            <- the program's runtime; not meant to be opened
  data\           <- your settings and battery history (created on first run)
```

`Faraday.exe` is a small launcher; the application lives in `app\`. Copying
`Faraday.exe` out on its own will not work — it will tell you so with a
clear message rather than a confusing Windows error.

- **Documentation:** [docs/README.md](docs/README.md)
- **Build instructions:** [docs/BUILD.md](docs/BUILD.md)
- **Security / AV posture:** [docs/AV_HARDENING.md](docs/AV_HARDENING.md)
- **Scan baseline:** [docs/VIRUSTOTAL_BASELINE.md](docs/VIRUSTOTAL_BASELINE.md)
- **Field testing:** [docs/FIELD_TESTING.md](docs/FIELD_TESTING.md)

Quick start (developers):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Test
powershell -ExecutionPolicy Bypass -File package.ps1 -Zip -Installer
```

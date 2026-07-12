# Faraday â€” Battery Intelligence Suite

A free, fully offline Windows battery health suite: dashboard with a
color-coded health ring, live charge monitoring with trend extrapolation,
capacity-decline history, alerts, guided calibration and exportable reports.

## Download

1. **Portable ZIP (recommended):** `dist\Faraday-1.0.4-portable-win64.zip`
   â€” extract anywhere and run `faraday.exe`. No install step, no admin
   rights, settings and history travel with the folder (USB-friendly);
   delete the folder to uninstall. VirusTotal (1.0.2 round): 0 detections,
   whole archive and every file inside. See [docs/PORTABLE.md](docs/PORTABLE.md).
2. **Installer (alternative):** `dist\Faraday-1.0.4-setup-win64.exe` â€”
   per-user, never elevates, clean uninstaller. Honest note: the installer
   is unsigned (no certificate is available to this free project), so one
   AV engine's ML heuristic flags the NSIS wrapper â€” the payload itself
   scans 0/70. Details and hashes:
   [docs/FALSE_POSITIVE_RESPONSE.md](docs/FALSE_POSITIVE_RESPONSE.md).

- **Documentation:** [docs/README.md](docs/README.md)
- **Build instructions:** [docs/BUILD.md](docs/BUILD.md)
- **Security / AV posture:** [docs/AV_HARDENING.md](docs/AV_HARDENING.md)
- **Scan baseline:** [docs/VIRUSTOTAL_BASELINE.md](docs/VIRUSTOTAL_BASELINE.md)
- **Field testing:** [docs/FIELD_TESTING.md](docs/FIELD_TESTING.md)

Quick start (developers):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Test
```

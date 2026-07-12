# Faraday — Battery Intelligence Suite

A free, fully offline Windows battery health suite: dashboard with a
color-coded health ring, live charge monitoring with trend extrapolation,
capacity-decline history, alerts, guided calibration and exportable reports.

- **Documentation:** [docs/README.md](docs/README.md)
- **Build instructions:** [docs/BUILD.md](docs/BUILD.md)
- **Security / AV posture:** [docs/AV_HARDENING.md](docs/AV_HARDENING.md)

Quick start (developers):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1 -Test
```

Artifacts: `dist\Faraday-1.0.2-portable-win64.zip` (portable, zero-install —
**recommended**, see [docs/PORTABLE.md](docs/PORTABLE.md)) and
`dist\Faraday-1.0.2-setup-win64.exe` (per-user installer).

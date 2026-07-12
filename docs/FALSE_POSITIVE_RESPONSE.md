# If your antivirus flags the Faraday installer

Short version: **the application is verifiably clean; one AV engine's
machine-learning model dislikes the unsigned installer wrapper.** Here are
the facts, the hashes to check, and what you (or the vendor) can do.

## The measured facts (VirusTotal, 2026-07-12)

| File | VirusTotal verdict |
|---|---|
| `faraday.exe` 1.0.1 — the actual application | **0 / 70. No vendor flagged it.** |
| Every one of the 102 files the installer places on disk | **0 detections each** |
| `Faraday-1.0.1-setup-win64.exe` — the NSIS installer wrapper | 1 / 69 — Trapmine: `Malicious.moderate.ml.score` |

- Application: <https://www.virustotal.com/gui/file/b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff>
  (SHA-256 `b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff`)
- Installer: <https://www.virustotal.com/gui/file/aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99>
  (SHA-256 `aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99`)

`Malicious.moderate.ml.score` is not a malware family — it is the name of
a machine-learning confidence bucket. None of the other 68 engines
(including Microsoft Defender, Kaspersky, BitDefender, ESET, Sophos,
CrowdStrike and Elastic) agree with it. This is the textbook profile of a
false positive on an unsigned installer.

Why unsigned? Code-signing certificates cost money this free project does
not have. Faraday compensates structurally: no network code at all, no
elevation ever (`asInvoker`), read-only hardware access, no registry
writes by the app, no packing/obfuscation — all documented and verifiable
in [AV_HARDENING.md](AV_HARDENING.md).

## What you can do

1. **Verify your download.** Compare your file's SHA-256
   (PowerShell: `Get-FileHash <file>`) against the published release
   hashes. A mismatch means a corrupted or tampered download — delete it.
   A match means you have the exact bytes analyzed above.
2. **Use the portable ZIP instead** — the recommended path if your AV
   objects. It contains no installer stub (the only flagged component),
   needs no install step and no admin rights, and its payload scanned
   0/70. See [PORTABLE.md](PORTABLE.md).
3. **Whitelist locally** (optional): exclude the Faraday install folder in
   your AV product. Never disable your AV globally for this or anything
   else.

## For AV vendors / reporting the false positive

The flagging vendor is **Trapmine**. False-positive reports can be
submitted to Trapmine support (<https://trapmine.com> — support/contact
channel) with the installer SHA-256 above. Useful context for any analyst:

- NSIS 3.12, stock unmodified stubs, standard `/SOLID lzma`.
- Full VersionInfo resource; per-user `RequestExecutionLevel user`;
  no `Exec`/`ExecShell` anywhere in the script; the complete NSIS script
  is public in the repository (`installer\faraday.nsi`).
- Payload: 102 files, individually 0-detection on VirusTotal.
- The application binary imports no networking DLLs and performs no
  network activity of any kind.

Most vendors clear clean unsigned utilities within days of an FP report;
each new release may need re-submission until the product accrues
reputation.

# If your antivirus flags the Faraday installer

Short version: **the application is verifiably clean; one AV engine's
machine-learning model dislikes the unsigned installer wrapper.** This has
now been measured across two releases. Here are the facts, the hashes to
check, and what you (or the vendor) can do.

## The measured facts (VirusTotal, 2026-07-12, both release rounds)

| File | VirusTotal verdict |
|---|---|
| `faraday.exe` 1.0.2 — the actual application | **0 / 70. No vendor flagged it** (1.0.1 was also 0/70) |
| `Faraday-1.0.2-portable-win64.zip` — recommended download | **0 detections**, and every file inside it scans 0/N individually |
| `Faraday-1.0.2-setup-win64.exe` — the NSIS installer wrapper | 1 / 68 — Trapmine: `Malicious.moderate.ml.score` (identical single hit on 1.0.1) |

- Application 1.0.2: <https://www.virustotal.com/gui/file/3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16>
  (SHA-256 `3e6437991eb28502a337ee30b980eea3a923b74b47e5dbd402e1ba602e621d16`)
- Portable ZIP 1.0.2: <https://www.virustotal.com/gui/file/83afe5704aaff4d348ef3d917e27f878f6f72c961ccfb9fbc7fd0eae7c69393e>
  (SHA-256 `83afe5704aaff4d348ef3d917e27f878f6f72c961ccfb9fbc7fd0eae7c69393e`)
- Installer 1.0.2: <https://www.virustotal.com/gui/file/9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457>
  (SHA-256 `9137dba13fcaba4a91e2b343a00c67d275f34a3196c0079065e76f162a351457`)
- Previous round (1.0.1): application 0/70
  (`b0a4825ca9c8c4b0c3130c7950e969e36a4b5e17a27a813300f5c361584becff`),
  installer 1/69 (`aea522aa4a0bdf650de3894ccf8050419fa46cbad5f1861b26283a2aabb5aa99`).

`Malicious.moderate.ml.score` is not a malware family — it is the name of
a machine-learning confidence bucket. None of the other ~68 engines
(including Microsoft Defender, Kaspersky, BitDefender, ESET, Sophos,
CrowdStrike and Elastic) agree with it, in either round. Trapmine's own
engine reports the application executable as Undetected. This is the
textbook profile of a false positive on an unsigned installer stub.

Why unsigned? Code-signing certificates cost money this free project does
not have. Faraday compensates structurally: no network code at all, no
elevation ever (`asInvoker`), read-only hardware access, no registry
writes by the app, no packing/obfuscation — all documented and verifiable
in [AV_HARDENING.md](AV_HARDENING.md).

## What you can do

1. **Use the portable ZIP** — the primary recommended download. It
   contains no installer stub (the only component any engine has ever
   flagged), needs no install step and no admin rights, and scans
   0-detection as a whole and file-by-file. See [PORTABLE.md](PORTABLE.md).
2. **Verify your download.** Compare your file's SHA-256
   (PowerShell: `Get-FileHash <file>`) against the hashes above. A
   mismatch means a corrupted or tampered download — delete it. A match
   means you have the exact bytes analyzed above.
3. **Whitelist locally** (optional): exclude the Faraday folder in your AV
   product. Never disable your AV globally for this or anything else.

## For AV vendors / reporting the false positive

A complete, ready-to-send report for the one flagging vendor is maintained
at [FP_SUBMISSIONS/Trapmine.md](FP_SUBMISSIONS/Trapmine.md) — hashes,
permalinks, corroborating 0/N results, and a technical description of the
installer (stock NSIS 3.12, full VersionInfo, per-user, zero
`Exec`/`ExecShell`; script public at `installer/faraday.nsi`). Most
vendors clear clean unsigned utilities within days of an FP report; each
new release may need re-submission until the product accrues reputation.

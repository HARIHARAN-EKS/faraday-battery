# Faraday packaging: builds the shipped folder tree (P2 layout).
#
#   dist\Faraday\
#     Faraday.exe        <- the ONE thing the user clicks (launcher stub)
#     README.txt
#     app\
#       faraday-core.exe (the Qt application)
#       runtime.manifest (every runtime file, app-relative; the launcher
#                         refuses to start if any listed file is missing)
#       Qt6*.dll, platforms\, qml\, sqldrivers\, ...
#
# Portable ZIP additionally carries app\portable.txt, which switches user
# data to the TOP-LEVEL Faraday\data\ folder (never inside app\).
param([switch]$Zip, [switch]$Installer)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$qt = "C:\Qt\6.8.2\mingw_64"
$dist = Join-Path $root "dist\Faraday"
$app = Join-Path $dist "app"

if (Test-Path $dist) { Remove-Item -Recurse -Force $dist }
New-Item -ItemType Directory -Force $app | Out-Null

# 1. Launcher at the top level; the Qt application inside app\.
Copy-Item (Join-Path $root "build\Faraday.exe") $dist
Copy-Item (Join-Path $root "build\faraday-core.exe") $app

# 2. Deploy the Qt runtime NEXT TO faraday-core.exe (inside app\) so the
#    Windows loader resolves every DLL from the exe's own directory.
& "$qt\bin\windeployqt.exe" --release --compiler-runtime --no-translations `
    --skip-plugin-types qmltooling,networkinformation,tls,generic `
    --qmldir (Join-Path $root "ui") (Join-Path $app "faraday-core.exe") | Out-Null

foreach ($drv in @("qsqlmimer.dll", "qsqlodbc.dll", "qsqlpsql.dll")) {
    $p = Join-Path $app "sqldrivers\$drv"
    if (Test-Path $p) { Remove-Item $p -Force }
}

# 3. runtime.manifest — every file under app\, app-relative, excluding the
#    manifest itself and the portable marker.
$appPath = (Resolve-Path $app).Path
Get-ChildItem $app -Recurse -File |
    Where-Object { $_.Name -notin @("runtime.manifest", "portable.txt") } |
    ForEach-Object { $_.FullName.Substring($appPath.Length + 1) } |
    Sort-Object |
    Set-Content (Join-Path $app "runtime.manifest") -Encoding ASCII

# 4. Top-level README.txt — short and plain.
Set-Content (Join-Path $dist "README.txt") @"
Faraday - Battery Intelligence Suite

Run Faraday.exe

That's it. Everything else lives in the 'app' folder and is not meant to
be opened directly. Keep this folder together: if you copy Faraday.exe out
on its own it cannot work, and it will tell you so.

Your settings and battery history are saved in the 'data' folder that
appears next to Faraday.exe, so the whole folder is self-contained and
safe to run from a USB stick.

To uninstall: delete this folder.
"@ -Encoding ASCII

$topLevel = (Get-ChildItem $dist | Measure-Object).Count
Write-Host "dist tree: $topLevel top-level items ($((Get-ChildItem $dist | ForEach-Object Name) -join ', '))"
Write-Host "app\ files: $((Get-ChildItem $app -Recurse -File).Count), manifest entries: $((Get-Content (Join-Path $app 'runtime.manifest')).Count)"

if ($Installer) {
    & 'C:\Program Files (x86)\NSIS\makensis.exe' (Join-Path $root "installer\faraday.nsi") | Select-Object -Last 1
}

if ($Zip) {
    $ver = (Get-Item (Join-Path $dist "Faraday.exe")).VersionInfo.FileVersion.Substring(0, 5)
    $stage = Join-Path $env:TEMP "faraday-zipstage"
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item -Recurse $dist (Join-Path $stage "Faraday")
    # Portable marker lives inside app\ (invisible to the user); it switches
    # data storage to the top-level Faraday\data\ folder.
    Set-Content (Join-Path $stage "Faraday\app\portable.txt") `
        "Faraday portable mode: settings and history are stored in the 'data' folder next to Faraday.exe." -Encoding ASCII
    $zipPath = Join-Path $root "dist\Faraday-$ver-portable-win64.zip"
    if (Test-Path $zipPath) { Remove-Item $zipPath -Force }
    Compress-Archive -Path (Join-Path $stage "Faraday") -DestinationPath $zipPath
    Write-Host "zip: $(Split-Path $zipPath -Leaf) ($([math]::Round((Get-Item $zipPath).Length/1MB,1)) MB)"
}

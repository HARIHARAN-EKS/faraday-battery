; Faraday - Battery Intelligence Suite - NSIS installer
;
; Built with a stock, unmodified NSIS 3.12 release (standard stubs,
; standard /SOLID lzma compression - the documented NSIS default choice
; for release installers, nothing exotic).
;
; Heuristic-surface policy (see docs/AV_HARDENING.md):
;   - Per-user install (RequestExecutionLevel user) - never elevates,
;     matching the application's asInvoker profile.
;   - Modern UI 2 with the standard page flow; no silent-by-default.
;   - FULL VersionInfo resource including OriginalFilename/InternalName
;     (metadata-less installers score worse with generic heuristics).
;   - No Exec / ExecShell / ExecWait of anything, anywhere - the installer
;     copies files, writes shortcuts and the standard HKCU uninstall
;     entry, and nothing else. No run-after-install checkbox either.
;   - No bundled third-party components: the payload is exactly the
;     windeployqt output of our own build.
;   - The only registry activity is the per-user uninstall entry (HKCU)
;     that Windows requires to list the uninstaller; the application
;     itself never touches the registry.
;   - Unsigned: no code-signing certificate is available. Nothing here
;     fakes or imitates a signature.

!define PRODUCT_NAME "Faraday"
!define PRODUCT_FULL_NAME "Faraday - Battery Intelligence Suite"
!define PRODUCT_VERSION "1.0.4"
!define PRODUCT_PUBLISHER "Faraday Project"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Faraday"

Unicode true
ManifestDPIAware true
ManifestSupportedOS all

Name "${PRODUCT_FULL_NAME}"
OutFile "..\dist\Faraday-${PRODUCT_VERSION}-setup-win64.exe"
InstallDir "$LOCALAPPDATA\Programs\Faraday"
RequestExecutionLevel user
SetCompressor /SOLID lzma
BrandingText "${PRODUCT_FULL_NAME} ${PRODUCT_VERSION}"

; ---- Full version-info resource -------------------------------------
VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "ProductName" "${PRODUCT_FULL_NAME}"
VIAddVersionKey "FileDescription" "${PRODUCT_FULL_NAME} Setup"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 ${PRODUCT_PUBLISHER}. MIT License."
VIAddVersionKey "OriginalFilename" "Faraday-${PRODUCT_VERSION}-setup-win64.exe"
VIAddVersionKey "InternalName" "faraday-setup"

; ---- Modern UI 2 ------------------------------------------------------
!include "MUI2.nsh"
!include "FileFunc.nsh"

!define MUI_ICON "..\resources\faraday.ico"
!define MUI_UNICON "..\resources\faraday.ico"
!define MUI_ABORTWARNING

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; Post-copy payload verification (D2): the install must never complete
; with a partial runtime - that is exactly the state that produced the
; field loader failure.
!macro VerifyInstalled FILE
  IfFileExists "$INSTDIR\${FILE}" +3 0
    MessageBox MB_OK|MB_ICONSTOP "Installation is incomplete: ${FILE} is missing.$\r$\nPlease re-run the installer."
    Abort
!macroend

; ---- Install ----------------------------------------------------------
Section "Faraday (required)"
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "..\dist\Faraday\*.*"

  ; Verify the critical payload actually landed.
  !insertmacro VerifyInstalled "faraday.exe"
  !insertmacro VerifyInstalled "faraday-app.exe"
  !insertmacro VerifyInstalled "runtime.manifest"
  !insertmacro VerifyInstalled "Qt6Core.dll"
  !insertmacro VerifyInstalled "Qt6Gui.dll"
  !insertmacro VerifyInstalled "Qt6Qml.dll"
  !insertmacro VerifyInstalled "Qt6Quick.dll"
  !insertmacro VerifyInstalled "platforms\qwindows.dll"
  !insertmacro VerifyInstalled "sqldrivers\qsqlite.dll"

  ; Start-menu shortcuts. SetOutPath above also fixes each shortcut's
  ; "Start in" directory to $INSTDIR, so the launcher always runs with the
  ; correct working directory beside its runtime.
  CreateDirectory "$SMPROGRAMS\Faraday"
  CreateShortCut "$SMPROGRAMS\Faraday\Faraday.lnk" "$INSTDIR\faraday.exe"
  CreateShortCut "$SMPROGRAMS\Faraday\Uninstall Faraday.lnk" "$INSTDIR\uninstall.exe"

  ; Uninstaller + per-user Apps entry (HKCU only; the app itself never
  ; writes the registry)
  WriteUninstaller "$INSTDIR\uninstall.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${PRODUCT_FULL_NAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${PRODUCT_VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "${PRODUCT_PUBLISHER}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\faraday.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" "$\"$INSTDIR\uninstall.exe$\""
  WriteRegStr HKCU "${UNINST_KEY}" "QuietUninstallString" "$\"$INSTDIR\uninstall.exe$\" /S"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1

  ; EstimatedSize (KB) so Apps & features shows a real size
  ${GetSize} "$INSTDIR" "/S=0K" $0 $1 $2
  IntFmt $0 "0x%08X" $0
  WriteRegDWORD HKCU "${UNINST_KEY}" "EstimatedSize" "$0"
SectionEnd

; ---- Uninstall --------------------------------------------------------
Section "Uninstall"
  ; Remove the optional autostart shortcut if the user enabled it
  Delete "$APPDATA\Microsoft\Windows\Start Menu\Programs\Startup\Faraday.lnk"

  Delete "$SMPROGRAMS\Faraday\Faraday.lnk"
  Delete "$SMPROGRAMS\Faraday\Uninstall Faraday.lnk"
  RMDir "$SMPROGRAMS\Faraday"

  RMDir /r "$INSTDIR"
  DeleteRegKey HKCU "${UNINST_KEY}"

  ; User data (settings + sample database) is intentionally preserved at
  ; %APPDATA%\Faraday Project\Faraday so a reinstall keeps history.
SectionEnd

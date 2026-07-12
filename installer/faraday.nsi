; Faraday - Battery Intelligence Suite - NSIS installer
;
; Per-user install (no elevation, matching the app's asInvoker profile).
; No bundled third-party components: the payload is exactly the windeployqt
; output of our own build. The only registry activity is the per-user
; uninstall entry (HKCU) that Windows requires to list the uninstaller;
; the application itself never touches the registry.

!define PRODUCT_NAME "Faraday"
!define PRODUCT_FULL_NAME "Faraday - Battery Intelligence Suite"
!define PRODUCT_VERSION "1.0.1"
!define PRODUCT_PUBLISHER "Faraday Project"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\Faraday"

Unicode true
Name "${PRODUCT_FULL_NAME}"
OutFile "..\dist\Faraday-${PRODUCT_VERSION}-setup-win64.exe"
InstallDir "$LOCALAPPDATA\Programs\Faraday"
RequestExecutionLevel user
SetCompressor /SOLID lzma

Icon "..\resources\faraday.ico"
UninstallIcon "..\resources\faraday.ico"

VIProductVersion "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductName" "${PRODUCT_FULL_NAME}"
VIAddVersionKey "CompanyName" "${PRODUCT_PUBLISHER}"
VIAddVersionKey "FileDescription" "${PRODUCT_FULL_NAME} Setup"
VIAddVersionKey "FileVersion" "${PRODUCT_VERSION}.0"
VIAddVersionKey "ProductVersion" "${PRODUCT_VERSION}"
VIAddVersionKey "LegalCopyright" "Copyright (C) 2026 ${PRODUCT_PUBLISHER}. MIT License."

Page directory
Page instfiles
UninstPage uninstConfirm
UninstPage instfiles

Section "Faraday (required)"
  SectionIn RO
  SetOutPath "$INSTDIR"
  File /r "..\dist\Faraday\*.*"

  ; Start-menu shortcut
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
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
SectionEnd

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

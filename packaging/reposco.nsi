Unicode true
!include "MUI2.nsh"
!ifndef STAGE_ROOT
  !error "Pass /DSTAGE_ROOT=<validated package staging directory>"
!endif
Name "Reposco"
!ifndef OUTPUT
  !define OUTPUT "Reposco-0.1.0-dev-setup.exe"
!endif
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\Reposco"
RequestExecutionLevel user
!define MUI_ABORTWARNING
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "Reposco"
  SetShellVarContext current
  SetOutPath "$INSTDIR"
  File /r "${STAGE_ROOT}\*"
  WriteUninstaller "$INSTDIR\Uninstall.exe"
  CreateShortcut "$SMPROGRAMS\Reposco.lnk" "$INSTDIR\reposco.exe"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Reposco" "DisplayName" "Reposco"
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Reposco" "UninstallString" '$"$INSTDIR\Uninstall.exe$"'
  WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Reposco" "DisplayVersion" "0.1.0-dev"
SectionEnd

Section "Uninstall"
  SetShellVarContext current
  StrCmp "$INSTDIR" "$LOCALAPPDATA\Programs\Reposco" +3
  MessageBox MB_ICONSTOP "Unexpected installation path. Remove only Reposco files manually."
  Abort
  Delete "$SMPROGRAMS\Reposco.lnk"
  DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\Reposco"
  RMDir /r "$INSTDIR"
SectionEnd

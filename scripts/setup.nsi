; ---------------------------------------------------------
; AeroFlip Installer Script
; ---------------------------------------------------------

!define APPNAME "AeroFlip"
!define APPVERSION "1.0"

Name "${APPNAME}"
OutFile "AeroFlipSetup64.exe"

InstallDir "$PROGRAMFILES64\${APPNAME}"

RequestExecutionLevel admin

Page directory
Page instfiles

; ---------------------------------------------------------
; Main Installation Section
; ---------------------------------------------------------
Section "Install"

    SetOutPath "$INSTDIR"
    
    File "..\x64\Deploy\AeroFlip.exe"
    File "install64.bat"
    File "uninstall.bat"
    
    ExecWait '"$INSTDIR\install64.bat"'
    
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\AeroFlip.exe"

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}" '"$INSTDIR\AeroFlip.exe"'

    Sleep 3000 ; (HACK) Wait for signing to absolutely finish

    ExecShell "open" "$INSTDIR\AeroFlip.exe" "" SW_SHOWNORMAL
SectionEnd

; ---------------------------------------------------------
; Uninstallation Section
; ---------------------------------------------------------
Section "Uninstall"
    ExecWait '"$INSTDIR\uninstall.bat"'

    Delete "$INSTDIR\AeroFlip.exe"
    Delete "$INSTDIR\install64.bat"
    Delete "$INSTDIR\uninstall.bat"
    Delete "$INSTDIR\uninstall.exe"
    
    Delete "$SMPROGRAMS\${APPNAME}.lnk"
        
    RMDir "$INSTDIR"

    DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}"

SectionEnd
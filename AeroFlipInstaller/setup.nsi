; ---------------------------------------------------------
; AeroFlip Installer Script
; ---------------------------------------------------------

!include "MUI2.nsh"

!define MUI_PAGE_LICENSE "../LICENSE" 

!insertmacro MUI_PAGE_LICENSE "${MUI_PAGE_LICENSE}"
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES

!define MUI_FINISHPAGE_RUN "$INSTDIR\AeroFlipConfigurator.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch Configurator"
!insertmacro MUI_PAGE_FINISH

; Uninstaller Pages
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

!include "x64.nsh"

!define APPNAME "AeroFlip"
!define APPVERSION "1.0"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${APPNAME}"

Name "${APPNAME}"
OutFile "AeroFlip_${ARCH}.exe"

Function .onInit
    ${If} "${ARCH}" == "x64"
        ${IfNot} ${RunningX64}
            MessageBox MB_OK|MB_ICONSTOP "This installer is 64-bit and can only be run on 64-bit Windows."
            Quit
        ${EndIf}
    ${ElseIf} "${ARCH}" == "x86"
        ${If} ${RunningX64}
            MessageBox MB_OK|MB_ICONSTOP "This installer is 32-bit and can only be run on 32-bit Windows."
            Quit
        ${EndIf}
    ${EndIf}
FunctionEnd

InstallDir "C:\Program Files\${APPNAME}"

RequestExecutionLevel admin

; ---------------------------------------------------------
; Main Installation Section
; ---------------------------------------------------------
Section "Install"
    SetOutPath "$INSTDIR"

    ExecWait '"$INSTDIR\killproc.bat"'
    ExecWait '"$INSTDIR\unsigncert.bat"'
    
    File "${EXEFILE}"
    File "${CFGEXEFILE}"
    File "killproc.bat"
    File "signcert.bat"
    File "unsigncert.bat"
    
    ExecWait '"$INSTDIR\signcert.bat"'
    
    WriteUninstaller "$INSTDIR\uninstall.exe"
    
    ; Shortcuts
    CreateShortcut "$SMPROGRAMS\${APPNAME}.lnk" "$INSTDIR\AeroFlip.exe"
    CreateShortcut "$SMPROGRAMS\${APPNAME} Configurator.lnk" "$INSTDIR\AeroFlipConfigurator.exe"

    WriteRegStr HKLM "${UNINST_KEY}" "DisplayName" "${APPNAME}"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayVersion" "${APPVERSION}"
    WriteRegStr HKLM "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\AeroFlip.exe,0"
    WriteRegStr HKLM "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
    WriteRegStr HKLM "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "${UNINST_KEY}" "Publisher" "ZilverBlade"
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoModify" 1
    WriteRegDWORD HKLM "${UNINST_KEY}" "NoRepair" 1

    ; Run on Windows startup
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}" '"$INSTDIR\AeroFlip.exe"'

    ExecShell "open" "$INSTDIR\AeroFlip.exe" "" SW_SHOWNORMAL


SectionEnd

; ---------------------------------------------------------
; Uninstallation Section
; ---------------------------------------------------------
Section "Uninstall"
    ExecWait '"$INSTDIR\killproc.bat"'
    ExecWait '"$INSTDIR\unsigncert.bat"'

    Delete "$INSTDIR\AeroFlip.exe"
    Delete "$INSTDIR\AeroFlipConfigurator.exe"
    Delete "$INSTDIR\killproc.bat"
    Delete "$INSTDIR\signcert.bat"
    Delete "$INSTDIR\unsigncert.bat"
    Delete "$INSTDIR\uninstall.exe"
    
    Delete "$SMPROGRAMS\${APPNAME}.lnk"
    Delete "$SMPROGRAMS\${APPNAME} Configurator.lnk"
        
    RMDir "$INSTDIR"

    ; Remove from Windows Startup list
    DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}"
    
    ; Clean up Add or Remove Programs entry
    DeleteRegKey HKLM "${UNINST_KEY}"
SectionEnd
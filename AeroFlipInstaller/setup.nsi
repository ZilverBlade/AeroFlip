; ---------------------------------------------------------
; AeroFlip Installer Script
; ---------------------------------------------------------

!include "x64.nsh"

!define APPNAME "AeroFlip"
!define APPVERSION "1.0"

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

Page directory
Page instfiles

; ---------------------------------------------------------
; Main Installation Section
; ---------------------------------------------------------
Section "Install"

    SetOutPath "$INSTDIR"
    
	; Kill previous process if its running
    ExecWait '"$INSTDIR\killproc.bat"'
	
    File "${EXEFILE}"
    File "killproc.bat"
    File "signcert.bat"
    File "unsigncert.bat"
    
    ExecWait '"$INSTDIR\signcert.bat"'
    
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
    ExecWait '"$INSTDIR\killproc.bat"'
    ExecWait '"$INSTDIR\unsigncert.bat"'

    Delete "$INSTDIR\AeroFlip.exe"
    Delete "$INSTDIR\killproc.bat"
    Delete "$INSTDIR\signcert.bat"
    Delete "$INSTDIR\unsigncert.bat"
    Delete "$INSTDIR\uninstall.exe"
    
    Delete "$SMPROGRAMS\${APPNAME}.lnk"
        
    RMDir "$INSTDIR"

    DeleteRegValue HKLM "Software\Microsoft\Windows\CurrentVersion\Run" "${APPNAME}"

SectionEnd
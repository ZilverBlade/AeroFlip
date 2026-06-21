@echo off
setlocal

echo [AeroFlip] Starting Local Certificate Elevation...

set "TARGET_EXE=C:\Program Files\AeroFlip\AeroFlip.exe"

:: Check if the executable actually exists before trying to sign it
if not exist "%TARGET_EXE%" (
    echo [ERROR] Could not find AeroFlip.exe at %TARGET_EXE%
    echo Did the installer copy the files correctly?
    pause
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'Stop';" ^
  "$exePath = '%TARGET_EXE%';" ^
  "Write-Host '[AeroFlip] 1. Generating secure local Code Signing certificate...';" ^
  "$cert = New-SelfSignedCertificate -Subject 'CN=AeroFlip Local Elevation' -Type CodeSigningCert -CertStoreLocation 'Cert:\LocalMachine\My' -HashAlgorithm SHA256;" ^
  "Write-Host '[AeroFlip] 2. Adding public key to Trusted Root Certification Authorities...';" ^
  "$store = [System.Security.Cryptography.X509Certificates.X509Store]::new('Root', 'LocalMachine');" ^
  "$store.Open('ReadWrite');" ^
  "$store.Add($cert);" ^
  "$store.Close();" ^
  "Write-Host '[AeroFlip] 3. Signing AeroFlip.exe with native Authenticode...';" ^
  "Set-AuthenticodeSignature -FilePath $exePath -Certificate $cert -HashAlgorithm SHA256;" ^
  "Write-Host '[AeroFlip] 4. Destroying local private key for security...';" ^
  "Remove-Item -Path ('Cert:\LocalMachine\My\' + $cert.Thumbprint);" ^
  "Write-Host '[AeroFlip] Setup Complete! UI Access granted.';"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] Something went wrong during the PowerShell signing phase.
    pause
    exit /b 1
)

echo [AeroFlip] Successfully elevated. You can now launch the application!
exit /b 0
@echo off
setlocal

echo [AeroFlip] Terminating process...

taskkill /f /im AeroFlip.exe

echo [AeroFlip] Cleaning up system certificates...

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$ErrorActionPreference = 'SilentlyContinue';" ^
  "Write-Host '[AeroFlip] Searching for local deployment certificate...';" ^
  "$certs = Get-ChildItem -Path Cert:\LocalMachine\Root | Where-Object { $_.Subject -eq 'CN=AeroFlip Local Elevation' };" ^
  "if ($certs) {" ^
  "    foreach ($cert in $certs) {" ^
  "        Write-Host ('[AeroFlip] Removing matching certificate: ' + $cert.Thumbprint);" ^
  "        Remove-Item -Path ('Cert:\LocalMachine\Root\' + $cert.Thumbprint) -Force;" ^
  "    }" ^
  "    Write-Host '[AeroFlip] Certificate store successfully purged.';" ^
  "} else {" ^
  "    Write-Host '[AeroFlip] No lingering certificates found. System is clean.';" ^
  "}"

if %ERRORLEVEL% NEQ 0 (
    echo [ERROR] An issue occurred while purging the certificate store.
    exit /b 1
)

echo [AeroFlip] Clean up complete!
exit /b 0
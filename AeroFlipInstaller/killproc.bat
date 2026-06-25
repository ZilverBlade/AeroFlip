@echo off
setlocal

echo [AeroFlip] Terminating process...

taskkill /f /im AeroFlip.exe
taskkill /f /im AeroFlipConfigurator.exe

exit /b 0
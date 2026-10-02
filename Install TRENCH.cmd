@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_vst3.ps1"
set "INSTALL_RESULT=%ERRORLEVEL%"
pause
exit /b %INSTALL_RESULT%

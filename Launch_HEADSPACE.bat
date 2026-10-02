@echo off
setlocal
set "APP=%~dp0out\build\vst3\plugin\workstation\HEADSPACE.exe"
if not exist "%APP%" set "APP=%~dp0out\build\vst3\plugin\workstation\TRENCH_Headspace_App.exe"
if not exist "%APP%" (
  echo HEADSPACE.exe is not built. Run native\workstation\build_headspace.cmd first.
  pause
  exit /b 1
)
start "" "%APP%"
endlocal

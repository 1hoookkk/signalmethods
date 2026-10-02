@echo off
setlocal
set "ROOT=%~dp0"
set "PLATES=%ROOT%tmp\plates"
if "%~1"=="" (
  echo usage: "%~nx0" ^<plate 01-11^>
  echo.
  echo  01  ChatGPT Sep 15 03_04_43
  echo  02  ChatGPT Sep 15 03_04_32
  echo  03  Celadon UNIX Workstation Faceplate
  echo  04  Iridescent Graphite Interface Panel
  echo  05  E6E27779 jpeg
  echo  06  ChatGPT Sep 11 12_39_22
  echo  07  df2_panel_beige 20260910-083338
  echo  08  ChatGPT Sep 10 07_20_08
  echo  09  ChatGPT Sep 10 08_02_50
  echo  10  ChatGPT Sep 10 06_40_26
  echo  11  ChatGPT Sep 15 03_04_51
  echo.
  echo unset        clear override, plugin uses its built-in plate
  echo.
  echo Set the plate, then load TRENCH on a track in your set.
  echo Reload the plugin ^(or toggle the editor^) to pick up the change.
  echo.
  echo Plugin: %ROOT%out\build\vst3\plugin\TRENCH_artefacts\Release\VST3\TRENCH.vst3
  endlocal
  exit /b 0
)
if /i "%~1"=="unset" (
  reg delete "HKCU\Environment" /F /V TRENCH_PLATE_FILE >nul 2>&1
  echo cleared TRENCH_PLATE_FILE
  endlocal
  exit /b 0
)
set "NUM=%~1"
for /f "tokens=* delims=0" %%N in ("%NUM%") do set "NUM=%%N"
if not defined NUM set "NUM=0"
if %NUM% LSS 10 set "NUM=0%NUM%"
set "FILE="
for %%F in ("%PLATES%\p%NUM%*") do set "FILE=%%~fF"
if not defined FILE (
  echo no plate matches p%NUM% in %PLATES%
  endlocal
  exit /b 1
)
setx TRENCH_PLATE_FILE "%FILE%" >nul
echo TRENCH_PLATE_FILE = %FILE%
echo Reload TRENCH in your DAW to see it.
endlocal

@echo off
rem Double-click to choose WAV files, or drag 2 or 4 WAV files onto this icon.
rem   2 files: first = M0 (wheel down), second = M100 (wheel up)
rem   4 files: M0Q0  M100Q0  M0Q100  M100Q100
setlocal EnableDelayedExpansion
set "TRENCH_ROOT=C:\Users\hooki\trench-workstation"
cd /d "%TRENCH_ROOT%"
if "%~1"=="" (
    powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%TRENCH_ROOT%\tools\make_body_picker.ps1"
    set "RESULT=!ERRORLEVEL!"
    pause
    exit /b !RESULT!
)
set /p NAME=Name your body (e.g. MY_BASS):
if "%NAME%"=="" set NAME=UNTITLED_BODY
python tools\make_body.py "%NAME%" %*
if errorlevel 1 (
    echo.
    echo Something refused - read the message above.
) else (
    echo.
    echo DONE. Body: bodies\candidates\%NAME%.body240
    echo Listen:    evidence\body_%NAME%_e2e\
    start "" "evidence\body_%NAME%_e2e"
)
pause

@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "RTN=%ROOT%\out\build\vst3\_deps\rtneuralsource-src"
set "BIN=%ROOT%\output\rtneural-workshop\bin"
if not exist "%BIN%" mkdir "%BIN%"
cd /d "%BIN%"
cl /nologo /std:c++17 /O2 /EHsc /DRTNEURAL_USE_STL=1 /DRTNEURAL_NO_DEBUG=1 /DRTNEURAL_DEFAULT_ALIGNMENT=16 /I"%RTN%" /I"%RTN%\modules\json" "%~dp0runner.cpp" /Fe:runner.exe
exit /b %errorlevel%

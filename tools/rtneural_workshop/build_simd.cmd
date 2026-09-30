@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
for %%I in ("%~dp0..\..") do set "ROOT=%%~fI"
set "RTN=%ROOT%\out\build\vst3\_deps\rtneuralsource-src"
set "BIN=%ROOT%\output\rtneural-workshop\bin"
if not exist "%BIN%" mkdir "%BIN%"
cd /d "%BIN%"
cl /nologo /std:c++17 /O2 /EHsc /arch:AVX2 /DRTNEURAL_USE_XSIMD=1 /DRTNEURAL_NO_DEBUG=1 /DRTNEURAL_DEFAULT_ALIGNMENT=32 /I"%RTN%" /I"%RTN%\modules\json" /I"%RTN%\modules\xsimd\include" "%~dp0runner.cpp" /Fe:runner_simd.exe /Fo:runner_simd.obj
exit /b %errorlevel%

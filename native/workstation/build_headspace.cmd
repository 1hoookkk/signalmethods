@echo off
setlocal
cd /D "%~dp0../.."
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cmake --preset vst3
if errorlevel 1 exit /b 1
cmake --build --preset headspace
if errorlevel 1 exit /b 1
ctest --preset headspace
exit /b %errorlevel%

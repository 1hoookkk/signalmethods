@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0..\.."
cmake --preset vst3
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset headspace -- -j4
if errorlevel 1 exit /b %errorlevel%
ctest --preset headspace
exit /b %errorlevel%

@echo off
setlocal
if not "%~1"=="" if /i not "%~1"=="--configure" goto usage
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
pushd "%~dp0..\.."
if errorlevel 1 exit /b %errorlevel%
if /i "%~1"=="--configure" goto configure
if not exist "out\build\vst3\CMakeCache.txt" goto configure
if exist "out\build\vst3\build.ninja" goto build
:configure
cmake --preset vst3
if errorlevel 1 goto failed
:build
cmake --build --preset headspace
if errorlevel 1 goto failed
ctest --preset headspace
if errorlevel 1 goto failed
popd
exit /b 0
:failed
set "headspace_result=%errorlevel%"
popd
exit /b %headspace_result%
:usage
echo Usage: build_headspace.cmd [--configure]
exit /b 2

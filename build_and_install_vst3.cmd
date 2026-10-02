@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo Failed to initialize MSVC environment.
    exit /b 1
)

pushd "%~dp0"
if errorlevel 1 exit /b 1

if not exist "out\build\vst3\build.ninja" (
    echo Configuring vst3 preset...
    cmake --preset vst3
    if errorlevel 1 (
        echo CMake configuration failed.
        popd
        exit /b 1
    )
)

echo Building TRENCH_VST3 target...
cmake --build out\build\vst3 --target TRENCH_VST3
if errorlevel 1 (
    echo Build of TRENCH_VST3 failed.
    popd
    exit /b 1
)

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0install_vst3.ps1"
set "INSTALL_RESULT=%ERRORLEVEL%"
popd
exit /b %INSTALL_RESULT%

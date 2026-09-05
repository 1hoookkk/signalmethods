@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0..\.."
cmake --preset vst3
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset vst3 --target trench_bridge trench_audio TRENCH_WorkstationTests trench_core_tests trench_core_from_audio_tests -- -j2
exit /b %errorlevel%

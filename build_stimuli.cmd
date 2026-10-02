@echo off
setlocal
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cmake --build out\build\vst3 --target TRENCH_Stimuli
if errorlevel 1 exit /b 1
echo TRENCH_Stimuli built successfully.

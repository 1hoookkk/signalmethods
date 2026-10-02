@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cmake --build C:\Users\hooki\trench-native\out\build\vst3 --target TRENCH_VST3 TRENCH_Tests TRENCH_ReviewTests -j 2

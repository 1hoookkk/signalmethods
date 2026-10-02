@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b 1
cmake --build out\build\vst3 --parallel 6 --target TRENCH_VST3 TRENCH_Tests TRENCH_CalibrationTests TRENCH_SwitchSafetyTests TRENCH_Render
exit /b %errorlevel%

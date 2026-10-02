@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d C:\Users\hooki\trench-native
cmake --build out\build\vst3 --target TRENCH_Render -j 2
exit /b %errorlevel%

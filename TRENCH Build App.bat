@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -no_logo
cd /D C:\Users\hooki\trench-native
cmake --preset app && cmake --build --preset app
echo.
echo Done. App: out\build\app\native\app\trench_native.exe
pause

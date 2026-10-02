@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cl /nologo /O2 /std:c++20 /EHsc /I"C:\Users\hooki\trench-native\plugin\source" probe.cpp /Fe:probe.exe
if errorlevel 1 exit /b 1
probe.exe

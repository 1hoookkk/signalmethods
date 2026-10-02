@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "%~dp0"
cl /nologo /O2 /EHsc /std:c++17 /W3 /I "C:\Users\hooki\trench-native\plugin\source\dsp" desk_ab.cpp /Fe:desk_ab.exe
exit /b %errorlevel%

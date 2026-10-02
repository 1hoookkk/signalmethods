@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
cd /d "%~dp0"
set R=C:\Users\hooki\trench-native\out\build\vst3\_deps\rtneuralsource-src
cl /nologo /std:c++17 /O2 /EHsc /DRTNEURAL_USE_STL=1 /DRTNEURAL_NO_DEBUG=1 /DRTNEURAL_DEFAULT_ALIGNMENT=16 /I%R% /I%R%\modules\json rtn_run.cpp /Fe:rtn_run.exe
exit /b %errorlevel%

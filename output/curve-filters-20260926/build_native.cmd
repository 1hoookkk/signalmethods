@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%
cd /d "%~dp0..\.."
cl.exe /nologo /std:c++20 /O2 /EHsc /I native/core/include output/curve-filters-20260926/quad_native.cpp native/core/src/packed_body.cpp /Fe:output/curve-filters-20260926/quad_native.exe /Fo:output/curve-filters-20260926/

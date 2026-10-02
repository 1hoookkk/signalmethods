@echo off
setlocal
cd /D "%~dp0.."

echo Setting up MSVC x64 compiler...
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 (
    echo Error: Visual Studio 2022 Build Tools not found.
    pause
    exit /b 1
)

set INC=/I "vendor\imgui" /I "vendor\imgui\examples\libs\glfw\include" /I "out\build\vst3\vcpkg_installed\x64-windows-release\include" /I "tools\sgi_studio" /I "tools\implot" /I "tools\implot3d" /I "plugin\source" /I "tools"
set LIB_PATHS=/LIBPATH:"out\build\vst3\vcpkg_installed\x64-windows-release\lib"
set LIBS=imgui.lib glfw3dll.lib d3d11.lib dxgi.lib d3dcompiler.lib user32.lib gdi32.lib shell32.lib ole32.lib comdlg32.lib

echo Compiling SGI Spectrogram and SPAN Studio (Native Dear ImGui + ImPlot + ImPlot3D + DirectX 11)...
cl /O2 /EHsc /MD /std:c++20 %INC% tools\sgi_studio\main.cpp tools\sgi_studio\imgui_impl_glfw.cpp tools\sgi_studio\imgui_impl_dx11.cpp tools\implot\implot.cpp tools\implot\implot_items.cpp tools\implot3d\implot3d.cpp tools\implot3d\implot3d_items.cpp plugin\source\dsp\Peevers.cpp /Fe:tools\SGI_Studio.exe /link %LIB_PATHS% %LIBS%

if errorlevel 1 (
    echo Build failed!
    pause
    exit /b 1
)

echo Build succeeded: tools\SGI_Studio.exe
endlocal

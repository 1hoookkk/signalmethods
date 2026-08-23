@echo off
set RT=%~dp0out\build\windows-msvc-release\vcpkg_installed\x64-windows-release
set PATH=%RT%\bin;%PATH%
set QT_PLUGIN_PATH=%RT%\Qt6\plugins
start "" "%~dp0out\build\windows-msvc-release\native\app\trench_native.exe" %*

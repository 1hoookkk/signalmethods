@echo off
setlocal
cd /D "%~dp0"
echo Starting SGI Spectrogram and SPAN Studio...
python tools\sgi_spectrogram_gui.py
endlocal

@echo off
start "" "C:\Program Files\MATLAB\R2025b\bin\matlab.exe" -nosplash -sd "%~dp0" -r "addpath('toolbox'); trench.setup; app=trench.launch;"

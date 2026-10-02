@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
cmake --build C:\Users\hooki\trench-native\out\build\vst3 --target TRENCH_Dev_VST3 TRENCH_CalibrationTests TRENCH_Tests --parallel 2

@echo off
if exist "C:\Users\hooki\trench-native\out\build\app\native\app\trench_native.exe" (
  start "" "C:\Users\hooki\trench-native\out\build\app\native\app\trench_native.exe"
) else (
  start "" "C:\Users\hooki\build\trench-native-cleanup-baseline\native\app\trench_native.exe"
)

@echo off
cd /d "%~dp0build\Release"

if not exist "LatencyTestTool.exe" (
    echo ERROR: Application not built yet!
    echo Please run build.bat first.
    pause
    exit /b 1
)

start "" "LatencyTestTool.exe"

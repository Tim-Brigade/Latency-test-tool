@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  Video Latency Test Tool - Packager
echo ========================================
echo.

:: Set version (update this for releases)
set VERSION=1.0.0

:: Create distribution folder name
set DIST_NAME=LatencyTestTool-v%VERSION%-win64
set DIST_DIR=dist\%DIST_NAME%

:: Clean and create dist directory
if exist "dist" rmdir /s /q "dist"
mkdir "%DIST_DIR%"

:: Check if build exists
if not exist "build\Release\LatencyTestTool.exe" (
    echo ERROR: build\Release\LatencyTestTool.exe not found!
    echo Please run build.bat first.
    pause
    exit /b 1
)

echo Copying files...

:: Copy executable
copy "build\Release\LatencyTestTool.exe" "%DIST_DIR%\" >nul

:: Copy all DLLs
copy "build\Release\*.dll" "%DIST_DIR%\" >nul

:: Copy resources if they exist
if exist "build\Release\resources" (
    xcopy "build\Release\resources" "%DIST_DIR%\resources\" /E /I /Q >nul
)

:: Create a simple README for users
(
echo Video Latency Test Tool v%VERSION%
echo ================================
echo.
echo USAGE:
echo   1. Run LatencyTestTool.exe
echo   2. Press U to edit the RTSP URL
echo   3. Press C to connect to the stream
echo   4. Press T to start the timestamp clock
echo   5. Point your camera at the timestamp display
echo   6. Press SPACE to freeze and measure latency
echo.
echo KEYBOARD SHORTCUTS:
echo   U - Edit URL
echo   C - Connect to stream
echo   D - Disconnect
echo   T - Start/Stop clock
echo   SPACE - Freeze frame ^(measure latency^)
echo   S - Save screenshot
echo   ESC - Exit
echo.
echo REQUIREMENTS:
echo   - Windows 10/11 64-bit
echo   - Visual C++ Redistributable 2022
echo     ^(Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe^)
echo.
echo For issues, visit: https://github.com/your-repo/latency-test-tool
) > "%DIST_DIR%\README.txt"

:: Create zip file using PowerShell
echo Creating zip archive...
powershell -Command "Compress-Archive -Path 'dist\%DIST_NAME%' -DestinationPath 'dist\%DIST_NAME%.zip' -Force"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo ========================================
    echo  Package created successfully!
    echo ========================================
    echo.
    echo Output: dist\%DIST_NAME%.zip
    echo.
    echo Share this zip file with others. They just need to:
    echo   1. Extract the zip
    echo   2. Install VC++ Redistributable if needed
    echo   3. Run LatencyTestTool.exe
    echo.
) else (
    echo.
    echo ERROR: Failed to create zip file
)

pause

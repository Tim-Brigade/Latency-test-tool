@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  Video Latency Test Tool - Packager
echo ========================================
echo.

:: Set version (update this for releases)
set VERSION=1.1.0

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

:: Note: connection_history.txt and latency_*.bmp are NOT copied (user data)

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
echo   2. Press U to edit the RTSP URL, or press 1-9 to use a recent connection
echo   3. Press C to connect ^(the clock starts automatically^)
echo   4. Point your camera at the white timestamp display panel
echo   5. Press SPACE to freeze the frame and measure latency
echo   6. Compare the frozen time in the video to the clock panel
echo.
echo KEYBOARD SHORTCUTS:
echo   U     - Edit URL
echo   C     - Connect to stream
echo   D     - Disconnect
echo   P     - Cycle transport protocol ^(Auto/TCP/UDP^)
echo   SPACE - Freeze frame ^(measure latency^)
echo   S     - Save screenshot
echo   1-9   - Quick connect to recent URLs
echo   F1    - Show help
echo   F2    - Show about
echo   ESC   - Close panel / Unpause / Exit
echo.
echo REQUIREMENTS:
echo   - Windows 10/11 64-bit
echo   - Visual C++ Redistributable 2022
echo     ^(Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe^)
echo.
echo Author: tim.biddulph@brigade-electronics.com
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

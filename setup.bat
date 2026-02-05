@echo off
setlocal

echo ==========================================
echo  Video Latency Test Tool - Quick Setup
echo ==========================================
echo.
echo This script will:
echo   1. Install vcpkg (if needed)
echo   2. Install dependencies
echo   3. Build the application
echo   4. Create a desktop shortcut
echo.
pause

:: Check for vcpkg, install if not found
if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=C:\vcpkg
        goto :vcpkg_found
    )
    if exist "%USERPROFILE%\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=%USERPROFILE%\vcpkg
        goto :vcpkg_found
    )

    echo vcpkg not found. Installing to C:\vcpkg...
    echo.

    :: Check for git
    where git >nul 2>&1
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Git is required but not found in PATH
        echo Please install Git from https://git-scm.com/
        pause
        exit /b 1
    )

    :: Clone and bootstrap vcpkg
    git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
    if %ERRORLEVEL% NEQ 0 (
        echo ERROR: Failed to clone vcpkg
        pause
        exit /b 1
    )

    pushd C:\vcpkg
    call bootstrap-vcpkg.bat
    popd

    set VCPKG_ROOT=C:\vcpkg

    :: Set environment variable permanently
    setx VCPKG_ROOT "C:\vcpkg"
    echo.
    echo vcpkg installed and VCPKG_ROOT set to C:\vcpkg
)

:vcpkg_found
echo.
echo Using vcpkg at: %VCPKG_ROOT%
echo.

:: Run the build script
call "%~dp0build.bat"

if %ERRORLEVEL% NEQ 0 (
    echo Setup failed!
    pause
    exit /b 1
)

:: Create shortcut automatically
echo.
echo Creating desktop shortcut...
powershell -ExecutionPolicy Bypass -File "%~dp0create-shortcut.ps1"

echo.
echo ==========================================
echo  Setup Complete!
echo ==========================================
echo.
echo You can now:
echo   - Double-click the desktop shortcut
echo   - Or run: run.bat
echo.
pause

@echo off
setlocal enabledelayedexpansion

echo ========================================
echo  Video Latency Test Tool - Build Script
echo ========================================
echo.

:: Find CMake - check PATH first, then Visual Studio locations
where cmake >nul 2>&1
if %ERRORLEVEL% EQU 0 (
    set "CMAKE_CMD=cmake"
    goto :cmake_found
)

:: Search for VS CMake
for %%Y in (2022 2019) do (
    for %%E in (Enterprise Professional Community BuildTools) do (
        set "VS_CMAKE=C:\Program Files\Microsoft Visual Studio\%%Y\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "!VS_CMAKE!" (
            set "CMAKE_CMD=!VS_CMAKE!"
            goto :cmake_found
        )
        set "VS_CMAKE=C:\Program Files (x86)\Microsoft Visual Studio\%%Y\%%E\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
        if exist "!VS_CMAKE!" (
            set "CMAKE_CMD=!VS_CMAKE!"
            goto :cmake_found
        )
    )
)

echo ERROR: CMake not found!
echo.
echo Please install CMake using one of these methods:
echo   winget install Kitware.CMake --source winget
echo   or download from https://cmake.org/download/
echo.
pause
exit /b 1

:cmake_found
echo Found CMake: %CMAKE_CMD%
echo.

:: Check for vcpkg
if not defined VCPKG_ROOT (
    if exist "C:\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=C:\vcpkg
    ) else if exist "C:\src\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=C:\src\vcpkg
    ) else if exist "%USERPROFILE%\vcpkg\vcpkg.exe" (
        set VCPKG_ROOT=%USERPROFILE%\vcpkg
    ) else (
        echo ERROR: vcpkg not found!
        echo Please install vcpkg and set VCPKG_ROOT environment variable
        echo.
        echo Instructions:
        echo   git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
        echo   cd C:\vcpkg
        echo   bootstrap-vcpkg.bat
        echo   set VCPKG_ROOT=C:\vcpkg
        echo.
        pause
        exit /b 1
    )
)

echo Using vcpkg at: %VCPKG_ROOT%
echo.

:: Install dependencies
echo Installing dependencies (this may take a while on first run)...
echo.
"%VCPKG_ROOT%\vcpkg.exe" install --triplet x64-windows

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Failed to install dependencies
    pause
    exit /b 1
)

echo.
echo Dependencies installed successfully!
echo.

:: Create build directory
if not exist "build" mkdir build

:: Configure with CMake
echo Configuring project with CMake...
"%CMAKE_CMD%" -B build -S . -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake" -DVCPKG_TARGET_TRIPLET=x64-windows

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: CMake configuration failed
    pause
    exit /b 1
)

echo.
echo Building Release configuration...
"%CMAKE_CMD%" --build build --config Release

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo ERROR: Build failed
    pause
    exit /b 1
)

echo.
echo ========================================
echo  Build completed successfully!
echo ========================================
echo.
echo Executable: build\Release\LatencyTestTool.exe
echo.

:: Ask to create shortcut
set /p CREATE_SHORTCUT="Create desktop shortcut? (Y/N): "
if /i "%CREATE_SHORTCUT%"=="Y" (
    powershell -ExecutionPolicy Bypass -File "%~dp0create-shortcut.ps1"
)

echo.
echo Done! You can now run the application.
pause

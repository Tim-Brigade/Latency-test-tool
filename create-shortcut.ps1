# Create Desktop Shortcut for Video Latency Test Tool

$scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
$exePath = Join-Path $scriptPath "build\Release\LatencyTestTool.exe"
$workingDir = Join-Path $scriptPath "build\Release"
$desktopPath = [Environment]::GetFolderPath("Desktop")
$shortcutPath = Join-Path $desktopPath "Video Latency Test Tool.lnk"

# Check if executable exists
if (-not (Test-Path $exePath)) {
    Write-Host "ERROR: Executable not found at: $exePath" -ForegroundColor Red
    Write-Host "Please build the project first by running build.bat" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

# Create shortcut
$WshShell = New-Object -ComObject WScript.Shell
$shortcut = $WshShell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = $exePath
$shortcut.WorkingDirectory = $workingDir
$shortcut.Description = "Video Latency Test Tool - Measure camera streaming latency"
$shortcut.Save()

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Desktop shortcut created successfully!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Shortcut: $shortcutPath" -ForegroundColor White
Write-Host ""
Write-Host "You can now launch the app from your desktop!" -ForegroundColor Yellow

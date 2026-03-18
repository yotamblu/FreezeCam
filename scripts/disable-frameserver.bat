@echo off
echo ========================================
echo  FreezeCam Pro - Disable Frame Server
echo ========================================
echo.
echo This disables the Windows Camera Frame Server so that
echo DirectShow virtual cameras (like FreezeCam) work in
echo Zoom, Discord, Teams, etc.
echo.

reg add "HKLM\SOFTWARE\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /t REG_DWORD /d 0 /f
if errorlevel 1 (
    echo ERROR: Failed to set 64-bit registry key.
    echo Please right-click this file and "Run as administrator"
    pause
    exit /b 1
)

reg add "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /t REG_DWORD /d 0 /f
if errorlevel 1 (
    echo ERROR: Failed to set 32-bit registry key.
    echo Please right-click this file and "Run as administrator"
    pause
    exit /b 1
)

echo.
echo SUCCESS! Frame Server Mode disabled.
echo.
echo IMPORTANT: Please restart Zoom (close and reopen it)
echo for the change to take effect.
echo.
pause

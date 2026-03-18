@echo off
echo ========================================
echo  FreezeCam Pro - Re-enable Frame Server
echo ========================================
echo.
echo This re-enables the Windows Camera Frame Server
echo (restores the default Windows behavior).
echo.

reg delete "HKLM\SOFTWARE\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /f 2>nul
reg delete "HKLM\SOFTWARE\WOW6432Node\Microsoft\Windows Media Foundation\Platform" /v EnableFrameServerMode /f 2>nul

echo.
echo Frame Server Mode re-enabled (default).
echo Restart Zoom/Discord for the change to take effect.
echo.
pause

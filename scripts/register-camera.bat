@echo off
echo ============================================
echo   FreezeCam Pro - Register Virtual Camera
echo ============================================
echo.
echo This requires Administrator privileges.
echo.

net session >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [!] Requesting administrator privileges...
    powershell -Command "Start-Process '%~f0' -Verb RunAs"
    exit /b
)

set DLL_PATH=%~dp0..\native\virtual-camera\build2\FreezeCamVirtualCamera.dll

if not exist "%DLL_PATH%" (
    echo [ERROR] DLL not found at: %DLL_PATH%
    echo Please build the project first: scripts\build-all.bat
    pause
    exit /b 1
)

echo Registering: %DLL_PATH%
regsvr32 /s "%DLL_PATH%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Registration failed!
    echo Try running regsvr32 manually:
    echo   regsvr32 "%DLL_PATH%"
) else (
    echo [OK] FreezeCam Pro Virtual Camera registered successfully!
    echo.
    echo The camera should now appear in Zoom, Discord, etc.
)

echo.
pause

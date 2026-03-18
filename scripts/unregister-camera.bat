@echo off
echo ============================================
echo   FreezeCam Pro - Unregister Virtual Camera
echo ============================================
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
    pause
    exit /b 1
)

echo Unregistering: %DLL_PATH%
regsvr32 /u /s "%DLL_PATH%"
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Unregistration failed!
) else (
    echo [OK] FreezeCam Pro Virtual Camera unregistered.
)

echo.
pause

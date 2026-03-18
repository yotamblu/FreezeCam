@echo off
echo ============================================
echo   FreezeCam Pro - Full Build
echo ============================================
echo.

:: Check for Node.js
where node >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] Node.js not found. Please install Node.js first.
    exit /b 1
)

:: Check for CMake
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [ERROR] CMake not found. Please install CMake first.
    exit /b 1
)

cd /d "%~dp0\.."

echo [1/4] Installing npm dependencies...
call npm install
if %ERRORLEVEL% neq 0 (
    echo [ERROR] npm install failed
    exit /b 1
)

echo.
echo [2/4] Building native addon...
cd native\addon
call npx node-gyp rebuild
if %ERRORLEVEL% neq 0 (
    echo [WARNING] Native addon build failed - virtual camera output will be disabled
) else (
    echo [OK] Native addon built successfully
)
cd ..\..

echo.
echo [3/4] Building virtual camera DirectShow filter...
cd native\virtual-camera

:: Try cmake from PATH first, then VS2022 bundled location
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    set CMAKE_EXE="C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
) else (
    set CMAKE_EXE=cmake
)

%CMAKE_EXE% -B build -G "Visual Studio 17 2022" -A x64 2>nul
if %ERRORLEVEL% neq 0 (
    echo Trying with Ninja generator...
    %CMAKE_EXE% -B build -G "Ninja" 2>nul
)
%CMAKE_EXE% --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo [WARNING] Virtual camera DLL build failed
    echo Make sure Visual Studio Build Tools are installed
) else (
    echo [OK] Virtual camera DLL built successfully
)
cd ..\..

echo.
echo [4/4] Rebuilding native addon for Electron...
call npx @electron/rebuild
if %ERRORLEVEL% neq 0 (
    echo [WARNING] electron-rebuild failed
)

echo.
echo ============================================
echo   Build complete!
echo ============================================
echo.
echo Next steps:
echo   1. Register virtual camera: scripts\register-camera.bat (run as admin)
echo   2. Start the app: npm start
echo.

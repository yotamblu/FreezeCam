# FreezeCam Pro

Freeze your webcam on Zoom with a hotkey while a virtual background video keeps playing — making people think you're still there.

## Features

- **Virtual Camera** — appears as "FreezeCam Pro Virtual Camera" in Zoom
- **Hotkey Freeze** — press `Ctrl+Shift+F` (configurable) to freeze your video instantly
- **Smart Segmentation** — person segmentation freezes too, so you don't look like a ghost
- **Virtual Background** — add any video as your background
- **Bilingual UI** — Hebrew (default) and English, with full RTL support
- **Liquid Glass UI** — beautiful, modern interface inspired by glassmorphism
- **One-click Installer** — setup EXE handles everything: drivers, registry, and all

## Installation (End Users)

Download and run **FreezeCam Pro Setup.exe**. The installer:

1. Installs the app to Program Files
2. Registers the virtual camera driver
3. Configures Windows for DirectShow camera compatibility
4. Creates Start Menu shortcuts and an uninstaller

**Requirements:** Windows 10/11 (64-bit), Zoom

## Usage

1. Select your webcam from the dropdown
2. (Optional) Choose a background video file
3. Open Zoom → Settings → Video → select **"FreezeCam Pro Virtual Camera"**
4. Press **Ctrl+Shift+F** to freeze/unfreeze

## Development

### Prerequisites

- **Windows 10/11** (64-bit)
- **Node.js** 18+
- **CMake** 3.20+
- **Visual Studio 2022 Build Tools** (C++ Desktop workload)
- **Python 3.x** (for node-gyp)

### Build from Source

```bash
git clone https://github.com/yotamblu/FreezeCam.git
cd FreezeCam

# Full build (npm install + native addon + virtual camera DLL)
scripts\build-all.bat

# Register the virtual camera (requires admin)
scripts\register-camera.bat

# Disable Windows Frame Server (required for Zoom, requires admin)
scripts\disable-frameserver.bat

# Launch
npm start
```

### Build the Installer

```bash
npm run dist
```

Output: `installer-output/FreezeCam Pro Setup 1.0.0.exe`

## Architecture

```
FreezeCam/
├── src/
│   ├── main/           # Electron main process
│   └── renderer/       # UI (HTML/CSS/JS + MediaPipe segmentation)
├── native/
│   ├── addon/          # Node.js N-API addon (shared memory bridge)
│   ├── virtual-camera/ # DirectShow virtual camera filter (C++ DLL)
│   └── shared/         # Shared memory header definitions
├── installer/          # NSIS installer hooks
└── scripts/            # Build & registration scripts
```

### How It Works

1. **Electron app** captures the webcam via `getUserMedia` and runs MediaPipe body segmentation
2. Composites the person over a background video on a canvas
3. **Native addon** writes processed frames to Windows shared memory
4. **DirectShow virtual camera** (COM DLL) reads frames from shared memory and serves them to Zoom

### Freeze Behavior

When freeze is activated:
- The current person image **and** segmentation mask are captured
- The background video keeps playing normally
- The frozen person is composited on top of the live background
- Result: looks like you're still sitting there, naturally

## Uninstall

Use the Windows uninstaller (Settings → Apps → FreezeCam Pro), or manually:

```bash
scripts\unregister-camera.bat
scripts\enable-frameserver.bat
```


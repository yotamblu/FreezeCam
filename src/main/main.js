const { app, BrowserWindow, ipcMain, globalShortcut, dialog } = require('electron');
const path = require('path');

const isPacked = app.isPackaged;

let mainWindow = null;
let currentHotkey = 'Ctrl+Shift+F';
let isFrozen = false;

function log(msg, ...args) {
  console.log(`[FreezeCam][Main] ${msg}`, ...args);
}
function logErr(msg, ...args) {
  console.error(`[FreezeCam][Main][ERROR] ${msg}`, ...args);
}

function resourcePath(...segments) {
  if (isPacked) {
    return path.join(process.resourcesPath, ...segments);
  }
  return path.join(__dirname, '..', '..', ...segments);
}

function createWindow() {
  log('Creating window...');
  mainWindow = new BrowserWindow({
    width: 520,
    height: 740,
    minWidth: 480,
    minHeight: 700,
    frame: false,
    transparent: true,
    resizable: true,
    icon: resourcePath('logo.png'),
    webPreferences: {
      preload: path.join(__dirname, 'preload.js'),
      contextIsolation: true,
      nodeIntegration: false,
      backgroundThrottling: false,
    },
    backgroundColor: '#00000000',
    show: false,
  });

  mainWindow.loadFile(path.join(__dirname, '..', 'renderer', 'index.html'));

  mainWindow.once('ready-to-show', () => {
    log('Window ready to show');
    mainWindow.show();
    if (!isPacked) {
      mainWindow.webContents.openDevTools({ mode: 'detach' });
    }
  });

  mainWindow.webContents.on('console-message', (_e, level, message, line, sourceId) => {
    const tag = ['VERBOSE', 'INFO', 'WARN', 'ERROR'][level] || 'LOG';
    console.log(`[Renderer][${tag}] ${message}  (${sourceId}:${line})`);
  });

  mainWindow.on('closed', () => {
    mainWindow = null;
  });

  registerHotkey(currentHotkey);
}

function registerHotkey(key) {
  log('Registering hotkey:', key);
  globalShortcut.unregisterAll();
  try {
    globalShortcut.register(key, () => {
      isFrozen = !isFrozen;
      log('Hotkey pressed, frozen =', isFrozen);
      if (mainWindow) {
        mainWindow.webContents.send('toggle-freeze', isFrozen);
      }
    });
    currentHotkey = key;
    log('Hotkey registered OK:', key);
    return true;
  } catch (e) {
    logErr('Hotkey registration failed:', e);
    return false;
  }
}

// IPC handlers
ipcMain.handle('window:minimize', () => mainWindow?.minimize());
ipcMain.handle('window:maximize', () => {
  if (mainWindow?.isMaximized()) {
    mainWindow.unmaximize();
  } else {
    mainWindow?.maximize();
  }
});
ipcMain.handle('window:close', () => mainWindow?.close());

ipcMain.handle('hotkey:set', (_event, key) => {
  const success = registerHotkey(key);
  return { success, key: currentHotkey };
});

ipcMain.handle('hotkey:get', () => currentHotkey);

ipcMain.handle('freeze:toggle', () => {
  isFrozen = !isFrozen;
  log('Freeze toggled via IPC, frozen =', isFrozen);
  if (mainWindow) {
    mainWindow.webContents.send('toggle-freeze', isFrozen);
  }
  return isFrozen;
});

ipcMain.handle('freeze:get', () => isFrozen);

ipcMain.handle('dialog:openVideo', async () => {
  log('Opening video file dialog...');
  const result = await dialog.showOpenDialog(mainWindow, {
    title: 'Select Background Video',
    filters: [
      { name: 'Video Files', extensions: ['mp4', 'webm', 'avi', 'mov', 'mkv'] },
    ],
    properties: ['openFile'],
  });
  if (result.canceled) {
    log('Video dialog canceled');
    return null;
  }
  log('Video selected:', result.filePaths[0]);
  return result.filePaths[0];
});

let nativeAddon = null;
function loadNativeAddon() {
  const addonPath = isPacked
    ? path.join(process.resourcesPath, 'native', 'freezecam_addon.node')
    : path.join(__dirname, '..', '..', 'native', 'addon', 'build', 'Release', 'freezecam_addon.node');
  log('Attempting to load native addon from:', addonPath);
  try {
    nativeAddon = require(addonPath);
    log('Native addon loaded successfully. Exports:', Object.keys(nativeAddon));
    return true;
  } catch (e) {
    logErr('Native addon load FAILED:', e.message);
    logErr('Stack:', e.stack);
    return false;
  }
}

ipcMain.handle('native:init', () => {
  log('native:init called');
  if (!nativeAddon) {
    log('Addon not loaded yet, loading now...');
    loadNativeAddon();
  }
  if (nativeAddon) {
    try {
      const result = nativeAddon.init();
      log('native:init result:', result);
      return true;
    } catch (e) {
      logErr('native:init FAILED:', e.message);
      logErr('Stack:', e.stack);
      return false;
    }
  }
  logErr('native:init - addon is null, cannot init');
  return false;
});

let frameSendCount = 0;
let lastFrameLogTime = 0;

ipcMain.handle('native:sendFrame', (_event, buffer, width, height) => {
  if (nativeAddon) {
    try {
      const buf = Buffer.from(buffer);
      nativeAddon.sendFrame(buf, width, height);
      frameSendCount++;
      const now = Date.now();
      if (now - lastFrameLogTime > 3000) {
        log(`Frames sent: ${frameSendCount}, latest size: ${buf.length}, dims: ${width}x${height}`);
        lastFrameLogTime = now;
      }
      return true;
    } catch (e) {
      logErr('sendFrame error:', e.message);
      return false;
    }
  }
  return false;
});

ipcMain.handle('native:shutdown', () => {
  log('native:shutdown called');
  if (nativeAddon) {
    try {
      nativeAddon.shutdown();
      log('native:shutdown OK');
      return true;
    } catch (e) {
      logErr('native:shutdown error:', e.message);
      return false;
    }
  }
  return false;
});

app.whenReady().then(() => {
  log('App ready');
  createWindow();
  loadNativeAddon();
});

app.on('window-all-closed', () => {
  log('All windows closed, quitting');
  globalShortcut.unregisterAll();
  if (nativeAddon) {
    try { nativeAddon.shutdown(); } catch {}
  }
  app.quit();
});

app.on('will-quit', () => {
  globalShortcut.unregisterAll();
});

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('freezecam', {
  // Window controls
  minimize: () => ipcRenderer.invoke('window:minimize'),
  maximize: () => ipcRenderer.invoke('window:maximize'),
  close: () => ipcRenderer.invoke('window:close'),

  // Hotkey
  setHotkey: (key) => ipcRenderer.invoke('hotkey:set', key),
  getHotkey: () => ipcRenderer.invoke('hotkey:get'),

  // Freeze
  toggleFreeze: () => ipcRenderer.invoke('freeze:toggle'),
  getFreezeState: () => ipcRenderer.invoke('freeze:get'),
  onToggleFreeze: (callback) => {
    ipcRenderer.on('toggle-freeze', (_event, frozen) => callback(frozen));
  },

  // Background video
  openVideoDialog: () => ipcRenderer.invoke('dialog:openVideo'),

  // Native addon
  initNative: () => ipcRenderer.invoke('native:init'),
  sendFrame: (buffer, width, height) => ipcRenderer.invoke('native:sendFrame', buffer, width, height),
  shutdownNative: () => ipcRenderer.invoke('native:shutdown'),
});

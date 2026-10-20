const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('JWPLCHMINative', {
  isNative: true,
  selectSketchDirectory: () => ipcRenderer.invoke('jwplc-hmi:select-sketch-directory'),
  getLinkedSketch: () => ipcRenderer.invoke('jwplc-hmi:get-linked-sketch'),
  unlinkSketch: () => ipcRenderer.invoke('jwplc-hmi:unlink-sketch'),
  setLinkedSketch: (path) => ipcRenderer.invoke('jwplc-hmi:set-linked-sketch', { path }),
  writeGeneratedHeader: (text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-generated-header', { text, overwrite }),
  writeProject: (fileName, text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-project', { fileName, text, overwrite }),
  writeGuide: (text) => ipcRenderer.invoke('jwplc-hmi:write-guide', { text }),
  readProject: () => ipcRenderer.invoke('jwplc-hmi:read-project'),
  openSketchIno: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-ino'),
  openSketchFolder: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-folder'),
  onSketchOpened: (callback) => {
    ipcRenderer.on('jwplc-hmi:sketch-opened', (_event, info) => callback(info));
  }
});

const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('JWPLCHMINative', {
  isNative: true,
  selectSketchDirectory: () => ipcRenderer.invoke('jwplc-hmi:select-sketch-directory'),
  getLinkedSketch: () => ipcRenderer.invoke('jwplc-hmi:get-linked-sketch'),
  writeGeneratedHeader: (text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-generated-header', { text, overwrite }),
  writeProject: (fileName, text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-project', { fileName, text, overwrite }),
  writeGuide: (text) => ipcRenderer.invoke('jwplc-hmi:write-guide', { text }),
  readProject: () => ipcRenderer.invoke('jwplc-hmi:read-project'),
  openSketchIno: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-ino'),
  openSketchFolder: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-folder')
});

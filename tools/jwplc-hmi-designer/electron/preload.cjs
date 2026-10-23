const { contextBridge, ipcRenderer } = require('electron');

contextBridge.exposeInMainWorld('JWPLCHMINative', {
  isNative: true,
  selectSketchDirectory: () => ipcRenderer.invoke('jwplc-hmi:select-sketch-directory'),
  createSketch: (suggestedName, inoText) =>
    ipcRenderer.invoke('jwplc-hmi:create-sketch', { suggestedName, inoText }),
  getLinkedSketch: () => ipcRenderer.invoke('jwplc-hmi:get-linked-sketch'),
  unlinkSketch: () => ipcRenderer.invoke('jwplc-hmi:unlink-sketch'),
  setLinkedSketch: (path) => ipcRenderer.invoke('jwplc-hmi:set-linked-sketch', { path }),
  writeGeneratedHeader: (text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-generated-header', { text, overwrite }),
  writeProject: (fileName, text, overwrite = false) =>
    ipcRenderer.invoke('jwplc-hmi:write-project', { fileName, text, overwrite }),
  writeGuide: (text) => ipcRenderer.invoke('jwplc-hmi:write-guide', { text }),
  readProject: () => ipcRenderer.invoke('jwplc-hmi:read-project'),
  openProjectFile: () => ipcRenderer.invoke('jwplc-hmi:open-project-file'),
  openSketchIno: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-ino'),
  openSketchFolder: () => ipcRenderer.invoke('jwplc-hmi:open-sketch-folder'),
  onSketchOpened: (callback) => {
    ipcRenderer.on('jwplc-hmi:sketch-opened', (_event, info) => callback(info));
  },
  // Selector de puertos serie propio (poc/designer-port-picker.js).
  registerSerialPortPicker: ({ onRequest, onList }) => {
    ipcRenderer.on('jwplc-hmi:serial-port-request', (_event, request) => onRequest(request));
    ipcRenderer.on('jwplc-hmi:serial-port-list', (_event, update) => onList(update));
    ipcRenderer.send('jwplc-hmi:serial-picker-ready');
  },
  chooseSerialPort: (requestId, portId) =>
    ipcRenderer.send('jwplc-hmi:serial-port-chosen', { requestId, portId: portId || '' })
});

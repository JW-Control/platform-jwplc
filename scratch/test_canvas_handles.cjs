const { app, BrowserWindow } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 1400, height: 900 });
  const htmlPath = path.resolve(__dirname, '../tools/jwplc-hmi-designer/poc/index.html');
  await win.loadFile(htmlPath);
  await new Promise(r => setTimeout(r, 1200));

  const result = await win.webContents.executeJavaScript(`
    (async () => {
      // Create Pixel 1 at (30, 30) with 20x20 pixels
      const pm1 = window.JWPLCHMIPixelMaps.create();
      pm1.x = 30;
      pm1.y = 30;
      for (let y = 0; y < 10; y++) {
        for (let x = 0; x < 10; x++) {
          pm1.pixels.push({ x, y, color: 0xF800 });
        }
      }
      window.JWPLCHMIPixelMaps.refresh();

      // Deselect all
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await new Promise(r => setTimeout(r, 100));

      // Click on tree row for Pixel 1
      const row1 = document.querySelector('.a11-pixel-row[data-pixel-key="' + pm1.key + '"]');
      if (row1) row1.click();
      await new Promise(r => setTimeout(r, 150));

      // Inspect selection handles on displayCanvas
      const canvas = document.getElementById('displayCanvas');
      const ctx = canvas.getContext('2d');
      const zoom = window.JWPLCHMIEditor.getZoom();

      // Pixel 1 bounds: x=30, y=30, w=10, h=10
      // Outline top-left corner is at (30 * zoom, 30 * zoom)
      // Check handles returned by editor
      const handles = window.JWPLCHMIEditor.getSelectionHandles ? window.JWPLCHMIEditor.getSelectionHandles([]) : [];

      return {
        selectedKeys: window.JWPLCHMIPixelMaps.getSelectedKeys(),
        handlesCount: handles.length,
        tlHandle: handles[0],
        brHandle: handles[4]
      };
    })()
  `);

  console.log('TREE SELECTION CANVAS RESULT:', JSON.stringify(result, null, 2));
  app.quit();
});

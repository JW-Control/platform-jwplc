const { app, BrowserWindow } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 1400, height: 900 });
  const htmlPath = path.resolve(__dirname, '../tools/jwplc-hmi-designer/poc/index.html');
  await win.loadFile(htmlPath);
  await new Promise(r => setTimeout(r, 1000));

  const result = await win.webContents.executeJavaScript(`
    (async () => {
      // 1. Click brush tool
      const brushBtn = document.querySelector('.tool[data-tool="pixel"]');
      brushBtn.click();
      await new Promise(r => setTimeout(r, 100));

      // 2. Switch to Apariencia subtab in pixel inspector
      const appearTab = document.querySelector('#pixelSubTabs [data-pixel-subtab="appearance"]');
      if (appearTab) appearTab.click();
      await new Promise(r => setTimeout(r, 100));

      // 3. Select Green (0x07E0 = 2016) in pixelMapColorEditorHost
      const host = document.getElementById('pixelMapColorEditorHost');
      const greenBtn = host ? host.querySelector('[data-pixel-palette="2016"]') : null;
      if (greenBtn) greenBtn.click();
      await new Promise(r => setTimeout(r, 100));

      // 4. Draw a stroke on canvas
      const canvas = document.getElementById('displayCanvas');
      const rect = canvas.getBoundingClientRect();
      canvas.dispatchEvent(new PointerEvent('pointerdown', { clientX: rect.left + 50, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointermove', { clientX: rect.left + 80, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointerup', { clientX: rect.left + 80, clientY: rect.top + 50, bubbles: true }));

      // 5. Inspect the drawn pixels in the active map
      const map = window.JWPLCHMIPixelMaps.getSelected();
      const pixelColors = map ? map.pixels.map(p => p.color) : [];

      return {
        appearTabFound: Boolean(appearTab),
        greenBtnFound: Boolean(greenBtn),
        pixelMapColorInternal: window.JWPLCHMIPixelMaps.getActiveColor(),
        editorSelectedColor: window.JWPLCHMIEditor.getSelectedColor(),
        drawnPixelColor: pixelColors[0]
      };
    })()
  `);

  console.log('TEST BRUSH COLOR RESULT:', JSON.stringify(result, null, 2));
  app.quit();
});

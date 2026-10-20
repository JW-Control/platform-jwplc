const { app, BrowserWindow } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 1400, height: 900 });
  const htmlPath = path.resolve(__dirname, '../tools/jwplc-hmi-designer/poc/index.html');
  await win.loadFile(htmlPath);
  await new Promise(r => setTimeout(r, 1000));

  const result = await win.webContents.executeJavaScript(`
    (async () => {
      // 1. Click brush tool and draw stroke
      const brushBtn = document.querySelector('.tool[data-tool="pixel"]');
      brushBtn.click();
      await new Promise(r => setTimeout(r, 100));

      const canvas = document.getElementById('displayCanvas');
      const rect = canvas.getBoundingClientRect();
      canvas.dispatchEvent(new PointerEvent('pointerdown', { clientX: rect.left + 50, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointermove', { clientX: rect.left + 80, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointerup', { clientX: rect.left + 80, clientY: rect.top + 50, bubbles: true }));

      const map = window.JWPLCHMIPixelMaps.getSelected();
      const colorBefore = map.pixels[0].color;

      // 2. Switch to Apariencia subtab
      const appearTab = document.querySelector('#pixelSubTabs [data-pixel-subtab="appearance"]');
      if (appearTab) appearTab.click();
      await new Promise(r => setTimeout(r, 100));

      // 3. Click Blue (0x001F = 31) in pixelMapColorEditorHost
      const host = document.getElementById('pixelMapColorEditorHost');
      const blueBtn = host ? host.querySelector('[data-pixel-palette="31"]') : null;
      if (blueBtn) blueBtn.click();
      await new Promise(r => setTimeout(r, 100));

      const colorAfter = map.pixels[0].color;

      // 4. Verify on canvas
      const zoom = window.JWPLCHMIEditor.getZoom();
      const ctx = canvas.getContext('2d');
      const pixelData = ctx.getImageData((map.x + map.pixels[0].x) * zoom, (map.y + map.pixels[0].y) * zoom, 1, 1).data;

      return {
        colorBefore,
        colorAfter,
        blueBtnFound: Boolean(blueBtn),
        pixelRgba: Array.from(pixelData)
      };
    })()
  `);

  console.log('TEST BRUSH RECOLOR RESULT:', JSON.stringify(result, null, 2));
  app.quit();
});

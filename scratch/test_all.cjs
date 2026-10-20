const { app, BrowserWindow } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 1400, height: 900 });
  const htmlPath = path.resolve(__dirname, '../tools/jwplc-hmi-designer/poc/index.html');
  await win.loadFile(htmlPath);
  await new Promise(r => setTimeout(r, 1200));

  const report = await win.webContents.executeJavaScript(`
    (async () => {
      const results = {};

      // 1. ESCAPE DESELECT
      const brushBtn = document.querySelector('.tool[data-tool="pixel"]');
      brushBtn.click();
      await new Promise(r => setTimeout(r, 100));
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await new Promise(r => setTimeout(r, 100));
      const toolAfterEsc = window.JWPLCHMIEditor.getSelectedTool();
      const hasActive = brushBtn.classList.contains('active');
      results.escape = { pass: toolAfterEsc === 'pointer' && !hasActive };

      // 2. DRAW PIXEL 1 -> DRAW LINE -> DRAW PIXEL 2
      brushBtn.click();
      await new Promise(r => setTimeout(r, 100));
      const pm1 = window.JWPLCHMIPixelMaps.getSelected();
      pm1.pixels.push({ x: 5, y: 5, color: 0xF800 });
      window.JWPLCHMIPixelMaps.refresh();

      const lineBtn = document.querySelector('.tool[data-tool="line"]');
      lineBtn.click();
      const canvas = document.getElementById('displayCanvas');
      const rect = canvas.getBoundingClientRect();
      canvas.dispatchEvent(new PointerEvent('pointerdown', { clientX: rect.left + 50, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointermove', { clientX: rect.left + 100, clientY: rect.top + 100, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointerup', { clientX: rect.left + 100, clientY: rect.top + 100, bubbles: true }));

      brushBtn.click();
      await new Promise(r => setTimeout(r, 100));
      const countAfter = window.JWPLCHMIPixelMaps.getAll().length;
      const pm2 = window.JWPLCHMIPixelMaps.getSelected();
      results.new_pixel_object = { pass: countAfter === 2 && pm2.key !== pm1.key };

      // 3. PIXEL STROKE COLOR
      // Switch to pixel Apariencia subtab
      const pixelAppearTab = document.querySelector('#pixelSubTabs [data-pixel-subtab="appearance"]');
      if (pixelAppearTab) pixelAppearTab.click();
      await new Promise(r => setTimeout(r, 100));

      // Click Green (0x07E0 = 2016) in pixelMapColorEditorHost
      const host = document.getElementById('pixelMapColorEditorHost');
      const greenBtn = host.querySelector('[data-pixel-palette="2016"]');
      if (greenBtn) greenBtn.click();
      await new Promise(r => setTimeout(r, 100));

      // Draw stroke with brush
      canvas.dispatchEvent(new PointerEvent('pointerdown', { clientX: rect.left + 150, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointermove', { clientX: rect.left + 180, clientY: rect.top + 50, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointerup', { clientX: rect.left + 180, clientY: rect.top + 50, bubbles: true }));

      const pm2Pixels = pm2.pixels.map(p => p.color);
      results.stroke_color = {
        pass: pm2Pixels.length > 0 && pm2Pixels.every(c => c === 2016),
        pixelsCount: pm2Pixels.length,
        color: pm2Pixels[0]
      };

      // 4. LINE COLOR IN APARIENCIA
      const lineField = window.JWPLCHMIEditor.getAllFields().find(f => f.type === 'LINE');
      window.JWPLCHMIEditor.setSelectedFieldKeys([lineField.key]);
      await new Promise(r => setTimeout(r, 100));

      const shapeAppearTab = document.querySelector('#shapeSubTabs [data-shape-subtab="appear"]');
      if (shapeAppearTab) shapeAppearTab.click();
      await new Promise(r => setTimeout(r, 100));

      const shapeHost = document.getElementById('shapeColorEditorHost');
      const shapeBlueBtn = shapeHost.querySelector('[data-shape-palette="31"]');
      if (shapeBlueBtn) shapeBlueBtn.click();
      await new Promise(r => setTimeout(r, 100));

      const updatedLine = window.JWPLCHMIEditor.getAllFields().find(f => f.key === lineField.key);
      results.line_color = { pass: updatedLine.frameColor === 31, color: updatedLine.frameColor };

      // 5. NO IDLE CONFIG LEAK
      window.JWPLCHMIPixelMaps.setSelectedKey(pm2.key);
      window.JWPLCHMIInspectorState.sync();
      await new Promise(r => setTimeout(r, 100));

      const idleSection = document.getElementById('idleConfigSection');
      const idleHiddenWhenPixel = idleSection ? idleSection.hidden : true;
      const pixelSectionVisible = !document.getElementById('pixelMapControlsSection').hidden;

      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await new Promise(r => setTimeout(r, 100));
      const idleVisibleWhenNone = idleSection ? !idleSection.hidden : true;

      results.idle_leak = {
        pass: idleHiddenWhenPixel && pixelSectionVisible && idleVisibleWhenNone,
        idleHiddenWhenPixel,
        pixelSectionVisible,
        idleVisibleWhenNone
      };

      return results;
    })()
  `);

  console.log('\n===== FULL VALIDATION REPORT =====');
  for (const [k, v] of Object.entries(report)) {
    console.log(`${k}: ${v.pass ? 'PASS' : 'FAIL'}`, JSON.stringify(v));
  }

  app.quit();
});

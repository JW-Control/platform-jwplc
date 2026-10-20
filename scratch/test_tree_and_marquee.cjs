const { app, BrowserWindow } = require('electron');
const path = require('path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ show: false, width: 1400, height: 900 });
  const htmlPath = path.resolve(__dirname, '../tools/jwplc-hmi-designer/poc/index.html');
  await win.loadFile(htmlPath);
  await new Promise(r => setTimeout(r, 1200));

  const result = await win.webContents.executeJavaScript(`
    (async () => {
      const results = {};

      // Step 1: Create Pixel 1 at (30, 30)
      const pm1 = window.JWPLCHMIPixelMaps.create();
      pm1.pixels = [
        { x: 0, y: 0, color: 0xF800 },
        { x: 1, y: 0, color: 0xF800 },
        { x: 0, y: 1, color: 0xF800 },
        { x: 1, y: 1, color: 0xF800 }
      ];
      pm1.x = 30;
      pm1.y = 30;
      window.JWPLCHMIPixelMaps.refresh();

      // Step 2: Create Pixel 2 at (100, 30)
      const pm2 = window.JWPLCHMIPixelMaps.create();
      pm2.pixels = [
        { x: 0, y: 0, color: 0x07E0 },
        { x: 1, y: 0, color: 0x07E0 },
        { x: 0, y: 1, color: 0x07E0 },
        { x: 1, y: 1, color: 0x07E0 }
      ];
      pm2.x = 100;
      pm2.y = 30;
      window.JWPLCHMIPixelMaps.refresh();

      // Deselect all
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await new Promise(r => setTimeout(r, 100));

      // TEST A: SELECT FROM OBJECT TREE
      const row1 = document.querySelector('.a11-pixel-row[data-pixel-key="' + pm1.key + '"]');
      if (row1) {
        row1.click();
        await new Promise(r => setTimeout(r, 150));
      }

      const selectedKeysTree = window.JWPLCHMIPixelMaps.getSelectedKeys();
      const row1Active = document.querySelector('.a11-pixel-row[data-pixel-key="' + pm1.key + '"]')?.classList.contains('active');
      const toolAfterTreeClick = window.JWPLCHMIEditor.getSelectedTool();

      results.testA_tree_selection = {
        pass: selectedKeysTree.includes(pm1.key) && row1Active && toolAfterTreeClick === 'pointer',
        selectedKeys: selectedKeysTree,
        row1Active,
        tool: toolAfterTreeClick
      };

      // TEST B: GREEN MARQUEE SELECTING MULTIPLE PIXELS
      // Deselect all
      window.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
      await new Promise(r => setTimeout(r, 100));

      // Green marquee (crossing mode): Drag from right (200, 100) to left (0, 0)
      const canvas = document.getElementById('displayCanvas');
      const rect = canvas.getBoundingClientRect();
      const scaleX = canvas.width / rect.width;
      const scaleY = canvas.height / rect.height;
      const zoom = window.JWPLCHMIEditor.getZoom();

      const toClient = (cx, cy) => ({
        clientX: rect.left + (cx * zoom) / scaleX,
        clientY: rect.top + (cy * zoom) / scaleY
      });

      const pStart = toClient(200, 100);
      const pEnd = toClient(0, 0);

      canvas.dispatchEvent(new PointerEvent('pointerdown', { clientX: pStart.clientX, clientY: pStart.clientY, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointermove', { clientX: pEnd.clientX, clientY: pEnd.clientY, bubbles: true }));
      window.dispatchEvent(new PointerEvent('pointerup', { clientX: pEnd.clientX, clientY: pEnd.clientY, bubbles: true }));
      await new Promise(r => setTimeout(r, 200));

      const selectedKeysMarquee = window.JWPLCHMIPixelMaps.getSelectedKeys();
      const row1ActiveAfter = document.querySelector('.a11-pixel-row[data-pixel-key="' + pm1.key + '"]')?.classList.contains('active');
      const row2ActiveAfter = document.querySelector('.a11-pixel-row[data-pixel-key="' + pm2.key + '"]')?.classList.contains('active');

      results.testB_green_marquee_multiselect = {
        pass: selectedKeysMarquee.length >= 2 && selectedKeysMarquee.includes(pm1.key) && selectedKeysMarquee.includes(pm2.key) && row1ActiveAfter && row2ActiveAfter,
        selectedKeys: selectedKeysMarquee,
        row1Active: row1ActiveAfter,
        row2Active: row2ActiveAfter
      };

      return results;
    })()
  `);

  console.log('TEST RESULT:', JSON.stringify(result, null, 2));
  app.quit();
});

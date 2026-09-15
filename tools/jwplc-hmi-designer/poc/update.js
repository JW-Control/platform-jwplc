const fs = require('fs');
let code = fs.readFileSync('app.js', 'utf8');

// 1. event.button !== 0 check in pointerdown
code = code.replace(
  "displayCanvas.addEventListener('pointerdown', (event) => {\n    const point = pointFromPointer(event);",
  "displayCanvas.addEventListener('pointerdown', (event) => {\n    if (event.button !== 0) return;\n    const point = pointFromPointer(event);"
);

// 2. Hide scrollbars in styles.css
let css = fs.readFileSync('styles.css', 'utf8');
if (!css.includes('::-webkit-scrollbar')) {
  css += '\n.canvas-viewport::-webkit-scrollbar { display: none; }\n.canvas-viewport { scrollbar-width: none; }\n';
}

// 3. Update vertical toolbar styles
css = css.replace('.right-vertical-toolbar {', '.right-vertical-toolbar {\n  background: #1e1e1e;\n  border: none;\n');
if (!css.includes('.vert-btn')) {
  css += '\n.vert-btn { background: #2a2a2a; color: #ff9a43; border: none; border-radius: 8px; width: 36px; height: 36px; display: flex; align-items: center; justify-content: center; font-size: 16px; cursor: pointer; transition: background 0.2s; }\n';
  css += '.vert-btn:hover { background: #3a3a3a; }\n';
  css += '.vert-btn.toggle-btn { color: #888; }\n';
  css += '.vert-btn.toggle-btn.active { color: #ff9a43; background: #332a22; }\n';
}
fs.writeFileSync('styles.css', css);

// 4. Update app.js tool variables
code = code.replace(
  "const gridToggle = document.getElementById('gridToggle');",
  "const gridToggle = { checked: true }; document.getElementById('vertGridToggle')?.addEventListener('click', function() { gridToggle.checked = !gridToggle.checked; this.classList.toggle('active', gridToggle.checked); render(); });"
);
code = code.replace(
  "const snapToggle = document.getElementById('snapToggle');",
  "const snapToggle = { checked: true }; document.getElementById('vertSnapToggle')?.addEventListener('click', function() { snapToggle.checked = !snapToggle.checked; this.classList.toggle('active', snapToggle.checked); render(); });"
);
code = code.replace(
  "const geometryToggle = document.getElementById('geometryToggle');",
  "const geometryToggle = { checked: true }; document.getElementById('vertGeoToggle')?.addEventListener('click', function() { geometryToggle.checked = !geometryToggle.checked; this.classList.toggle('active', geometryToggle.checked); render(); });"
);
// Replace gridSizeSelect with a static value
code = code.replace(
  "const gridSizeSelect = document.getElementById('gridSizeSelect');",
  "const gridSizeSelect = { value: '8' };"
);

// Zoom logic
code = code.replace(
  "zoomSelect.addEventListener('change', () => {\n    zoom = Number(zoomSelect.value);\n    render();\n  });",
  "document.getElementById('zoomInBtn')?.addEventListener('click', () => { zoom = Math.min(8, zoom + 1); render(); });\n  document.getElementById('zoomOutBtn')?.addEventListener('click', () => { zoom = Math.max(1, zoom - 1); render(); });\n  document.getElementById('fitCanvasBtn')?.addEventListener('click', () => { zoom = 1; render(); });"
);

// Remove zoomSelect from wheel event
code = code.replace(
  "const zoomSelect = document.getElementById('zoomSelect');\n      let currentIndex = zoomSelect.selectedIndex;",
  ""
);
code = code.replace(
  /if \(e\.deltaY < 0\) \{[\s\S]*?zoom = Number\(zoomSelect\.value\);/, 
  "if (e.deltaY < 0) { zoom = Math.min(8, zoom + 1); } else { zoom = Math.max(1, zoom - 1); }"
);

// 5. Draw selection indicator in render()
const selectionRender = `
    const sel = selectedField();
    if (sel && sel.type !== 'PIXELMAP') {
      const g = computeFieldGeometry(sel);
      let left = g.fieldX;
      let top = g.fieldY;
      let w = g.fieldW;
      let h = g.fieldH;
      if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(sel.type)) {
        left = Math.min(sel.x, sel.x2);
        top = Math.min(sel.y, sel.y2);
        w = Math.abs(sel.x2 - sel.x) || 1;
        h = Math.abs(sel.y2 - sel.y) || 1;
      }
      displayCtx.save();
      displayCtx.strokeStyle = '#00ff00';
      displayCtx.lineWidth = 1;
      displayCtx.setLineDash([4, 4]);
      displayCtx.strokeRect(left * zoom - 1.5, top * zoom - 1.5, w * zoom + 3, h * zoom + 3);
      displayCtx.restore();
    }
`;
code = code.replace("updateMetrics();", selectionRender + "\n    updateMetrics();");

// Middle click pan (event.button === 1)
code = code.replace(
  "if (e.button === 2) { // Right click",
  "if (e.button === 2 || e.button === 1) { // Right or Middle click"
);
code = code.replace(
  "if (e.button === 2 && isPanning) {",
  "if ((e.button === 2 || e.button === 1) && isPanning) {"
);

fs.writeFileSync('app.js', code);
console.log('Update successful');

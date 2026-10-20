with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. dragInitialPixelMaps declaration
c = c.replace(
'''  let dragInitialPointer = null;
  let dragInitialFields = null;''',
'''  let dragInitialPointer = null;
  let dragInitialFields = null;
  let dragInitialPixelMaps = null;'''
)

# 2. Update getSelectionBounds to accept pms
old_gsb = '''  function getSelectionBounds(fields) {
    if (!fields || fields.length === 0) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    for (const f of fields) {
      const b = getFieldBounds(f);
      if (b.minX < minX) minX = b.minX;
      if (b.maxX > maxX) maxX = b.maxX;
      if (b.minY < minY) minY = b.minY;
      if (b.maxY > maxY) maxY = b.maxY;
    }
    if (minX > maxX || minY > maxY) return null;'''

new_gsb = '''  function getSelectionBounds(fields, pms = []) {
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    if (fields && fields.length > 0) {
      for (const f of fields) {
        const b = getFieldBounds(f);
        if (b.minX < minX) minX = b.minX;
        if (b.maxX > maxX) maxX = b.maxX;
        if (b.minY < minY) minY = b.minY;
        if (b.maxY > maxY) maxY = b.maxY;
      }
    }
    if (pms && pms.length > 0) {
      for (const pm of pms) {
        const b = getPixelMapBounds(pm);
        if (b) {
          if (b.minX < minX) minX = b.minX;
          if (b.maxX > maxX) maxX = b.maxX;
          if (b.minY < minY) minY = b.minY;
          if (b.maxY > maxY) maxY = b.maxY;
        }
      }
    }
    if (minX > maxX || minY > maxY || minX === Infinity) return null;'''

assert old_gsb in c, "old_gsb not found in app.js"
c = c.replace(old_gsb, new_gsb)

# 3. Update getSelectionHandles to accept (fields, pms = null)
old_gsh = '''function getSelectionHandles(fields) {
    if (!fields || fields.length === 0) {
      const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
        (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
      );
      if (selPms.length > 0 && selectedTool !== 'pixel' && selectedTool !== 'erase') {
        const unionB = getPixelMapsUnionBounds(selPms);
        if (unionB) {
          const midX = (unionB.screenX1 + unionB.screenX2) / 2;
          const midY = (unionB.screenY1 + unionB.screenY2) / 2;
          return [
            { id: 'tl', x: unionB.screenX1, y: unionB.screenY1, cursor: 'nwse-resize' },
            { id: 'tc', x: midX, y: unionB.screenY1, cursor: 'ns-resize' },
            { id: 'tr', x: unionB.screenX2, y: unionB.screenY1, cursor: 'nesw-resize' },
            { id: 'rc', x: unionB.screenX2, y: midY, cursor: 'ew-resize' },
            { id: 'br', x: unionB.screenX2, y: unionB.screenY2, cursor: 'nwse-resize' },
            { id: 'bc', x: midX, y: unionB.screenY2, cursor: 'ns-resize' },
            { id: 'bl', x: unionB.screenX1, y: unionB.screenY2, cursor: 'nesw-resize' },
            { id: 'lc', x: unionB.screenX1, y: midY, cursor: 'ew-resize' }
          ];
        }
      }
      return [];
    }

    if (fields.length === 1 && fields[0].type === 'LINE') {
      const f = fields[0];
      const x1 = (f.x ?? 0) * zoom;
      const y1 = (f.y ?? 0) * zoom;
      const x2 = (f.x2 ?? f.x ?? 0) * zoom;
      const y2 = (f.y2 ?? f.y ?? 0) * zoom;
      return [
        { id: 'lineStart', x: x1, y: y1, cursor: 'crosshair' },
        { id: 'lineEnd', x: x2, y: y2, cursor: 'crosshair' }
      ];
    }

    const b = getSelectionBounds(fields);
    if (!b) return [];

    return [
      { id: 'tl', x: b.screenX1, y: b.screenY1, cursor: 'nwse-resize' },
      { id: 'tc', x: b.screenX1 + b.screenW / 2, y: b.screenY1, cursor: 'ns-resize' },
      { id: 'tr', x: b.screenX2, y: b.screenY1, cursor: 'nesw-resize' },
      { id: 'rc', x: b.screenX2, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' },
      { id: 'br', x: b.screenX2, y: b.screenY2, cursor: 'nwse-resize' },
      { id: 'bc', x: b.screenX1 + b.screenW / 2, y: b.screenY2, cursor: 'ns-resize' },
      { id: 'bl', x: b.screenX1, y: b.screenY2, cursor: 'nesw-resize' },
      { id: 'lc', x: b.screenX1, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' }
    ];
  }'''

new_gsh = '''function getSelectionHandles(fields, pms = null) {
    const selPms = pms || (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );

    if (fields && fields.length === 1 && selPms.length === 0 && fields[0].type === 'LINE') {
      const f = fields[0];
      const x1 = (f.x ?? 0) * zoom;
      const y1 = (f.y ?? 0) * zoom;
      const x2 = (f.x2 ?? f.x ?? 0) * zoom;
      const y2 = (f.y2 ?? f.y ?? 0) * zoom;
      return [
        { id: 'lineStart', x: x1, y: y1, cursor: 'crosshair' },
        { id: 'lineEnd', x: x2, y: y2, cursor: 'crosshair' }
      ];
    }

    const b = getSelectionBounds(fields, selPms);
    if (!b) return [];

    return [
      { id: 'tl', x: b.screenX1, y: b.screenY1, cursor: 'nwse-resize' },
      { id: 'tc', x: b.screenX1 + b.screenW / 2, y: b.screenY1, cursor: 'ns-resize' },
      { id: 'tr', x: b.screenX2, y: b.screenY1, cursor: 'nesw-resize' },
      { id: 'rc', x: b.screenX2, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' },
      { id: 'br', x: b.screenX2, y: b.screenY2, cursor: 'nwse-resize' },
      { id: 'bc', x: b.screenX1 + b.screenW / 2, y: b.screenY2, cursor: 'ns-resize' },
      { id: 'bl', x: b.screenX1, y: b.screenY2, cursor: 'nesw-resize' },
      { id: 'lc', x: b.screenX1, y: b.screenY1 + b.screenH / 2, cursor: 'ew-resize' }
    ];
  }'''

assert old_gsh in c, "old_gsh not found in app.js"
c = c.replace(old_gsh, new_gsh)

# 4. Update drawSelectionAndGuides to handle combined fields and pms cleanly
old_dsag = '''  function drawSelectionAndGuides() {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );

    if (fields.length === 0 && selPms.length === 0) {
      return;
    }

    displayCtx.save();

    // Draw individual blue outline for each selected pixel map
    if (selPms.length > 0 && selectedTool !== 'pixel' && selectedTool !== 'erase') {
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1;
      selPms.forEach((pm) => {
        const b = getPixelMapBounds(pm);
        if (b) {
          displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
        }
      });
    }

    // If only pixel maps are selected (no normal fields)
    if (fields.length === 0 && selPms.length > 0) {
      if (selectedTool !== 'pixel' && selectedTool !== 'erase') {
        const unionB = getPixelMapsUnionBounds(selPms);
        if (unionB) {
          // 1. Dotted projection lines to rulers
          displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
          displayCtx.setLineDash([2, 3]);
          displayCtx.lineWidth = 1;
          displayCtx.beginPath();
          displayCtx.moveTo(unionB.screenX1 + 0.5, 0);
          displayCtx.lineTo(unionB.screenX1 + 0.5, displayCanvas.height);
          displayCtx.moveTo(unionB.screenX2 + 0.5, 0);
          displayCtx.lineTo(unionB.screenX2 + 0.5, displayCanvas.height);
          displayCtx.moveTo(0, unionB.screenY1 + 0.5);
          displayCtx.lineTo(displayCanvas.width, unionB.screenY1 + 0.5);
          displayCtx.moveTo(0, unionB.screenY2 + 0.5);
          displayCtx.lineTo(displayCanvas.width, unionB.screenY2 + 0.5);
          displayCtx.stroke();
          displayCtx.setLineDash([]);

          // 2. Union outline if multiple
          if (selPms.length > 1) {
            displayCtx.strokeStyle = 'rgba(0, 132, 255, 0.6)';
            displayCtx.setLineDash([4, 4]);
            displayCtx.strokeRect(unionB.screenX1 + 0.5, unionB.screenY1 + 0.5, unionB.screenW, unionB.screenH);
            displayCtx.setLineDash([]);
          }

          // 3. Selection handles around union
          const hs = 6;
          const handles = getSelectionHandles([]);
          handles.forEach((h) => {
            const rx = Math.round(h.x - hs / 2);
            const ry = Math.round(h.y - hs / 2);
            displayCtx.fillStyle = '#060d13';
            displayCtx.fillRect(rx, ry, hs, hs);
            displayCtx.strokeStyle = '#0084ff';
            displayCtx.lineWidth = 1.5;
            displayCtx.strokeRect(rx + 0.5, ry + 0.5, hs - 1, hs - 1);
          });
        }
      }
      displayCtx.restore();
      return;
    }

    const b = getSelectionBounds(fields);
    if (!b) return;

    displayCtx.save();

    // 1. Dotted projection lines to rulers (always use AABB for guides)
    displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
    displayCtx.setLineDash([2, 3]);
    displayCtx.lineWidth = 1;
    displayCtx.beginPath();
    displayCtx.moveTo(b.screenX1 + 0.5, 0);
    displayCtx.lineTo(b.screenX1 + 0.5, displayCanvas.height);
    displayCtx.moveTo(b.screenX2 + 0.5, 0);
    displayCtx.lineTo(b.screenX2 + 0.5, displayCanvas.height);
    displayCtx.moveTo(0, b.screenY1 + 0.5);
    displayCtx.lineTo(displayCanvas.width, b.screenY1 + 0.5);
    displayCtx.moveTo(0, b.screenY2 + 0.5);
    displayCtx.lineTo(displayCanvas.width, b.screenY2 + 0.5);
    displayCtx.stroke();
    displayCtx.setLineDash([]);

    // 2. Bounding outline
    displayCtx.strokeStyle = '#0084ff';
    displayCtx.lineWidth = 1;

    if (fields.length === 1 && fields[0].type === 'LINE') {
      displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
    } else if (fields.length === 1 && ['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
      const f = fields[0];
      const angleDeg = Number(f.rotation) || 0;
      if (angleDeg === 0) {
        displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
      } else {
        const lb = getShapeLocalBounds(f);
        const x1 = f.x ?? 0; const y1 = f.y ?? 0;
        const x2 = f.x2 ?? x1; const y2 = f.y2 ?? y1;
        const cx = (x1 + x2) / 2; const cy = (y1 + y2) / 2;
        const flipH = Boolean(f.flipH); const flipV = Boolean(f.flipV);
        const corners = [
          [lb.minX, lb.minY], [lb.maxX + 1, lb.minY], [lb.maxX + 1, lb.maxY + 1], [lb.minX, lb.maxY + 1]
        ].map(([px, py]) => {
          const r = transformShapePoint(px, py, cx, cy, angleDeg, flipH, flipV);
          return [r.x * zoom + 0.5, r.y * zoom + 0.5];
        });
        displayCtx.beginPath();
        displayCtx.moveTo(corners[0][0], corners[0][1]);
        for (let i = 1; i < corners.length; i++) displayCtx.lineTo(corners[i][0], corners[i][1]);
        displayCtx.closePath();
        displayCtx.stroke();
      }
    } else {
      displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
    }

    // 3. Selection Handles (Only on AABB bounds)
    const hs = 6;
    const handles = getSelectionHandles(fields);
    handles.forEach((h) => {
      const rx = Math.round(h.x - hs / 2);
      const ry = Math.round(h.y - hs / 2);
      displayCtx.fillStyle = '#060d13';
      displayCtx.fillRect(rx, ry, hs, hs);
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1.5;
      displayCtx.strokeRect(rx + 0.5, ry + 0.5, hs - 1, hs - 1);
    });

    displayCtx.restore();
  }'''

new_dsag = '''  function drawSelectionAndGuides() {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );

    const totalCount = fields.length + selPms.length;
    if (totalCount === 0 || selectedTool === 'pixel' || selectedTool === 'erase') {
      return;
    }

    displayCtx.save();

    // 1. Draw individual blue outline for each selected pixel map
    if (selPms.length > 0) {
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1;
      selPms.forEach((pm) => {
        const b = getPixelMapBounds(pm);
        if (b) {
          displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
        }
      });
    }

    // 2. Draw individual outline for each selected shape/field
    if (fields.length > 0) {
      displayCtx.strokeStyle = '#0084ff';
      displayCtx.lineWidth = 1;
      fields.forEach((f) => {
        const fb = getFieldBounds(f);
        if (fb) {
          const sx1 = fb.minX * zoom;
          const sy1 = fb.minY * zoom;
          const sw = (fb.maxX - fb.minX + 1) * zoom;
          const sh = (fb.maxY - fb.minY + 1) * zoom;
          displayCtx.strokeRect(sx1 + 0.5, sy1 + 0.5, sw, sh);
        }
      });
    }

    // 3. Combined union bounds for guides, outer box and handles
    const b = getSelectionBounds(fields, selPms);
    if (b) {
      // Dotted projection lines to rulers
      displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
      displayCtx.setLineDash([2, 3]);
      displayCtx.lineWidth = 1;
      displayCtx.beginPath();
      displayCtx.moveTo(b.screenX1 + 0.5, 0);
      displayCtx.lineTo(b.screenX1 + 0.5, displayCanvas.height);
      displayCtx.moveTo(b.screenX2 + 0.5, 0);
      displayCtx.lineTo(b.screenX2 + 0.5, displayCanvas.height);
      displayCtx.moveTo(0, b.screenY1 + 0.5);
      displayCtx.lineTo(displayCanvas.width, b.screenY1 + 0.5);
      displayCtx.moveTo(0, b.screenY2 + 0.5);
      displayCtx.lineTo(displayCanvas.width, b.screenY2 + 0.5);
      displayCtx.stroke();
      displayCtx.setLineDash([]);

      // Outer dashed bounding box if multiple items selected
      if (totalCount > 1) {
        displayCtx.strokeStyle = 'rgba(0, 132, 255, 0.6)';
        displayCtx.setLineDash([4, 4]);
        displayCtx.strokeRect(b.screenX1 + 0.5, b.screenY1 + 0.5, b.screenW, b.screenH);
        displayCtx.setLineDash([]);
      }

      // Selection Handles around union bounding box
      const hs = 6;
      const handles = getSelectionHandles(fields, selPms);
      handles.forEach((h) => {
        const rx = Math.round(h.x - hs / 2);
        const ry = Math.round(h.y - hs / 2);
        displayCtx.fillStyle = '#060d13';
        displayCtx.fillRect(rx, ry, hs, hs);
        displayCtx.strokeStyle = '#0084ff';
        displayCtx.lineWidth = 1.5;
        displayCtx.strokeRect(rx + 0.5, ry + 0.5, hs - 1, hs - 1);
      });
    }

    displayCtx.restore();
  }'''

assert old_dsag in c, "old_dsag not found in app.js"
c = c.replace(old_dsag, new_dsag)

# 5. Drag handling for both fields and pixel maps in handlePointerDown, handlePointerMove, handlePointerUp
old_hpd_drag = '''      if (isPointInsideSelection(point)) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
        dragInitialFields = curSelected.map((f) => ({ ...f }));
        render();
        return;
      }'''

new_hpd_drag = '''      if (isPointInsideSelection(point)) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
        dragInitialFields = curSelected.map((f) => ({ ...f }));
        dragInitialPixelMaps = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).map((m) => ({ key: m.key, x: m.x, y: m.y }));
        render();
        return;
      }'''

assert old_hpd_drag in c, "old_hpd_drag not found in app.js"
c = c.replace(old_hpd_drag, new_hpd_drag)

old_hpm_drag = '''        curSelected.forEach((f) => {
          const orig = dragInitialFields?.find((o) => o.key === f.key);
          if (!orig) return;
          if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
            if (f.x2 !== undefined) f.x2 = orig.x2 + dx;
            if (f.y2 !== undefined) f.y2 = orig.y2 + dy;
          } else {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
          }
        });'''

new_hpm_drag = '''        curSelected.forEach((f) => {
          const orig = dragInitialFields?.find((o) => o.key === f.key);
          if (!orig) return;
          if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type)) {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
            if (f.x2 !== undefined) f.x2 = orig.x2 + dx;
            if (f.y2 !== undefined) f.y2 = orig.y2 + dy;
          } else {
            f.x = orig.x + dx;
            f.y = orig.y + dy;
          }
        });
        if (dragInitialPixelMaps && dragInitialPixelMaps.length > 0) {
          dragInitialPixelMaps.forEach((orig) => {
            const pm = window.JWPLCHMIPixelMaps?.getAll?.()?.find((x) => x.key === orig.key);
            if (pm) {
              pm.x = orig.x + dx;
              pm.y = orig.y + dy;
            }
          });
        }'''

assert old_hpm_drag in c, "old_hpm_drag not found in app.js"
c = c.replace(old_hpm_drag, new_hpm_drag)

old_hpu_clear = '''    draggingObject = false;
    dragInitialPointer = null;
    dragInitialFields = null;'''

new_hpu_clear = '''    draggingObject = false;
    dragInitialPointer = null;
    dragInitialFields = null;
    dragInitialPixelMaps = null;'''

assert old_hpu_clear in c, "old_hpu_clear not found in app.js"
c = c.replace(old_hpu_clear, new_hpu_clear)

with open('tools/jwplc-hmi-designer/poc/app.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Successfully updated app.js for unified multi-selection")

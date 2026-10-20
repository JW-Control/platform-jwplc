with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    app_code = f.read()

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    pm_code = f.read()

# ---------------------------------------------
# 1. Update app.js
# ---------------------------------------------

# 1.1 dragInitialPixelMaps declaration
old_decl = '''  let dragInitialPointer = null;
  let dragInitialFields = null;'''
new_decl = '''  let dragInitialPointer = null;
  let dragInitialFields = null;
  let dragInitialPixelMaps = null;'''
assert old_decl in app_code, "old_decl not found in app.js"
app_code = app_code.replace(old_decl, new_decl, 1)

# 1.2 getSelectionBounds
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
assert old_gsb in app_code, "old_gsb not found in app.js"
app_code = app_code.replace(old_gsb, new_gsb, 1)

# 1.3 getSelectionHandles
old_gsh = '''  function getSelectionHandles(fields) {
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

new_gsh = '''  function getSelectionHandles(fields, pms = null) {
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
assert old_gsh in app_code, "old_gsh not found in app.js"
app_code = app_code.replace(old_gsh, new_gsh, 1)

# 1.4 hitTestHandle
old_hth = '''  function hitTestHandle(event) {
    const fields = selectedFields();
    if (fields.length === 0 && !window.JWPLCHMIPixelMaps?.getSelected?.()) return null;
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const mouseScreenX = (event.clientX - rect.left) * scaleX;
    const mouseScreenY = (event.clientY - rect.top) * scaleY;

    const handles = getSelectionHandles(fields);
    const hitRadius = 12;
    for (const h of handles) {
      const dx = mouseScreenX - h.x;
      const dy = mouseScreenY - h.y;
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        return h;
      }
    }
    return null;
  }'''

new_hth = '''  function hitTestHandle(event) {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );
    if (fields.length === 0 && selPms.length === 0) return null;
    const rect = displayCanvas.getBoundingClientRect();
    const scaleX = displayCanvas.width / rect.width;
    const scaleY = displayCanvas.height / rect.height;
    const mouseScreenX = (event.clientX - rect.left) * scaleX;
    const mouseScreenY = (event.clientY - rect.top) * scaleY;

    const handles = getSelectionHandles(fields, selPms);
    const hitRadius = 12;
    for (const h of handles) {
      const dx = mouseScreenX - h.x;
      const dy = mouseScreenY - h.y;
      if (dx * dx + dy * dy <= hitRadius * hitRadius) {
        return h;
      }
    }
    return null;
  }'''
assert old_hth in app_code, "old_hth not found in app.js"
app_code = app_code.replace(old_hth, new_hth, 1)

# 1.5 isPointInsideSelection
old_ipis = '''  function isPointInsideSelection(point) {
    const fields = selectedFields();
    if (fields.length === 0) {
      const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
        (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
      );
      if (selPms.length > 0 && selectedTool !== 'pixel' && selectedTool !== 'erase') {
        return selPms.some((selPm) => selPm.pixels.some((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          return Math.abs(px - point.x) <= 2 && Math.abs(py - point.y) <= 2;
        }));
      }
      return false;
    }
    if (fields.length === 1 && fields[0].type === 'LINE') {
      const strokeSize = fields[0].borderEnabled !== false ? Math.max(1, Math.round(fields[0].size || 1)) : 1;
      if (distanceToFieldBorder(point, fields[0]) <= Math.max(2.0, strokeSize / 2 + 1.5)) return true;
      return false;
    }
    if (fields.length === 1 && ['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
      if (isPointInsideShapeArea(point.x, point.y, fields[0])) return true;
    }
    const b = getSelectionBounds(fields);
    if (!b) return false;
    const margin = 4;
    return (
      point.x >= b.minX - margin &&
      point.x <= b.maxX + margin &&
      point.y >= b.minY - margin &&
      point.y <= b.maxY + margin
    );
  }'''

new_ipis = '''  function isPointInsideSelection(point) {
    const fields = selectedFields();
    const selPms = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).filter(
      (pm) => Number(pm.page || 0) === activePage && pm.editorVisible !== false && pm.pixels?.length
    );
    const totalCount = fields.length + selPms.length;
    if (totalCount === 0) return false;

    if (fields.length === 0 && selPms.length > 0) {
      if (selectedTool !== 'pixel' && selectedTool !== 'erase') {
        return selPms.some((selPm) => selPm.pixels.some((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          return Math.abs(px - point.x) <= 2 && Math.abs(py - point.y) <= 2;
        }));
      }
      return false;
    }

    if (fields.length === 1 && selPms.length === 0) {
      if (fields[0].type === 'LINE') {
        const strokeSize = fields[0].borderEnabled !== false ? Math.max(1, Math.round(fields[0].size || 1)) : 1;
        if (distanceToFieldBorder(point, fields[0]) <= Math.max(2.0, strokeSize / 2 + 1.5)) return true;
        return false;
      }
      if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(fields[0].type)) {
        if (isPointInsideShapeArea(point.x, point.y, fields[0])) return true;
      }
    }

    if (selPms.some((pm) => pm.pixels.some((p) => Math.abs(pm.x + p.x - point.x) <= 2 && Math.abs(pm.y + p.y - point.y) <= 2))) {
      return true;
    }
    for (const f of fields) {
      if (['RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON'].includes(f.type) && isPointInsideShapeArea(point.x, point.y, f)) {
        return true;
      }
      if (f.type === 'LINE' && distanceToFieldBorder(point, f) <= 2.5) {
        return true;
      }
    }

    const b = getSelectionBounds(fields, selPms);
    if (!b) return false;
    const margin = 4;
    return (
      point.x >= b.minX - margin &&
      point.x <= b.maxX + margin &&
      point.y >= b.minY - margin &&
      point.y <= b.maxY + margin
    );
  }'''
assert old_ipis in app_code, "old_ipis not found in app.js"
app_code = app_code.replace(old_ipis, new_ipis, 1)

# 1.6 drawSelectionAndGuides
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

    // 3. Combined union bounds for guides, outer dashed box and handles
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
assert old_dsag in app_code, "old_dsag not found in app.js"
app_code = app_code.replace(old_dsag, new_dsag, 1)

# 1.7 handlePointerDown unified drag initialization
old_hpd_drag = '''    if (isPointInsideSelection(point)) {
      const curSelected = selectedFields();
      if (!curSelected.some((f) => f.locked)) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
        dragInitialFields = curSelected.map((f) => ({ ...f }));
        render();
        return;
      }
    }'''

new_hpd_drag = '''    if (isPointInsideSelection(point)) {
      const curSelected = selectedFields();
      if (!curSelected.some((f) => f.locked)) {
        draggingObject = true;
        dragInitialPointer = { ...point, rawX: point.x, rawY: point.y };
        dragInitialFields = curSelected.map((f) => ({ ...f }));
        dragInitialPixelMaps = (window.JWPLCHMIPixelMaps?.getSelectedMaps?.() || []).map((m) => ({ key: m.key, x: m.x, y: m.y }));
        render();
        return;
      }
    }'''
assert old_hpd_drag in app_code, "old_hpd_drag not found in app.js"
app_code = app_code.replace(old_hpd_drag, new_hpd_drag, 1)

# 1.8 Single-clicking a field clears pixelmap selection
old_field_hit = '''    const hit = hitTestField(point);
    if (hit) {
      setSelectedKeys([hit.key]);'''

new_field_hit = '''    const hit = hitTestField(point);
    if (hit) {
      setSelectedKeys([hit.key]);
      window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);'''
assert old_field_hit in app_code, "old_field_hit not found in app.js"
app_code = app_code.replace(old_field_hit, new_field_hit, 1)

# 1.9 handlePointerMove dragging
old_hpm_drag = '''      if (draggingObject && dragInitialPointer) {
        gestureChanged = true;
        const dx = point.x - dragInitialPointer.x;
        const dy = point.y - dragInitialPointer.y;
        curSelected.forEach((f) => {
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
        render();
        return;
      }'''

new_hpm_drag = '''      if (draggingObject && dragInitialPointer) {
        gestureChanged = true;
        const dx = point.x - dragInitialPointer.x;
        const dy = point.y - dragInitialPointer.y;
        curSelected.forEach((f) => {
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
        }
        render();
        return;
      }'''
assert old_hpm_drag in app_code, "old_hpm_drag not found in app.js"
app_code = app_code.replace(old_hpm_drag, new_hpm_drag, 1)

# 1.10 endPointer drag cleanup
old_ep_cleanup = '''    dragInitialPointer = null;
    dragInitialFields = null;
    lastPoint = null;'''

new_ep_cleanup = '''    dragInitialPointer = null;
    dragInitialFields = null;
    if (dragInitialPixelMaps && dragInitialPixelMaps.length > 0) {
      window.JWPLCHMIPixelMaps?.syncInspector?.();
      window.JWPLCHMIPixelMaps?.notifyChanged?.();
    }
    dragInitialPixelMaps = null;
    lastPoint = null;'''
assert old_ep_cleanup in app_code, "old_ep_cleanup not found in app.js"
app_code = app_code.replace(old_ep_cleanup, new_ep_cleanup, 1)

# 1.11 Marquee selection passing clearFields = false when both matched
old_marquee_match = '''          } else if (matchedList.length > 0 && matchedPixelMaps.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key));
            selectedTool = 'pointer';'''

new_marquee_match = '''          } else if (matchedList.length > 0 && matchedPixelMaps.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key), false);
            selectedTool = 'pointer';'''
assert old_marquee_match in app_code, "old_marquee_match not found in app.js"
app_code = app_code.replace(old_marquee_match, new_marquee_match, 1)

# 1.12 Export isPointInsideSelection & hitTestField on window.JWPLCHMIEditor
old_export = '''    hasFieldSelection: () => selectedFields().length > 0,
    getSelectionHandles: (fields = selectedFields()) => getSelectionHandles(fields),'''

new_export = '''    hasFieldSelection: () => selectedFields().length > 0,
    getSelectionHandles: (fields = selectedFields()) => getSelectionHandles(fields),
    isPointInsideSelection: (p) => isPointInsideSelection(p),
    hitTestField: (p) => hitTestField(p),'''
assert old_export in app_code, "old_export not found in app.js"
app_code = app_code.replace(old_export, new_export, 1)

with open('tools/jwplc-hmi-designer/poc/app.js', 'w', encoding='utf-8', newline='') as f:
    f.write(app_code)
print("app.js patched successfully!")

# ---------------------------------------------
# 2. Update designer-pixelmap.js
# ---------------------------------------------

# 2.1 Pointerdown capture handler in designer-pixelmap.js:
# Allow unified drag if editor.isPointInsideSelection(p) is true,
# don't clear fields on Shift/Ctrl, don't clear pixel maps on Shift/Ctrl
old_pm_pointerdown = '''    // currentTool === 'pointer'
    if (editor()?.hitTestHandle?.(event)) {
      return;
    }

    const hit = hitMap(p);
    if (hit) {
      clearFieldSelection();
      if (event.shiftKey || event.ctrlKey) {
        if (selectedKeys.includes(hit.key)) {
          selectedKeys = selectedKeys.filter((k) => k !== hit.key);
        } else {
          selectedKeys.push(hit.key);
        }
      } else if (!selectedKeys.includes(hit.key)) {
        selectedKeys = [hit.key];
      }
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      mode = 'MOVE';
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      dragStart = p;
      dragOrigins = selectedMaps().map((m) => ({ key: m.key, x: m.x, y: m.y }));
      syncInspector();
      refreshBase();
      return;
    }

    if (selectedKeys.length > 0 || selectedKey) {
      selectedKey = null;
      selectedKeys = [];
      syncInspector();
      refreshBase();
    }'''

new_pm_pointerdown = '''    // currentTool === 'pointer'
    if (editor()?.hitTestHandle?.(event)) {
      return;
    }

    // If clicking inside an active selection (multi-selection containing shapes and/or pixelmaps),
    // allow editor (app.js) to handle unified dragging without clearing selection
    if (editor()?.isPointInsideSelection?.(p)) {
      return;
    }

    const hit = hitMap(p);
    if (hit) {
      if (!event.shiftKey && !event.ctrlKey) {
        clearFieldSelection();
      }
      if (event.shiftKey || event.ctrlKey) {
        if (selectedKeys.includes(hit.key)) {
          selectedKeys = selectedKeys.filter((k) => k !== hit.key);
        } else {
          selectedKeys.push(hit.key);
        }
      } else if (!selectedKeys.includes(hit.key)) {
        selectedKeys = [hit.key];
      }
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      mode = 'MOVE';
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      dragStart = p;
      dragOrigins = selectedMaps().map((m) => ({ key: m.key, x: m.x, y: m.y }));
      syncInspector();
      refreshBase();
      return;
    }

    if (!event.shiftKey && !event.ctrlKey) {
      if (selectedKeys.length > 0 || selectedKey) {
        selectedKey = null;
        selectedKeys = [];
        syncInspector();
        refreshBase();
      }
    }'''
assert old_pm_pointerdown in pm_code, "old_pm_pointerdown not found in designer-pixelmap.js"
pm_code = pm_code.replace(old_pm_pointerdown, new_pm_pointerdown, 1)

# 2.2 Export syncInspector and notifyChanged
old_pm_export = '''    getSelectedKeys: () => (selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : [])),'''
new_pm_export = '''    getSelectedKeys: () => (selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : [])),
    syncInspector: () => syncInspector(),
    notifyChanged: () => changed(),'''
assert old_pm_export in pm_code, "old_pm_export not found in designer-pixelmap.js"
pm_code = pm_code.replace(old_pm_export, new_pm_export, 1)

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'w', encoding='utf-8', newline='') as f:
    f.write(pm_code)
print("designer-pixelmap.js patched successfully!")

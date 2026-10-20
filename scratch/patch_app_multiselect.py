with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. Update CAD marquee in handlePointerUp
old_marquee_pm = '''          // Check pixelmaps for CAD Marquee
          let matchedPixelMap = null;
          const allPixelMaps = window.JWPLCHMIPixelMaps?.getAll?.() || [];
          for (let i = allPixelMaps.length - 1; i >= 0; i--) {
            const pm = allPixelMaps[i];
            if (Number(pm.page || 0) !== activePage || pm.editorVisible === false || !pm.pixels?.length) continue;
            if (isLeftToRight) {
              // Blue Window: Must be completely enclosed in marquee box
              let pMinX = Infinity, pMinY = Infinity, pMaxX = -Infinity, pMaxY = -Infinity;
              pm.pixels.forEach((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                if (px < pMinX) pMinX = px;
                if (px > pMaxX) pMaxX = px;
                if (py < pMinY) pMinY = py;
                if (py > pMaxY) pMaxY = py;
              });
              if (pMinX !== Infinity && pMinX >= boxL && pMaxX <= boxR && pMinY >= boxT && pMaxY <= boxB) {
                matchedPixelMap = pm;
                break;
              }
            } else {
              // Green Crossing ("modo verde"): Touches ANY drawn pixel
              const touchesPixel = pm.pixels.some((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                return px >= boxL && px <= boxR && py >= boxT && py <= boxB;
              });
              if (touchesPixel) {
                matchedPixelMap = pm;
                break;
              }
            }
          }

          if (matchedList.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            const primary = selectedField();
            selectedTool = primary ? toolForField(primary) : 'pointer';
            window.JWPLCHMIPixelMaps?.setSelectedKey?.(null);
            syncInputsFromState();
            syncToolUI();
          } else if (matchedPixelMap) {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKey?.(matchedPixelMap.key);
            selectedTool = 'pointer';
            syncToolUI();
          } else {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKey?.(null);
            syncToolUI();
          }'''

new_marquee_pm = '''          // Check pixelmaps for CAD Marquee (supports multiple pixel objects)
          const matchedPixelMaps = [];
          const allPixelMaps = window.JWPLCHMIPixelMaps?.getAll?.() || [];
          for (let i = 0; i < allPixelMaps.length; i++) {
            const pm = allPixelMaps[i];
            if (Number(pm.page || 0) !== activePage || pm.editorVisible === false || !pm.pixels?.length) continue;
            if (isLeftToRight) {
              // Blue Window: Must be completely enclosed in marquee box
              let pMinX = Infinity, pMinY = Infinity, pMaxX = -Infinity, pMaxY = -Infinity;
              pm.pixels.forEach((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                if (px < pMinX) pMinX = px;
                if (px > pMaxX) pMaxX = px;
                if (py < pMinY) pMinY = py;
                if (py > pMaxY) pMaxY = py;
              });
              if (pMinX !== Infinity && pMinX >= boxL && pMaxX <= boxR && pMinY >= boxT && pMaxY <= boxB) {
                matchedPixelMaps.push(pm);
              }
            } else {
              // Green Crossing ("modo verde"): Touches ANY drawn pixel
              const touchesPixel = pm.pixels.some((p) => {
                const px = pm.x + p.x;
                const py = pm.y + p.y;
                return px >= boxL && px <= boxR && py >= boxT && py <= boxB;
              });
              if (touchesPixel) {
                matchedPixelMaps.push(pm);
              }
            }
          }

          if (matchedList.length > 0 && matchedPixelMaps.length === 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            const primary = selectedField();
            selectedTool = primary ? toolForField(primary) : 'pointer';
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
            syncInputsFromState();
            syncToolUI();
          } else if (matchedPixelMaps.length > 0 && matchedList.length === 0) {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key));
            selectedTool = 'pointer';
            syncToolUI();
          } else if (matchedList.length > 0 && matchedPixelMaps.length > 0) {
            setSelectedKeys(matchedList.map((f) => f.key));
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.(matchedPixelMaps.map((m) => m.key));
            selectedTool = 'pointer';
            syncInputsFromState();
            syncToolUI();
          } else {
            setSelectedKeys([]);
            window.JWPLCHMIPixelMaps?.setSelectedKeys?.([]);
            syncToolUI();
          }'''

assert old_marquee_pm in c, "old_marquee_pm not found in app.js"
c = c.replace(old_marquee_pm, new_marquee_pm)

# 2. Add pixel map bounds helpers right before drawSelectionAndGuides
helpers = '''  function getPixelMapBounds(pm) {
    if (!pm || !pm.pixels?.length) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    pm.pixels.forEach((p) => {
      const px = pm.x + p.x;
      const py = pm.y + p.y;
      if (px < minX) minX = px;
      if (px > maxX) maxX = px;
      if (py < minY) minY = py;
      if (py > maxY) maxY = py;
    });
    if (minX === Infinity) return null;
    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom;
    const screenY2 = (maxY + 1) * zoom;
    return {
      minX, minY, maxX, maxY,
      screenX1, screenY1, screenX2, screenY2,
      screenW: Math.max(8, screenX2 - screenX1),
      screenH: Math.max(8, screenY2 - screenY1)
    };
  }

  function getPixelMapsUnionBounds(pms) {
    if (!pms || pms.length === 0) return null;
    let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
    pms.forEach((pm) => {
      const b = getPixelMapBounds(pm);
      if (b) {
        if (b.minX < minX) minX = b.minX;
        if (b.maxX > maxX) maxX = b.maxX;
        if (b.minY < minY) minY = b.minY;
        if (b.maxY > maxY) maxY = b.maxY;
      }
    });
    if (minX === Infinity) return null;
    const screenX1 = minX * zoom;
    const screenY1 = minY * zoom;
    const screenX2 = (maxX + 1) * zoom;
    const screenY2 = (maxY + 1) * zoom;
    return {
      minX, minY, maxX, maxY,
      screenX1, screenY1, screenX2, screenY2,
      screenW: Math.max(8, screenX2 - screenX1),
      screenH: Math.max(8, screenY2 - screenY1)
    };
  }

'''

c = c.replace('  function drawSelectionAndGuides() {', helpers + '  function drawSelectionAndGuides() {')

# 3. Update drawSelectionAndGuides to support multiple pixel maps
old_draw_sel = '''    const fields = selectedFields();
    if (fields.length === 0) {
      const selPm = window.JWPLCHMIPixelMaps?.getSelected?.();
      if (selPm && selectedTool === 'pointer' && Number(selPm.page || 0) === activePage && selPm.editorVisible !== false && selPm.pixels?.length) {
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        selPm.pixels.forEach((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          if (px < minX) minX = px;
          if (px > maxX) maxX = px;
          if (py < minY) minY = py;
          if (py > maxY) maxY = py;
        });
        if (minX !== Infinity) {
          const sx1 = minX * zoom;
          const sy1 = minY * zoom;
          const sx2 = (maxX + 1) * zoom;
          const sy2 = (maxY + 1) * zoom;
          const sw = Math.max(8, sx2 - sx1);
          const sh = Math.max(8, sy2 - sy1);

          displayCtx.save();
          // 1. Dotted projection lines to rulers
          displayCtx.strokeStyle = 'rgba(255, 255, 255, 0.40)';
          displayCtx.setLineDash([2, 3]);
          displayCtx.lineWidth = 1;
          displayCtx.beginPath();
          displayCtx.moveTo(sx1 + 0.5, 0);
          displayCtx.lineTo(sx1 + 0.5, displayCanvas.height);
          displayCtx.moveTo(sx2 + 0.5, 0);
          displayCtx.lineTo(sx2 + 0.5, displayCanvas.height);
          displayCtx.moveTo(0, sy1 + 0.5);
          displayCtx.lineTo(displayCanvas.width, sy1 + 0.5);
          displayCtx.moveTo(0, sy2 + 0.5);
          displayCtx.lineTo(displayCanvas.width, sy2 + 0.5);
          displayCtx.stroke();
          displayCtx.setLineDash([]);

          // 2. Bounding outline (#0084ff)
          displayCtx.strokeStyle = '#0084ff';
          displayCtx.lineWidth = 1;
          displayCtx.strokeRect(sx1 + 0.5, sy1 + 0.5, sw, sh);

          // 3. 8 Selection Handles
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
          displayCtx.restore();
        }
      }
      return;
    }'''

new_draw_sel = '''    const fields = selectedFields();
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
    }'''

assert old_draw_sel in c, "old_draw_sel not found in app.js"
c = c.replace(old_draw_sel, new_draw_sel)

# 4. Update getSelectionHandles to support multiple pixel maps
old_handles = '''function getSelectionHandles(fields) {
    if (!fields || fields.length === 0) {
      const selPm = window.JWPLCHMIPixelMaps?.getSelected?.();
      if (selPm && selectedTool === 'pointer' && Number(selPm.page || 0) === activePage && selPm.editorVisible !== false && selPm.pixels?.length) {
        let minX = Infinity, minY = Infinity, maxX = -Infinity, maxY = -Infinity;
        selPm.pixels.forEach((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          if (px < minX) minX = px;
          if (px > maxX) maxX = px;
          if (py < minY) minY = py;
          if (py > maxY) maxY = py;
        });
        if (minX !== Infinity) {
          const sx1 = minX * zoom;
          const sy1 = minY * zoom;
          const sx2 = (maxX + 1) * zoom;
          const sy2 = (maxY + 1) * zoom;
          const midX = (sx1 + sx2) / 2;
          const midY = (sy1 + sy2) / 2;
          return [
            { id: 'tl', x: sx1, y: sy1, cursor: 'nwse-resize' },
            { id: 'tc', x: midX, y: sy1, cursor: 'ns-resize' },
            { id: 'tr', x: sx2, y: sy1, cursor: 'nesw-resize' },
            { id: 'rc', x: sx2, y: midY, cursor: 'ew-resize' },
            { id: 'br', x: sx2, y: sy2, cursor: 'nwse-resize' },
            { id: 'bc', x: midX, y: sy2, cursor: 'ns-resize' },
            { id: 'bl', x: sx1, y: sy2, cursor: 'nesw-resize' },
            { id: 'lc', x: sx1, y: midY, cursor: 'ew-resize' }
          ];
        }
      }
      return [];
    }'''

new_handles = '''function getSelectionHandles(fields) {
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
    }'''

assert old_handles in c, "old_handles not found in app.js"
c = c.replace(old_handles, new_handles)

# 5. Update isPointInsideSelection
old_inside = '''  function isPointInsideSelection(point) {
    const fields = selectedFields();
    if (fields.length === 0) {
      const selPm = window.JWPLCHMIPixelMaps?.getSelected?.();
      if (selPm && selectedTool === 'pointer' && Number(selPm.page || 0) === activePage && selPm.editorVisible !== false && selPm.pixels?.length) {
        return selPm.pixels.some((p) => {
          const px = selPm.x + p.x;
          const py = selPm.y + p.y;
          return Math.abs(px - point.x) <= 2 && Math.abs(py - point.y) <= 2;
        });
      }
      return false;
    }'''

new_inside = '''  function isPointInsideSelection(point) {
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
    }'''

assert old_inside in c, "old_inside not found in app.js"
c = c.replace(old_inside, new_inside)

with open('tools/jwplc-hmi-designer/poc/app.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Successfully updated app.js")

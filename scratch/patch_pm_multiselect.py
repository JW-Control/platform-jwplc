with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. State variable selectedKeys and dragOrigins
c = c.replace(
'''  let maps = [];
  let selectedKey = null;
  let serial = 0;
  let mode = 'DRAW';
  let color = 0xFD20;
  let brushSize = 1;
  let eraserSize = 1;
  let pointer = null;
  let lastPoint = null;
  let dragStart = null;
  let dragOrigin = null;''',
'''  let maps = [];
  let selectedKeys = [];
  let selectedKey = null;
  let serial = 0;
  let mode = 'DRAW';
  let color = 0xFD20;
  let brushSize = 1;
  let eraserSize = 1;
  let pointer = null;
  let lastPoint = null;
  let dragStart = null;
  let dragOrigin = null;
  let dragOrigins = [];'''
)

# 2. selected() and selectedMaps()
c = c.replace(
'''  function selected() {
    return maps.find((map) => map.key === selectedKey) || null;
  }''',
'''  function selected() {
    return maps.find((map) => map.key === selectedKey || selectedKeys.includes(map.key)) || null;
  }

  function selectedMaps() {
    return maps.filter((map) => selectedKeys.includes(map.key) || map.key === selectedKey);
  }'''
)

# 3. createMap & duplicateMap
c = c.replace(
'''    maps.push(map);
    selectedKey = map.key;
    mode = 'DRAW';''',
'''    maps.push(map);
    selectedKey = map.key;
    selectedKeys = [map.key];
    mode = 'DRAW';'''
)

c = c.replace(
'''    maps.push(copy);
    selectedKey = copy.key;
    refreshBase();''',
'''    maps.push(copy);
    selectedKey = copy.key;
    selectedKeys = [copy.key];
    refreshBase();'''
)

# 4. deleteMap
c = c.replace(
'''  function deleteMap() {
    const map = selected();
    if (!map) return;
    maps = maps.filter((item) => item.key !== map.key);
    selectedKey = null;
    refreshBase();
    changed();
  }''',
'''  function deleteMap() {
    const toDelete = selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : []);
    if (toDelete.length === 0) return;
    maps = maps.filter((item) => !toDelete.includes(item.key));
    selectedKey = null;
    selectedKeys = [];
    refreshBase();
    changed();
  }'''
)

# 5. patchObjectList row and click
c = c.replace(
'''      const isAct = map.key === selectedKey;
      row.className = `object-item a11-pixel-row${isAct ? \' active\' : \'\'}${!map.editorVisible ? \' is-hidden\' : \'\'}${map.locked ? \' is-locked\' : \'\'}`;''',
'''      const isAct = selectedKeys.includes(map.key) || map.key === selectedKey;
      row.className = `object-item a11-pixel-row${isAct ? \' active\' : \'\'}${!map.editorVisible ? \' is-hidden\' : \'\'}${map.locked ? \' is-locked\' : \'\'}`;'''
)

old_row_clicks = '''      row.addEventListener('click', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        clearFieldSelection();
        selectedKey = map.key;
        mode = 'MOVE';
        syncInspector();
        render();
        window.JWPLCHMIInspectorState?.sync?.();
      });

      row.addEventListener('dblclick', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        clearFieldSelection();
        selectedKey = map.key;
        mode = 'MOVE';
        syncInspector();
        render();
        window.JWPLCHMIEditor?.fitSelection?.();
      });'''

new_row_clicks = '''      row.addEventListener('click', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        clearFieldSelection();
        if (event.shiftKey || event.ctrlKey) {
          if (selectedKeys.includes(map.key)) {
            selectedKeys = selectedKeys.filter((k) => k !== map.key);
          } else {
            selectedKeys.push(map.key);
          }
        } else {
          selectedKeys = [map.key];
        }
        selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
        mode = 'MOVE';
        const pointerBtn = document.querySelector('.tool[data-tool="pointer"]');
        if (pointerBtn) pointerBtn.click();
        else window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
        syncInspector();
        refreshBase();
        window.JWPLCHMIInspectorState?.sync?.();
      });

      row.addEventListener('dblclick', (event) => {
        if (event.target.closest('.object-btn')) return;
        event.preventDefault();
        clearFieldSelection();
        selectedKeys = [map.key];
        selectedKey = map.key;
        mode = 'MOVE';
        const pointerBtn = document.querySelector('.tool[data-tool="pointer"]');
        if (pointerBtn) pointerBtn.click();
        else window.dispatchEvent(new CustomEvent('jwplc:tool-changed', { detail: { tool: 'pointer' } }));
        syncInspector();
        refreshBase();
        window.JWPLCHMIEditor?.fitSelection?.();
      });'''

assert old_row_clicks in c, "old_row_clicks not found"
c = c.replace(old_row_clicks, new_row_clicks)

# 6. Canvas pointerdown and hit
old_hit = '''    const hit = hitMap(p);
    if (hit) {
      clearFieldSelection();
      selectedKey = hit.key;
      mode = 'MOVE';
      event.preventDefault();
      event.stopImmediatePropagation();
      pointer = event.pointerId;
      lastPoint = p;
      try { displayCanvas.setPointerCapture(pointer); } catch (_) {}
      dragStart = p;
      dragOrigin = { x: hit.x, y: hit.y };
      syncInspector();
      refreshBase();
      return;
    }

    if (selectedKey) {
      selectedKey = null;
      syncInspector();
      refreshBase();
    }'''

new_hit = '''    const hit = hitMap(p);
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

assert old_hit in c, "old_hit not found"
c = c.replace(old_hit, new_hit)

# 7. Drag in pointermove
old_pm_move = '''    if (mode === 'MOVE' && dragStart && dragOrigin) {
      const dx = p.x - dragStart.x;
      const dy = p.y - dragStart.y;
      let nextX = dragOrigin.x + dx;
      let nextY = dragOrigin.y + dy;
      
      const snapToggle = document.getElementById('snapToggle');
      if (snapToggle && snapToggle.checked) {
        const grid = Number(document.getElementById('gridSizeSelect')?.value) || 8;
        nextX = Math.round(nextX / grid) * grid;
        nextY = Math.round(nextY / grid) * grid;
      }
      
      nextX = Math.max(0, Math.min(WIDTH - bounds(map).width, nextX));
      nextY = Math.max(0, Math.min(HEIGHT - bounds(map).height, nextY));
      if (map.x !== nextX || map.y !== nextY) {
        map.x = nextX;
        map.y = nextY;
        syncInspector();
        refreshBase();
      }
    }'''

new_pm_move = '''    if (mode === 'MOVE' && dragStart && dragOrigins && dragOrigins.length > 0) {
      const dx = p.x - dragStart.x;
      const dy = p.y - dragStart.y;
      const snapToggle = document.getElementById('snapToggle');
      const grid = Number(document.getElementById('gridSizeSelect')?.value) || 8;
      dragOrigins.forEach((orig) => {
        const m = maps.find((x) => x.key === orig.key);
        if (m) {
          let nextX = orig.x + dx;
          let nextY = orig.y + dy;
          if (snapToggle && snapToggle.checked) {
            nextX = Math.round(nextX / grid) * grid;
            nextY = Math.round(nextY / grid) * grid;
          }
          nextX = Math.max(0, Math.min(WIDTH - bounds(m).width, nextX));
          nextY = Math.max(0, Math.min(HEIGHT - bounds(m).height, nextY));
          m.x = nextX;
          m.y = nextY;
        }
      });
      syncInspector();
      refreshBase();
    }'''

assert old_pm_move in c, "old_pm_move not found"
c = c.replace(old_pm_move, new_pm_move)

# 8. Escape handler
old_esc = '''      if (currentTool === 'pixel' || currentTool === 'erase' || selectedKey) {
        event.preventDefault();
        event.stopImmediatePropagation();
        selectedKey = null;
        clearFieldSelection();'''

new_esc = '''      if (currentTool === 'pixel' || currentTool === 'erase' || selectedKey || selectedKeys.length > 0) {
        event.preventDefault();
        event.stopImmediatePropagation();
        selectedKey = null;
        selectedKeys = [];
        clearFieldSelection();'''

assert old_esc in c, "old_esc not found"
c = c.replace(old_esc, new_esc)

# 9. Window export
old_export = '''    hitTest: (point) => hitMap(point),
    setSelectedKey: (key) => {
      if (key) clearFieldSelection();
      selectedKey = key;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    getBounds: (map) => bounds(map)'''

new_export = '''    hitTest: (point) => hitMap(point),
    getSelectedMaps: selectedMaps,
    getSelectedKeys: () => (selectedKeys.length > 0 ? selectedKeys : (selectedKey ? [selectedKey] : [])),
    setSelectedKey: (key) => {
      if (key) clearFieldSelection();
      selectedKeys = key ? [key] : [];
      selectedKey = key;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    setSelectedKeys: (keys) => {
      selectedKeys = Array.isArray(keys) ? [...keys] : (keys ? [keys] : []);
      if (selectedKeys.length > 0) clearFieldSelection();
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    getBounds: (map) => bounds(map)'''

assert old_export in c, "old_export not found"
c = c.replace(old_export, new_export)

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Successfully updated designer-pixelmap.js")

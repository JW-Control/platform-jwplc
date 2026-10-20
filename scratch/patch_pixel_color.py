with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. Update setPixelColor to recolor existing pixels on selected map
old_set_pixel = '''  function setPixelColor(v565, commit = true) {
    color = v565 & 0xFFFF;
    updatePixelSwatch(color);
    if (commit) {
      changed();
    }
  }'''

new_set_pixel = '''  function setPixelColor(v565, commit = true) {
    color = v565 & 0xFFFF;
    updatePixelSwatch(color);
    const map = selected();
    if (map && Array.isArray(map.pixels) && map.pixels.length > 0) {
      map.pixels.forEach((p) => {
        p.color = color;
      });
      render();
      refreshBase();
    }
    if (commit) {
      changed();
    }
  }'''

assert old_set_pixel in c, "old_set_pixel not found"
c = c.replace(old_set_pixel, new_set_pixel)

# 2. Update pointerdown drawColor
old_pd = "const drawColor = (editor()?.getSelectedColor?.() ?? color) & 0xFFFF;"
# It appears twice: in pointerdown and pointermove
count = c.count(old_pd)
print(f"Found {count} occurrences of old drawColor")
assert count == 2, f"Expected 2 occurrences, found {count}"
c = c.replace(old_pd, "const drawColor = color & 0xFFFF;")

# 3. Update syncInspector to restore color from selected map if it has pixels
old_sync_insp = '''    ensureEditorMeta(map);
    const b = bounds(map);
    const displayName = map.name || 'PIXEL';'''

new_sync_insp = '''    ensureEditorMeta(map);
    if (map.pixels && map.pixels.length > 0) {
      color = map.pixels[map.pixels.length - 1].color;
      syncMainColor();
    }
    const b = bounds(map);
    const displayName = map.name || 'PIXEL';'''

assert old_sync_insp in c, "old_sync_insp not found"
c = c.replace(old_sync_insp, new_sync_insp)

# 4. Update setActiveColor to use setPixelColor
old_set_active = '''    setActiveColor: (value) => { color = Number(value) & 0xFFFF; syncMainColor(); syncInspector(); }'''
new_set_active = '''    setActiveColor: (value) => { setPixelColor(Number(value) & 0xFFFF, true); syncMainColor(); syncInspector(); }'''

assert old_set_active in c, "old_set_active not found"
c = c.replace(old_set_active, new_set_active)

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Successfully patched designer-pixelmap.js")

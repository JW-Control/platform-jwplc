with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. Update jwplc:editor-refresh
old_rf = '''  window.addEventListener('jwplc:editor-refresh', () => {
    if (editor()?.hasFieldSelection?.()) selectedKey = null;
    if (selected() && Number(selected().page || 0) !== page()) selectedKey = null;
    requestAnimationFrame(() => { render(); syncTools(); });
  });'''

new_rf = '''  window.addEventListener('jwplc:editor-refresh', () => {
    if (selected() && Number(selected().page || 0) !== page()) {
      selectedKey = null;
      selectedKeys = [];
    }
    requestAnimationFrame(() => { render(); syncTools(); });
  });'''

assert old_rf in c, "old_rf not found"
c = c.replace(old_rf, new_rf)

# 2. Update setSelectedKeys clearFields parameter
old_set_keys = '''    setSelectedKey: (key) => {
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
    },'''

new_set_keys = '''    setSelectedKey: (key, clearFields = true) => {
      if (clearFields && key) clearFieldSelection();
      selectedKeys = key ? [key] : [];
      selectedKey = key;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },
    setSelectedKeys: (keys, clearFields = true) => {
      if (clearFields && keys && keys.length > 0) clearFieldSelection();
      selectedKeys = Array.isArray(keys) ? [...keys] : (keys ? [keys] : []);
      selectedKey = selectedKeys.length > 0 ? selectedKeys[selectedKeys.length - 1] : null;
      syncInspector();
      refreshBase();
      window.JWPLCHMIInspectorState?.sync?.();
    },'''

assert old_set_keys in c, "old_set_keys not found"
c = c.replace(old_set_keys, new_set_keys)

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Updated designer-pixelmap.js for combined selection")

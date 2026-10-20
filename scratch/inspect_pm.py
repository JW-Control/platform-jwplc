with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    c = f.read()

idx = c.find("window.addEventListener('jwplc:editor-refresh'")
print(c[idx:idx+400])

with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', encoding='utf-8') as f:
    code = f.read()

idx = code.find('window.addEventListener(\'pointermove\'')
print(code[idx+800:idx+1500])

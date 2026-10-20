with open('tools/jwplc-hmi-designer/poc/app.js', encoding='utf-8') as f:
    code = f.read()

idx = code.find('getSelectedColor')
print(code[idx-50:idx+300])

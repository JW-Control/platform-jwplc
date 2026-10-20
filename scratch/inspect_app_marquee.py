with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

idx = c.find('function isPointInsideSelection(point) {')
print(c[idx:idx+800])

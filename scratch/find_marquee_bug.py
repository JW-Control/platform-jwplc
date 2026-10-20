with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

idx = c.find('if (matchedList.length > 0 && matchedPixelMaps.length === 0)')
print(c[idx:idx+1200])

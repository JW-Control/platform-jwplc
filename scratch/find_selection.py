with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', encoding='utf-8') as f:
    lines = f.readlines()

for i, l in enumerate(lines[:60]):
    if 'selectedKey' in l or 'maps' in l:
        print(f"{i+1}: {l.strip()}")

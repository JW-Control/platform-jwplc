with open('tools/jwplc-hmi-designer/poc/designer-pixelmap.js', 'r', encoding='utf-8') as f:
    c = f.read()

import re
for m in re.finditer(r'patchObjectList', c):
    idx = m.start()
    print(c[max(0, idx-40):min(len(c), idx+100)])
    print('---')

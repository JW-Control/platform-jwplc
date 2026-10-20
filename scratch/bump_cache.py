with open('tools/jwplc-hmi-designer/poc/service-worker.js', 'r', encoding='utf-8') as f:
    c = f.read()

c = c.replace('jwplc-hmi-designer-a15-sketch-v31', 'jwplc-hmi-designer-a15-sketch-v32')
with open('tools/jwplc-hmi-designer/poc/service-worker.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

with open('tools/jwplc-hmi-designer/poc/designer-field-visibility.js', 'r', encoding='utf-8') as f:
    c = f.read()
c = c.replace('?v=1793800700', '?v=1793800800')
with open('tools/jwplc-hmi-designer/poc/designer-field-visibility.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

with open('tools/jwplc-hmi-designer/poc/desktop.html', 'r', encoding='utf-8') as f:
    c = f.read()
c = c.replace('?v=1793800700', '?v=1793800800')
with open('tools/jwplc-hmi-designer/poc/desktop.html', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Bumped cache to v32 / 1793800800")

with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

old_exp = '    hasFieldSelection: () => selectedFields().length > 0,'
new_exp = '''    hasFieldSelection: () => selectedFields().length > 0,
    getSelectionHandles: (fields = selectedFields()) => getSelectionHandles(fields),
    hitTestHandle,'''

assert old_exp in c, "old_exp not found"
c = c.replace(old_exp, new_exp)

with open('tools/jwplc-hmi-designer/poc/app.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Exported getSelectionHandles and hitTestHandle")

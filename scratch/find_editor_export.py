with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

idx = c.find('window.JWPLCHMIEditor = {')
print(c[idx:idx+1500])

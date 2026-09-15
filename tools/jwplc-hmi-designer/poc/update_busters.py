import re
import time

def buster(match):
    src = match.group(1)
    if '?' in src:
        src = src.split('?')[0]
    return match.group(0).replace(match.group(1), f"{src}?v={int(time.time())}")

with open('desktop.html', 'r', encoding='utf-8') as f:
    desktop = f.read()
desktop = re.sub(r'<script src="([^"]+)"></script>', buster, desktop)
desktop = re.sub(r'<link rel="stylesheet" href="([^"]+)" />', buster, desktop)
desktop = re.sub(r'\'<script src="\./(designer-[^"]+\.js)"\>\' \+ closeScript \+', lambda m: f"'<script src=\"./{m.group(1)}?v={int(time.time())}\">' + closeScript +", desktop)
with open('desktop.html', 'w', encoding='utf-8') as f:
    f.write(desktop)

with open('index.html', 'r', encoding='utf-8') as f:
    html = f.read()
html = re.sub(r'<script src="([^"]+)"></script>', buster, html)
html = re.sub(r'<link rel="stylesheet" href="([^"]+)" />', buster, html)
with open('index.html', 'w', encoding='utf-8') as f:
    f.write(html)
print("Cache busters updated!")

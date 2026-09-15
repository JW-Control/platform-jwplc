import re

with open('desktop.html', 'r', encoding='utf-8') as f:
    desktop = f.read()

# The error handler was injected incorrectly inside a javascript string replace.
# Let's fix it by making the javascript string single-line or backticks.
# Actually, I can just remove it and re-inject it properly using backticks for the js string.

# Instead of regex, I'll just replace the multiline broken string with a single-line or backticks.
# Since the injected code has literal newlines, it broke JS.

# Let's use regex to find the replace block and use backticks instead of quotes.
desktop = desktop.replace("'\\n<script>\\n", "`\\n<script>\\n")
desktop = desktop.replace("});\\n</script>\\n' + '<script src=\"./designer-project", "});\\n</script>\\n` + '<script src=\"./designer-project")

# Wait, let's just restore the file completely to the previous commit (e4583892) and then re-apply the cache buster correctly.

with open('tools/jwplc-hmi-designer/poc/app.js', 'r', encoding='utf-8') as f:
    c = f.read()

# 1. drawShape frameColor nullish coalescing
old_draw_color = "const color = field.frameColor || 0xFFFF;"
assert old_draw_color in c, "old_draw_color not found in app.js"
c = c.replace(old_draw_color, "const color = field.frameColor ?? 0xFFFF;")

# 2. buildPalette sync with PixelMaps
old_bp = '''          syncInputsFromState();
        }
        updateActiveColorUI();'''
new_bp = '''          syncInputsFromState();
        } else if (window.JWPLCHMIPixelMaps?.getSelected?.()) {
          window.JWPLCHMIPixelMaps.setActiveColor(color.value);
        }
        updateActiveColorUI();'''
assert old_bp in c, "old_bp not found in app.js"
c = c.replace(old_bp, new_bp)

# 3. pickColorAt sync with PixelMaps
old_pca = '''      } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
        field.valueColor = selectedColor.value;
      }
      syncInputsFromState();
    }
    return true;'''
new_pca = '''      } else if (['TEXT', 'VALUE', 'BOOL', 'BAR'].includes(field.type)) {
        field.valueColor = selectedColor.value;
      }
      syncInputsFromState();
    } else if (window.JWPLCHMIPixelMaps?.getSelected?.()) {
      window.JWPLCHMIPixelMaps.setActiveColor(selectedColor.value);
    }
    return true;'''
assert old_pca in c, "old_pca not found in app.js"
c = c.replace(old_pca, new_pca)

with open('tools/jwplc-hmi-designer/poc/app.js', 'w', encoding='utf-8', newline='') as f:
    f.write(c)

print("Successfully patched app.js")

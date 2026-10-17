// Símbolos que define una librería JWPLC, según cómo se compile en el package:
//   - precompiled=full -> funciones definidas en src/esp32/lib<Nombre>.a (nm -C);
//   - modo fuente (sin precompiled, p. ej. JWPLC_Display durante el alpha) ->
//     definiciones `Clase::metodo(` y funciones libres de src/*.cpp. El .a se
//     regenera recién al cierre del alpha y no participa del build.
'use strict';

const fs = require('fs');
const path = require('path');
const { execFileSync } = require('child_process');

function isPrecompiled(libDir) {
  const props = path.join(libDir, 'library.properties');
  return fs.existsSync(props) && /^precompiled=full\s*$/m.test(fs.readFileSync(props, 'utf8'));
}

function librarySymbols(libDir) {
  const name = path.basename(libDir);
  const symbols = new Set();
  if (isPrecompiled(libDir)) {
    const out = execFileSync('nm', ['-C', path.join(libDir, 'src', 'esp32', `lib${name}.a`)]).toString();
    out.split('\n').filter((l) => / T /.test(l)).forEach((l) => symbols.add(l.replace(/.* T /, '').replace(/\(.*/, '')));
    return { mode: 'precompiled', symbols };
  }
  const src = path.join(libDir, 'src');
  for (const file of fs.readdirSync(src).filter((f) => f.endsWith('.cpp'))) {
    const text = fs.readFileSync(path.join(src, file), 'utf8');
    for (const m of text.matchAll(/^[\w:<>*&\s]*?\b([A-Za-z_]\w*::[A-Za-z_]\w*)\s*\(/gm)) symbols.add(m[1]);
    for (const m of text.matchAll(/^(?:[\w:<>*&]+\s+)+([A-Za-z_]\w*)\s*\([^;]*$/gm)) symbols.add(m[1]);
  }
  return { mode: 'source', symbols };
}

module.exports = { isPrecompiled, librarySymbols };

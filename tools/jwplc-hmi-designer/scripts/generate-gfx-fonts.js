// Genera poc/designer-fonts-data.js a partir de las fuentes de TFT_eSPI:
//  - FreeFonts GFXfont (Fonts/GFXFF/*.h)  -> window.JWPLCGfxFontData
//  - Font 2 (Font16.c) y Font 4 (Font32rle.c) -> window.JWPLCTftBitmapFontData
// Uso: node tools/jwplc-hmi-designer/scripts/generate-gfx-fonts.js [carpeta_Fonts_de_TFT_eSPI]
// Sin argumento descarga de https://github.com/Bodmer/TFT_eSPI (rama master).
'use strict';

const fs = require('fs');
const path = require('path');

const FAMILIES = { SANS: 'FreeSans', SERIF: 'FreeSerif', MONO: 'FreeMono' };
const BASE = 'https://raw.githubusercontent.com/Bodmer/TFT_eSPI/master/Fonts/';
const OUT = path.join(__dirname, '..', 'poc', 'designer-fonts-data.js');

// dir = carpeta `Fonts` de TFT_eSPI (con subcarpeta GFXFF). Sin dir se descarga.
async function readSource(relativePath, dir) {
  if (dir) return fs.readFileSync(path.join(dir, relativePath), 'utf8');
  const response = await fetch(`${BASE}${relativePath}`);
  if (!response.ok) throw new Error(`No se pudo descargar ${relativePath} (${response.status})`);
  return response.text();
}

function parseGfxHeader(name, source) {
  const bitmapMatch = source.match(/Bitmaps\[\]\s*PROGMEM\s*=\s*\{([\s\S]*?)\};/);
  const glyphMatch = source.match(/Glyphs\[\]\s*PROGMEM\s*=\s*\{([\s\S]*?)\};/);
  const fontMatch = source.match(/GFXfont\s+\w+\s+PROGMEM\s*=\s*\{[\s\S]*?0x([0-9A-Fa-f]+)\s*,\s*0x([0-9A-Fa-f]+)\s*,\s*(\d+)\s*\}/);
  if (!bitmapMatch || !glyphMatch || !fontMatch) throw new Error(`Formato inesperado en ${name}`);

  const bytes = bitmapMatch[1].replace(/\/\*[\s\S]*?\*\//g, '').match(/0x[0-9A-Fa-f]{2}/g).map((v) => parseInt(v, 16));
  const glyphs = [...glyphMatch[1].replace(/\/\/.*$/gm, '').matchAll(/\{\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*,\s*(-?\d+)\s*\}/g)]
    .map((m) => m.slice(1, 7).map(Number));

  const first = parseInt(fontMatch[1], 16);
  const last = parseInt(fontMatch[2], 16);
  if (glyphs.length !== last - first + 1) throw new Error(`${name}: ${glyphs.length} glifos para rango ${first}-${last}`);
  return { first, last, yAdvance: Number(fontMatch[3]), glyphs, bitmap: Buffer.from(bytes).toString('base64') };
}

// Preprocesa #define/#ifdef/#ifndef/#else/#endif con un conjunto de defines activo.
function preprocess(source, defined) {
  const out = [];
  const stack = [];
  for (const line of source.split(/\r?\n/)) {
    const directive = line.match(/^\s*#\s*(\w+)\s*(\w*)/);
    if (directive) {
      const [, name, arg] = directive;
      if (name === 'ifdef') { stack.push(defined.has(arg)); continue; }
      if (name === 'ifndef') { stack.push(!defined.has(arg)); continue; }
      if (name === 'else') { stack.push(!stack.pop()); continue; }
      if (name === 'endif') { stack.pop(); continue; }
      if (name === 'define') { if (stack.every(Boolean)) defined.add(arg); continue; }
    }
    if (stack.every(Boolean)) out.push(line);
  }
  return out.join('\n');
}

// Convierte Font16.c (Font 2: una fila por byte(s), MSB izquierda) o Font32rle.c
// (Font 4: RLE de 8 bits) a bitmaps empaquetados MSB-first, w*h bits contiguos
// por glifo, igual que TFT_eSPI::drawChar para fuentes 2..8.
function parseTftBitmapFont(source, prefix, height, rle, defined) {
  const text = preprocess(source, defined).replace(/\/\/.*$/gm, '');
  const widthMatch = text.match(new RegExp(`widtbl_${prefix}\\[96\\]\\s*=\\s*\\{([^}]*)\\}`));
  if (!widthMatch) throw new Error(`No se encontro widtbl_${prefix}`);
  const widths = widthMatch[1].match(/\d+/g).map(Number);
  if (widths.length !== 96) throw new Error(`widtbl_${prefix}: ${widths.length} entradas`);

  const glyphBytes = new Map();
  for (const m of text.matchAll(new RegExp(`chr_${prefix}_([0-9A-Fa-f]{2})\\[\\d*\\]\\s*=\\s*\\{([^}]*)\\}`, 'g'))) {
    glyphBytes.set(parseInt(m[1], 16), (m[2].match(/0x[0-9A-Fa-f]{2}/g) || []).map((v) => parseInt(v, 16)));
  }

  const chunks = [];
  const offsets = [];
  let total = 0;
  for (let index = 0; index < 96; index += 1) {
    const width = widths[index];
    const bytes = glyphBytes.get(32 + index);
    if (!bytes) throw new Error(`Falta glifo ${32 + index} en ${prefix}`);
    const pixels = new Uint8Array(width * height);
    if (rle) {
      let pc = 0;
      for (let i = 0; i < bytes.length && pc < pixels.length; i += 1) {
        const value = bytes[i];
        const run = (value & 0x7F) + 1;
        if (value & 0x80) pixels.fill(1, pc, Math.min(pc + run, pixels.length));
        pc += run;
      }
    } else {
      // TFT_eSPI (Font 2): w = (width + 6) / 8, el ancho de la tabla incluye +1 px.
      const rowBytes = Math.floor((width + 6) / 8);
      if (bytes.length < rowBytes * height) throw new Error(`Glifo ${32 + index} de ${prefix} incompleto`);
      for (let row = 0; row < height; row += 1) {
        for (let col = 0; col < width; col += 1) {
          pixels[row * width + col] = (col >> 3) < rowBytes ? (bytes[row * rowBytes + (col >> 3)] >> (7 - (col & 7))) & 1 : 0;
        }
      }
    }
    const packed = new Uint8Array(Math.ceil(pixels.length / 8));
    pixels.forEach((bit, i) => { if (bit) packed[i >> 3] |= 0x80 >> (i & 7); });
    offsets.push(total);
    chunks.push(Buffer.from(packed));
    total += packed.length;
  }
  return { first: 32, height, widths, offsets, bitmap: Buffer.concat(chunks).toString('base64') };
}

(async () => {
  const dir = process.argv[2];
  const gfx = {};
  for (const [key, base] of Object.entries(FAMILIES)) {
    for (const pt of [9, 12, 18, 24]) {
      for (const bold of [false, true]) {
        const name = `${base}${bold ? 'Bold' : ''}${pt}pt7b`;
        gfx[`${key}_${bold ? 'BOLD_' : ''}${pt}`] = parseGfxHeader(name, await readSource(`GFXFF/${name}.h`, dir));
      }
    }
  }

  // Defaults de TFT_eSPI: Font16.c define TFT_ESPI_FONT2_DOLLAR y
  // TFT_ESPI_GRAVE_IS_DEGREE; FONT_4_GBP queda comentado.
  const bitmaps = {
    FONT2: parseTftBitmapFont(await readSource('Font16.c', dir), 'f16', 16, false, new Set()),
    FONT4: parseTftBitmapFont(await readSource('Font32rle.c', dir), 'f32', 26, true, new Set())
  };

  const header = '// GENERADO por scripts/generate-gfx-fonts.js desde TFT_eSPI Fonts/. No editar a mano.\n';
  fs.writeFileSync(
    OUT,
    `${header}window.JWPLCGfxFontData = ${JSON.stringify(gfx)};\nwindow.JWPLCTftBitmapFontData = ${JSON.stringify(bitmaps)};\n`
  );
  console.log(`OK ${Object.keys(gfx).length} FreeFonts + ${Object.keys(bitmaps).length} bitmap -> ${OUT} (${fs.statSync(OUT).size} bytes)`);
})().catch((error) => { console.error(error); process.exit(1); });

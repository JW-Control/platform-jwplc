#!/usr/bin/env node
// Genera el catálogo de iconos del Designer desde library.cjs (Alpha16).
//
//   node tools/jwplc-hmi-designer/icons/build-catalog.cjs [--check]
//
// Salidas:
//   poc/designer-icon-catalog.js  datos (4 bpp en base64 + paletas RGB565 por estado)
//   icons/catalogo.png            hoja de previsualización (cada icono en cada estado)
// --check: falla si el catálogo del repo no coincide con la fuente (lo usa el test).
'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { createGrid } = require('./raster.cjs');
const { CATEGORIES, ICONS } = require('./library.cjs');

const HERE = __dirname;
const DESIGNER = path.resolve(HERE, '..');
const CATALOG = path.join(DESIGNER, 'poc', 'designer-icon-catalog.js');
const SHEET = path.join(HERE, 'catalogo.png');
const MAX_SIZE = 64;
const MAX_STATES = 8;

// Rasteriza un icono a `size` px: imágenes 4 bpp únicas + paleta por estado (con subframes si es animado).
function buildSize(icon, size) {
  const stateFramesRasters = icon.states.map((stDef, st) => {
    const frameCount = stDef.frameCount || (Array.isArray(stDef.frames) ? stDef.frames.length : 1);
    const frames = [];
    for (let f = 0; f < frameCount; f += 1) {
      const g = createGrid(size);
      icon.draw(g, st, f);
      frames.push(g.roles);
    }
    return frames;
  });

  const roleOrder = [];
  stateFramesRasters.forEach((frames) => {
    frames.forEach((roles) => {
      roles.forEach((role) => {
        if (role !== '.' && !roleOrder.includes(role)) roleOrder.push(role);
      });
    });
  });
  if (roleOrder.length > 15) throw new Error(`${icon.id}@${size}: ${roleOrder.length} roles (máximo 15)`);

  const stride = Math.ceil(size / 2);
  const images = [];
  const states = stateFramesRasters.map((frames, st) => {
    const stDef = icon.states[st];
    const encodedFrames = frames.map((roles, f) => {
      const bytes = Buffer.alloc(stride * size);
      roles.forEach((role, p) => {
        const index = role === '.' ? 0 : roleOrder.indexOf(role) + 1;
        const x = p % size;
        const y = Math.floor(p / size);
        const at = y * stride + (x >> 1);
        bytes[at] |= (x & 1) ? index : index << 4;
      });
      const b64 = bytes.toString('base64');
      let image = images.indexOf(b64);
      if (image < 0) { images.push(b64); image = images.length - 1; }

      const frameColors = (Array.isArray(stDef.frames) && stDef.frames[f]?.colors)
        ? { ...stDef.colors, ...stDef.frames[f].colors }
        : stDef.colors;
      const palette = new Array(16).fill(0);
      roleOrder.forEach((role, k) => {
        if (!(role in frameColors)) throw new Error(`${icon.id}: el estado «${stDef.name}» (frame ${f}) no da color al rol ${role}`);
        palette[k + 1] = frameColors[role] & 0xFFFF;
      });
      return { image, palette };
    });

    const primary = encodedFrames[0];
    const result = { image: primary.image, palette: primary.palette };
    if (encodedFrames.length > 1) {
      result.frameCount = encodedFrames.length;
      result.intervalMs = stDef.intervalMs || 150;
      result.frames = encodedFrames;
    }
    return result;
  });

  return { width: size, height: size, transparent: 0, images, states };
}

function buildCatalog() {
  const ids = new Set();
  const icons = ICONS.map((icon) => {
    if (ids.has(icon.id)) throw new Error(`id repetido: ${icon.id}`);
    ids.add(icon.id);
    if (!CATEGORIES.some((c) => c.id === icon.category)) throw new Error(`${icon.id}: categoría desconocida`);
    if (!icon.id.startsWith(`${icon.category}/`)) throw new Error(`${icon.id}: el id debe empezar por la categoría`);
    if (icon.states.length < 2 || icon.states.length > MAX_STATES) throw new Error(`${icon.id}: 2..${MAX_STATES} estados`);
    const sizes = {};
    icon.sizes.forEach((size) => {
      if (size < 8 || size > MAX_SIZE) throw new Error(`${icon.id}: tamaño ${size} fuera de 8..${MAX_SIZE}`);
      sizes[size] = buildSize(icon, size);
    });
    return { id: icon.id, name: icon.name, category: icon.category, stateNames: icon.states.map((s) => s.name), sizes };
  });
  return { version: 1, categories: CATEGORIES, icons };
}

function catalogSource(catalog) {
  const body = JSON.stringify(catalog, (key, value) => {
    if (key === 'palette') return `__PAL__${value.join(',')}`;
    return value;
  }, 1).replace(/"__PAL__([0-9,]*)"/g, '[$1]');
  return `// Catálogo de iconos con estado del HMI Designer (Alpha16).
// GENERADO por icons/build-catalog.cjs desde icons/library.cjs: no editar a mano.
// Cada tamaño: images = píxeles 4 bpp en base64 (dos por byte, el de la
// izquierda en el nibble alto, (ancho + 1) / 2 bytes por fila); states = qué
// imagen y qué paleta RGB565 (16 colores, índice 0 transparente) usa cada estado.
(function (root, data) {
  if (typeof module === 'object' && module.exports) module.exports = data;
  else root.JWPLCIconCatalog = data;
})(typeof self !== 'undefined' ? self : this, Object.freeze(${body}));
`;
}

// ---- hoja de previsualización ----------------------------------------------
function glcdFont() {
  const js = fs.readFileSync(path.join(DESIGNER, 'poc', 'gfx-classic-font.js'), 'utf8');
  const body = js.slice(js.indexOf('new Uint8Array(['), js.indexOf(']);')).replace(/\/\/.*$/gm, '');
  return body.match(/0x[0-9A-Fa-f]{2}/g).map((h) => parseInt(h, 16));
}

const ASCII = { 'á': 160, 'é': 130, 'í': 161, 'ó': 162, 'ú': 163, 'ñ': 164, 'Á': 65, 'É': 144, 'Ó': 79, 'Ú': 85, '·': 250, 'Ñ': 165 };

function writePng(file, w, h, rgb) {
  const crcTable = Array.from({ length: 256 }, (_, n) => {
    let c = n;
    for (let k = 0; k < 8; k += 1) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
    return c >>> 0;
  });
  const crc32 = (buf) => {
    let crc = 0xFFFFFFFF;
    for (const b of buf) crc = crcTable[(crc ^ b) & 0xFF] ^ (crc >>> 8);
    return (crc ^ 0xFFFFFFFF) >>> 0;
  };
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const td = Buffer.concat([Buffer.from(type), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(td));
    return Buffer.concat([len, td, crc]);
  };
  const raw = Buffer.alloc((w * 3 + 1) * h);
  for (let y = 0; y < h; y += 1) {
    raw[y * (w * 3 + 1)] = 0;
    rgb.copy(raw, y * (w * 3 + 1) + 1, y * w * 3, (y + 1) * w * 3);
  }
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(w, 0); ihdr.writeUInt32BE(h, 4); ihdr[8] = 8; ihdr[9] = 2;
  fs.writeFileSync(file, Buffer.concat([Buffer.from([0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A]),
    chunk('IHDR', ihdr), chunk('IDAT', zlib.deflateSync(raw, { level: 9 })), chunk('IEND', Buffer.alloc(0))]));
}

function renderSheet(catalog) {
  const core = require(path.join(DESIGNER, 'poc', 'designer-icons-core.js'));
  const font = glcdFont();
  const SIZE = 48;
  const CELL = SIZE + 10;
  const NAME_W = 170;
  const maxStates = Math.max(...catalog.icons.map((i) => i.stateNames.length));
  const W = NAME_W + maxStates * CELL + 70 + 10;
  const rows = catalog.icons.length + catalog.categories.length;
  const H = rows * (CELL + 2) + 10;
  const fb = new Uint16Array(W * H).fill(0x0000);
  const px = (x, y, c) => { if (x >= 0 && y >= 0 && x < W && y < H) fb[y * W + x] = c; };
  const text = (s, x, y, c) => {
    for (const ch of s) {
      const code = ASCII[ch] ?? ch.charCodeAt(0);
      const idx = (code > 175 ? code + 1 : code) * 5;
      for (let col = 0; col < 5; col += 1) for (let row = 0; row < 8; row += 1) if ((font[idx + col] >> row) & 1) px(x + col, y + row, c);
      x += 6;
    }
  };
  const blit = (icon, size, st, x0, y0) => {
    const img = core.decode(icon.sizes[size], st);
    for (let y = 0; y < img.height; y += 1) for (let x = 0; x < img.width; x += 1) {
      const c = img.pixels[y * img.width + x];
      if (c >= 0) px(x0 + x, y0 + y, c);
    }
  };
  let y = 6;
  for (const cat of catalog.categories) {
    text(cat.name.toUpperCase(), 6, y + 4, 0x07FF);
    y += 18;
    for (const icon of catalog.icons.filter((i) => i.category === cat.id)) {
      const big = icon.sizes[SIZE] ? SIZE : Math.max(...Object.keys(icon.sizes).map(Number));
      text(icon.name, 6, y + 6, 0xFFFF);
      text(icon.id.split('/')[1], 6, y + 18, 0x8410);
      text(Object.keys(icon.sizes).join('/') + ' px', 6, y + 30, 0x8410);
      icon.stateNames.forEach((name, st) => {
        const x = NAME_W + st * CELL;
        for (let k = 0; k < SIZE; k += 1) for (let j = 0; j < SIZE; j += 1) px(x + j, y + k, 0x10A2);
        blit(icon, big, st, x, y);
        text(name.slice(0, 9), x, y + SIZE + 1, 0x8410);
      });
      if (icon.sizes[24]) blit(icon, 24, 1, NAME_W + maxStates * CELL + 6, y);
      if (icon.sizes[64]) text('64', NAME_W + maxStates * CELL + 36, y + 6, 0xFFE0);
      y += CELL + 10;
    }
  }
  const height = Math.min(H, y + 4);
  const rgb = Buffer.alloc(W * height * 3);
  for (let i = 0; i < W * height; i += 1) {
    const c = fb[i];
    rgb[i * 3] = ((c >> 11) & 0x1F) * 255 / 31;
    rgb[i * 3 + 1] = ((c >> 5) & 0x3F) * 255 / 63;
    rgb[i * 3 + 2] = (c & 0x1F) * 255 / 31;
  }
  // Escala 2 para verla cómoda.
  const S = 2;
  const big = Buffer.alloc(W * S * height * S * 3);
  for (let yy = 0; yy < height * S; yy += 1) for (let xx = 0; xx < W * S; xx += 1) {
    rgb.copy(big, (yy * W * S + xx) * 3, ((yy >> 1) * W + (xx >> 1)) * 3, ((yy >> 1) * W + (xx >> 1)) * 3 + 3);
  }
  writePng(SHEET, W * S, height * S, big);
}

function main() {
  const catalog = buildCatalog();
  const source = catalogSource(catalog);
  if (process.argv.includes('--check')) {
    const current = fs.existsSync(CATALOG) ? fs.readFileSync(CATALOG, 'utf8') : '';
    if (current.replace(/\r\n/g, '\n') !== source.replace(/\r\n/g, '\n')) {
      console.error('poc/designer-icon-catalog.js no coincide con icons/library.cjs: corre icons/build-catalog.cjs');
      process.exit(1);
    }
    console.log(`Catálogo al día: ${catalog.icons.length} iconos`);
    return;
  }
  fs.writeFileSync(CATALOG, source);
  renderSheet(catalog);
  const sizes = catalog.icons.reduce((n, i) => n + Object.keys(i.sizes).length, 0);
  console.log(`${catalog.icons.length} iconos, ${sizes} tamaños, ${catalog.categories.length} categorías -> ${path.relative(process.cwd(), CATALOG)} (${(source.length / 1024).toFixed(0)} KB)`);
  console.log(`Hoja: ${path.relative(process.cwd(), SHEET)}`);
}

if (require.main === module) main();
module.exports = { buildCatalog, catalogSource };

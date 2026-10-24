#!/usr/bin/env node
// Catálogo de iconos (poc/designer-icon-catalog.js), sin navegador:
// 1. está al día con icons/library.cjs (build-catalog.cjs --check);
// 2. cada entrada cabe en el firmware: 24/32/48/64 px, 2..8 estados, 16 colores,
//    filas de (ancho + 1) / 2 bytes e índices de imagen válidos;
// 3. cada categoría tiene iconos y los ids son `categoria/nombre` únicos.
'use strict';

const path = require('path');
const { spawnSync } = require('child_process');
const catalog = require('../poc/designer-icon-catalog.js');
const core = require('../poc/designer-icons-core.js');

let failures = 0;
function check(condition, message) {
  console.log(`  ${condition ? 'PASS' : 'FAIL'} ${message}`);
  if (!condition) failures += 1;
}

const build = spawnSync(process.execPath, [path.join(__dirname, '..', 'icons', 'build-catalog.cjs'), '--check'], { encoding: 'utf8' });
check(build.status === 0, `catálogo al día con icons/library.cjs (si falla: node icons/build-catalog.cjs)${build.status ? `\n${build.stdout}${build.stderr}` : ''}`);

const problems = [];
const ids = new Set();
for (const icon of catalog.icons) {
  if (ids.has(icon.id)) problems.push(`${icon.id}: id repetido`);
  ids.add(icon.id);
  if (!icon.id.startsWith(`${icon.category}/`)) problems.push(`${icon.id}: no empieza por su categoría`);
  if (icon.stateNames.length < 2 || icon.stateNames.length > core.MAX_STATES) problems.push(`${icon.id}: ${icon.stateNames.length} estados`);
  for (const [size, entry] of Object.entries(icon.sizes)) {
    const where = `${icon.id}@${size}`;
    if (![24, 32, 48, 64].includes(Number(size)) || entry.width !== Number(size) || entry.height !== Number(size)) problems.push(`${where}: tamaño`);
    if (entry.states.length !== icon.stateNames.length) problems.push(`${where}: estados ${entry.states.length} != nombres ${icon.stateNames.length}`);
    entry.images.forEach((b64, i) => {
      if (core.base64ToBytes(b64).length !== entry.height * Math.ceil(entry.width / 2)) problems.push(`${where}: imagen ${i} con largo incorrecto`);
    });
    entry.states.forEach((s, i) => {
      if (!(s.image >= 0 && s.image < entry.images.length)) problems.push(`${where}: estado ${i} apunta a una imagen inexistente`);
      if (s.palette.length !== 16 || s.palette.some((c) => !(Number.isInteger(c) && c >= 0 && c <= 0xFFFF))) problems.push(`${where}: paleta del estado ${i}`);
      const d = core.decode(entry, i);
      if (d.pixels.length !== entry.width * entry.height || d.pixels.every((p) => p < 0)) problems.push(`${where}: estado ${i} vacío`);
    });
    if (core.paletteLength(entry) > 16) problems.push(`${where}: más de 16 colores`);
  }
}
check(problems.length === 0, `${catalog.icons.length} iconos caben en el firmware${problems.length ? `\n      ${problems.join('\n      ')}` : ''}`);
check(catalog.categories.every((c) => catalog.icons.some((i) => i.category === c.id)), `${catalog.categories.length} categorías, todas con iconos`);
check(catalog.icons.some((i) => i.sizes[64]), 'hay iconos de 64 px');

console.log(failures ? `\n${failures} verificación(es) fallaron.` : '\nTodas las verificaciones pasaron.');
process.exit(failures ? 1 : 0);

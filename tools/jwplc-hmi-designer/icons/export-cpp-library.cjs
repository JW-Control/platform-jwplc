#!/usr/bin/env node
// Exporta la biblioteca completa de iconos (28 iconos en todos sus tamaños) a C++
// para la librería JWPLC_Display (JWPLC_Icons.h y JWPLC_Icons.cpp).
//
// Uso:
//   node tools/jwplc-hmi-designer/icons/export-cpp-library.cjs [--check]
//
// --check: verifica que los archivos generados estén al día sin sobreescribir.
'use strict';

const fs = require('fs');
const path = require('path');
const catalog = require('../poc/designer-icon-catalog.js');
const core = require('../poc/designer-icons-core.js');

const HERE = __dirname;
const SRC_DIR = path.resolve(HERE, '..', '..', '..', 'JWPLC', '2.1.0', 'libraries', 'JWPLC_Display', 'src');
const HEADER_PATH = path.join(SRC_DIR, 'JWPLC_Icons.h');
const CPP_PATH = path.join(SRC_DIR, 'JWPLC_Icons.cpp');

const METHOD_MAP = {
  'motores/motor': 'motor',
  'motores/motorreductor': 'motorreductor',
  'motores/motor_dos_sentidos': 'motorDosSentidos',
  'bombas/bomba': 'bomba',
  'bombas/ventilador': 'ventilador',
  'bombas/compresor': 'compresor',
  'pilotos/piloto': 'piloto',
  'pilotos/baliza': 'baliza',
  'valvulas/valvula': 'valvula',
  'valvulas/valvula_motorizada': 'valvulaMotorizada',
  'valvulas/valvula_solenoide': 'valvulaSolenoide',
  'equipos/piston': 'piston',
  'equipos/piston_vertical': 'pistonVertical',
  'equipos/tanque': 'tanque',
  'equipos/silo': 'silo',
  'equipos/resistencia': 'resistencia',
  'equipos/agitador': 'agitador',
  'equipos/transportador': 'transportador',
  'sensores/proximidad': 'proximidad',
  'sensores/termometro': 'termometro',
  'sensores/manometro': 'manometro',
  'alarmas/alarma': 'alarma',
  'alarmas/campana': 'campana',
  'alarmas/sirena': 'sirena',
  'botones/pulsador': 'pulsador',
  'botones/paro_emergencia': 'paroEmergencia',
  'botones/selector': 'selector',
  'botones/contacto': 'contacto'
};

function hex(value, digits) {
  return `0x${(Number(value) >>> 0).toString(16).toUpperCase().padStart(digits, '0')}`;
}

function wrap(items, perLine, indent) {
  const lines = [];
  for (let i = 0; i < items.length; i += perLine) {
    lines.push(indent + items.slice(i, i + perLine).join(', '));
  }
  return lines.join(',\n');
}

function generateHeader() {
  const lines = [];
  lines.push('// Biblioteca nativa de iconos JWPLC (JWPLC_Display)');
  lines.push('// GENERADO AUTOMÁTICAMENTE por tools/jwplc-hmi-designer/icons/export-cpp-library.cjs');
  lines.push('// No editar manualmente. Para actualizar, edita icons/library.cjs y regenera.');
  lines.push('');
  lines.push('#ifndef JWPLC_ICONS_H');
  lines.push('#define JWPLC_ICONS_H');
  lines.push('');
  lines.push('#include <Arduino.h>');
  lines.push('#include <JWPLC_UI.h>');
  lines.push('');
  lines.push('class JWPLC_IconsClass');
  lines.push('{');
  lines.push('public:');

  for (const cat of catalog.categories) {
    lines.push(`    // --- ${cat.name} ---`);
    const catIcons = catalog.icons.filter((i) => i.category === cat.id);
    for (const icon of catIcons) {
      const method = METHOD_MAP[icon.id];
      const sizes = Object.keys(icon.sizes).map(Number).sort((a, b) => a - b);
      lines.push(`    static const JWPLC_UIIcon* ${method}(uint8_t size = ${sizes[0]}); // ${icon.name} [${sizes.join(', ')} px]`);
    }
    lines.push('');
  }

  lines.push('    // Búsqueda genérica por identificador del catálogo ("motores/motor", "motor", etc.)');
  lines.push('    static const JWPLC_UIIcon* get(const char *iconId, uint8_t size = 24);');
  lines.push('};');
  lines.push('');
  lines.push('extern JWPLC_IconsClass JWPLC_Icons;');
  lines.push('');
  lines.push('#endif // JWPLC_ICONS_H');
  lines.push('');
  return lines.join('\n');
}

function generateCpp() {
  const lines = [];
  lines.push('// Implementación de biblioteca nativa de iconos JWPLC');
  lines.push('// GENERADO AUTOMÁTICAMENTE por tools/jwplc-hmi-designer/icons/export-cpp-library.cjs');
  lines.push('// No editar manualmente.');
  lines.push('');
  lines.push('#include "JWPLC_Icons.h"');
  lines.push('#include <string.h>');
  lines.push('');
  lines.push('JWPLC_IconsClass JWPLC_Icons;');
  lines.push('');
  lines.push('namespace {');

  for (const icon of catalog.icons) {
    const method = METHOD_MAP[icon.id];
    const prefix = icon.id.replace(/[^A-Za-z0-9]/g, '_').toUpperCase();

    lines.push(`\n// ============================================================================`);
    lines.push(`// ${icon.name} (${icon.id})`);
    lines.push(`// ============================================================================`);

    const sizes = Object.keys(icon.sizes).map(Number).sort((a, b) => a - b);
    for (const size of sizes) {
      const entry = icon.sizes[size];
      const sym = `ICON_${prefix}_${size}`;
      const n = entry.states.length;
      const colors = core.paletteLength(entry);

      lines.push(`\n// --- ${icon.name} ${size} px (${n} estados) ---`);

      // Pixels
      entry.images.forEach((b64, k) => {
        const bytes = Array.from(core.base64ToBytes(b64), (b) => hex(b, 2));
        lines.push(`static const uint8_t ${sym}_PX${k}[${bytes.length}] =\n{\n${wrap(bytes, 16, '    ')}\n};`);
      });

      // Palettes
      const pals = [];
      for (let sIdx = 0; sIdx < n; sIdx += 1) {
        const pal = entry.states[sIdx].palette;
        pals.push(`    {${pal.slice(0, colors).map((c) => hex(c, 4)).join(', ')}}`);
      }
      lines.push(`static const uint16_t ${sym}_PAL[${n}][${colors}] =\n{\n${pals.join(',\n')}\n};`);

      // Frames for animated states
      entry.states.forEach((st, k) => {
        if (st.frames && st.frames.length > 1) {
          const frameLines = st.frames.map((f) => `    {${sym}_PX${f.image}, ${sym}_PAL[${k}]}`);
          lines.push(`static const JWPLC_UIIconFrame ${sym}_ST${k}_FRAMES[${st.frames.length}] =\n{\n${frameLines.join(',\n')}\n};`);
        }
      });

      // States
      const states = [];
      for (let k = 0; k < n; k += 1) {
        const st = entry.states[k];
        if (st.frames && st.frames.length > 1) {
          states.push(`    {${sym}_PX${st.image}, ${sym}_PAL[${k}], ${st.frames.length}, ${st.intervalMs || 120}, ${sym}_ST${k}_FRAMES}`);
        } else {
          states.push(`    {${sym}_PX${st.image}, ${sym}_PAL[${k}]}`);
        }
      }
      lines.push(`static const JWPLC_UIIconState ${sym}_STATES[${n}] =\n{\n${states.join(',\n')}\n};`);

      // Icon struct
      lines.push(`static const JWPLC_UIIcon ${sym}(${entry.width}, ${entry.height}, ${n}, ${sym}_STATES, ${entry.transparent});`);
    }
  }

  lines.push('\n} // namespace anonymous');
  lines.push('');

  // Implement methods
  for (const icon of catalog.icons) {
    const method = METHOD_MAP[icon.id];
    const prefix = icon.id.replace(/[^A-Za-z0-9]/g, '_').toUpperCase();
    const sizes = Object.keys(icon.sizes).map(Number).sort((a, b) => a - b);

    lines.push(`const JWPLC_UIIcon* JWPLC_IconsClass::${method}(uint8_t size)`);
    lines.push('{');
    if (sizes.length === 1) {
      lines.push(`    return &ICON_${prefix}_${sizes[0]};`);
    } else {
      // Find midpoints for nearest match
      for (let i = 0; i < sizes.length - 1; i += 1) {
        const sCurrent = sizes[i];
        const sNext = sizes[i + 1];
        const threshold = Math.floor((sCurrent + sNext) / 2);
        lines.push(`    if (size <= ${threshold}) return &ICON_${prefix}_${sCurrent};`);
      }
      lines.push(`    return &ICON_${prefix}_${sizes[sizes.length - 1]};`);
    }
    lines.push('}');
    lines.push('');
  }

  // Implement get()
  lines.push('const JWPLC_UIIcon* JWPLC_IconsClass::get(const char *iconId, uint8_t size)');
  lines.push('{');
  lines.push('    if (!iconId || !iconId[0]) return motor(size);');
  for (const icon of catalog.icons) {
    const method = METHOD_MAP[icon.id];
    const shortName = icon.id.split('/')[1];
    lines.push(`    if (strcmp(iconId, "${icon.id}") == 0 || strcmp(iconId, "${shortName}") == 0 || strcmp(iconId, "${method}") == 0) return ${method}(size);`);
  }
  lines.push('    return motor(size); // fallback');
  lines.push('}');
  lines.push('');

  return lines.join('\n');
}

function main() {
  const isCheck = process.argv.includes('--check');
  const headerContent = generateHeader();
  const cppContent = generateCpp();

  if (isCheck) {
    let ok = true;
    if (!fs.existsSync(HEADER_PATH) || fs.readFileSync(HEADER_PATH, 'utf8') !== headerContent) {
      console.error(`JWPLC_Icons.h no está al día. Ejecuta: node tools/jwplc-hmi-designer/icons/export-cpp-library.cjs`);
      ok = false;
    }
    if (!fs.existsSync(CPP_PATH) || fs.readFileSync(CPP_PATH, 'utf8') !== cppContent) {
      console.error(`JWPLC_Icons.cpp no está al día. Ejecuta: node tools/jwplc-hmi-designer/icons/export-cpp-library.cjs`);
      ok = false;
    }
    process.exit(ok ? 0 : 1);
  }

  fs.mkdirSync(SRC_DIR, { recursive: true });
  fs.writeFileSync(HEADER_PATH, headerContent, 'utf8');
  fs.writeFileSync(CPP_PATH, cppContent, 'utf8');
  console.log(`Exportados con éxito:\n  ${HEADER_PATH}\n  ${CPP_PATH}`);
}

if (require.main === module) main();
module.exports = { METHOD_MAP, generateHeader, generateCpp };

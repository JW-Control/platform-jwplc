#!/usr/bin/env node
// Paridad del formato de VALUE (Alpha15 I1) entre el firmware y el Designer.
//
// Compila JWPLC_Display/src/JWPLC_UI_Format.cpp (el real) en host con g++,
// le pasa una grilla de formatos × valores y compara muestra, desborde y
// texto con poc/designer-value-format.js. Sin g++ sólo corre los casos fijos.
//
//   node tools/jwplc-hmi-designer/tests/value-format-parity.cjs
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const SRC = path.join(ROOT, 'JWPLC', '2.1.0', 'libraries', 'JWPLC_Display', 'src');
const STUB = path.join(__dirname, 'host-stub');
const F = require(path.join(__dirname, '..', 'poc', 'designer-value-format.js'));
const C = require(path.join(__dirname, '..', 'poc', 'designer-cp437.js'));

let failures = 0;
const check = (ok, msg) => { if (!ok) { failures++; console.log(`  FAIL ${msg}`); } };

// ---- Casos fijos (contrato legible) ---------------------------------------
const FIXED = [
  [{ integerDigits: 3, decimalDigits: 1 }, 12.34, '12.3'],
  [{ integerDigits: 3 }, 1234, '###'],
  [{ integerDigits: 3 }, 2.5, '2'],
  [{ integerDigits: 3, decimalDigits: 2 }, 0.125, '0.12'],
  [{ integerDigits: 6, thousandsSep: ',' }, 123456, '123,456'],
  [{ integerDigits: 7, thousandsSep: ' ' }, 1234567, '1 234 567'],
  [{ integerDigits: 6, decimalDigits: 2, thousandsSep: '.' }, 123456.78, '123.456,78'],
  [{ integerDigits: 6, thousandsSep: ',', leadingZeros: true }, 1234, '001,234'],
  [{ integerDigits: 6, thousandsSep: ',', signedValue: true }, -1234, '-1,234'],
  [{ integerDigits: 4, thousandsSep: ',' }, 12345, '#####'],
  [{ valueKind: 'DURATION', durationPattern: 'SS', integerDigits: 3 }, 45, '45'],
  [{ valueKind: 'DURATION', durationPattern: 'SS', integerDigits: 2 }, 5, '05'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 2 }, 303, '05:03'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 2 }, 6000, '--:--'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 3 }, 6000, '100:00'],
  [{ valueKind: 'DURATION', durationPattern: 'HH_MM_SS', integerDigits: 2 }, 3903, '01:05:03'],
  [{ valueKind: 'DURATION', durationPattern: 'HH_MM_SS', integerDigits: 2, durationUnit: 'MS' }, 3903999, '01:05:03'],
  [{ valueKind: 'DURATION', durationPattern: 'COMPACT', integerDigits: 2 }, 3903, '1h05m'],
  [{ valueKind: 'DURATION', durationPattern: 'COMPACT', integerDigits: 2 }, 303, '5m03s'],
  [{ valueKind: 'DURATION', durationPattern: 'COMPACT', integerDigits: 2 }, 45, '45s'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 2, signedValue: true }, -75, '-01:15'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 2, signedValue: true }, -0.4, '00:00'],
  [{ valueKind: 'DURATION', durationPattern: 'MM_SS', integerDigits: 2 }, -5, '--:--'],
  [{ valueKind: 'DATETIME', dateTimePattern: 'HH_MM' }, 12, '--:--']
];

console.log('== casos fijos (Designer)');
for (const [field, value, expected] of FIXED) {
  const got = F.format(field, value).text;
  check(got === expected, `${JSON.stringify(field)} ${value} -> "${got}" (esperado "${expected}")`);
}
console.log(`  ${FIXED.length} casos`);

// ---- Grilla contra el firmware --------------------------------------------
function hasGpp() {
  try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); return true; } catch (_) { return false; }
}

const PATTERN_ENUM = { SS: 0, MM_SS: 1, HH_MM_SS: 2, COMPACT: 3 };

function grid() {
  const formats = [];
  for (const integerDigits of [1, 2, 3, 4, 6, 9]) {
    for (const decimalDigits of [0, 1, 2, 3]) {
      for (const signedValue of [false, true]) {
        for (const leadingZeros of [false, true]) {
          for (const thousandsSep of ['', ',', '.', ' ']) {
            formats.push({ valueKind: 'NUMBER', integerDigits, decimalDigits, signedValue, leadingZeros, thousandsSep });
          }
        }
      }
    }
    for (const durationPattern of Object.keys(PATTERN_ENUM)) {
      for (const durationUnit of ['S', 'MS']) {
        for (const signedValue of [false, true]) {
          formats.push({ valueKind: 'DURATION', integerDigits, durationPattern, durationUnit, signedValue });
        }
      }
    }
  }
  // Incluye empates exactos (2.5, 0.125, 987654321.25): printf redondea al par.
  const values = [0, 1, 7, 12.34, 59, 60, 61, 99.99, 100, 303, 999, 1000, 3599, 3600, 3903, 9999.4,
    12345.678, 86399, 123456, 999999, 1234567.891, 359999, 360000, 3903999, 123456789, 987654321.25,
    2.5, 3.5, 0.125, 0.375, -2.5, -0.5, -1, -12.34, -75, -0.4, -3903, -123456.7, 1e16, -1e16];
  return { formats, values };
}

const DRIVER = `
#include "JWPLC_UI_Format.h"
#include <cstdio>
#include <cstdlib>
int main() {
  int ints, decs, sgn, lz, sep, kind, pat, unit; char v[64];
  // Primero los relojes: "C pat valid year month day hour minute second".
  int nclocks = 0;
  if (scanf("%d", &nclocks) != 1) return 1;
  for (int i = 0; i < nclocks; ++i) {
    int p, valid, y, mo, d, h, mi, se;
    if (scanf("%d %d %d %d %d %d %d %d", &p, &valid, &y, &mo, &d, &h, &mi, &se) != 8) return 1;
    JWPLC_UIValueFormat f = JWPLC_UIDateTimeFormat((JWPLC_UIDateTimePattern)p);
    JWPLCUIFormat::Clock c = { valid != 0, (uint16_t)y, (uint8_t)mo, (uint8_t)d, (uint8_t)h, (uint8_t)mi, (uint8_t)se };
    char s[64], o[64], t[64];
    JWPLCUIFormat::sample(f, s, sizeof(s));
    JWPLCUIFormat::overflow(f, o, sizeof(o));
    bool ok = JWPLCUIFormat::dateTime(f, c, t, sizeof(t));
    printf("%s|%s|%s|%d\\n", s, o, t, ok ? 1 : 0);
  }
  while (scanf("%d %d %d %d %d %d %d %d %63s", &ints, &decs, &sgn, &lz, &sep, &kind, &pat, &unit, v) == 9) {
    JWPLC_UIValueFormat f = kind
      ? JWPLC_UIDurationFormat((JWPLC_UIDurationPattern)pat, (JWPLC_UIDurationUnit)unit, (uint8_t)ints, sgn != 0)
      : JWPLC_UIValueFormat((uint8_t)ints, (uint8_t)decs, sgn != 0, lz != 0, (char)sep);
    char s[64], o[64], t[64];
    JWPLCUIFormat::sample(f, s, sizeof(s));
    JWPLCUIFormat::overflow(f, o, sizeof(o));
    bool ok = JWPLCUIFormat::value(f, strtod(v, nullptr), t, sizeof(t));
    printf("%s|%s|%s|%d\\n", s, o, t, ok ? 1 : 0);
  }
  return 0;
}
`;

// ---- Texto con tildes (Alpha15 I3) ----------------------------------------
// Tres caminos deben dar los mismos bytes en la TFT:
//   1. el literal que emite el codegen (cppEscape) compilado por g++;
//   2. utf8ToGlcd() del firmware (setText con texto UTF-8 del sketch);
//   3. el lienzo: glyphIndex() -> byteForGlyph().
const TEXTS = ['Presión', '25°C', 'Ñandú', 'Médico', 'Ángulo', '¿Listo? ¡Sí!', 'µs ± 2²', '☃ nieve 😀',
  'a\\b "q"', '€ precio', 'Ōmega', 'ABC xyz 0123', 'ü Ü ç Ç â ê î ô û', 'Línea\tTab', '½ ¼ « » ª º', '█▌ ≥ ≤ ≈ ∞ √ ÷'];
const canvasBytes = (t) => [...t].map((ch) => C.byteForGlyph(C.glyphIndex(ch.codePointAt(0))));
console.log('== texto: casos fijos (Designer)');
check(C.cppEscape('Presión 25°C') === 'Presi\\242n 25\\367C', `cppEscape("Presión 25°C") -> ${C.cppEscape('Presión 25°C')}`);
check(C.cppEscape('Á') === 'A' && C.cppEscape('☃') === '?', 'sin glifo: la letra sin tilde, si no ?');
check(JSON.stringify(C.unsupported('Ángulo ☃ ñ')) === JSON.stringify(['Á', '☃']), 'unsupported() lista sólo lo que no tiene glifo exacto');

if (!hasGpp()) {
  console.log('== grilla firmware: OMITIDA (sin g++)');
} else {
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-fmt-'));
  const exe = path.join(dir, 'fmt');
  fs.writeFileSync(path.join(dir, 'driver.cpp'), DRIVER);
  execFileSync('g++', ['-std=c++17', '-O1', '-Wall', '-Wextra', '-I', STUB, '-I', SRC,
    path.join(dir, 'driver.cpp'), path.join(SRC, 'JWPLC_UI_Format.cpp'), '-o', exe], { stdio: 'inherit' });

  const { formats, values } = grid();
  const cases = [];
  for (const f of formats) for (const v of values) cases.push([f, v]);

  // Relojes: válidos, bordes y fuera de rango (deben dar guiones).
  const clocks = [
    { valid: true, year: 2026, month: 10, day: 18, hour: 14, minute: 5, second: 7 },
    { valid: true, year: 2000, month: 1, day: 1, hour: 0, minute: 0, second: 0 },
    { valid: true, year: 2099, month: 12, day: 31, hour: 23, minute: 59, second: 59 },
    { valid: true, year: 999, month: 2, day: 9, hour: 9, minute: 9, second: 9 },
    { valid: false, year: 2026, month: 10, day: 18, hour: 14, minute: 5, second: 7 },
    { valid: true, year: 2026, month: 13, day: 1, hour: 0, minute: 0, second: 0 },
    { valid: true, year: 2026, month: 0, day: 1, hour: 0, minute: 0, second: 0 },
    { valid: true, year: 2026, month: 1, day: 32, hour: 0, minute: 0, second: 0 },
    { valid: true, year: 2026, month: 1, day: 1, hour: 24, minute: 0, second: 0 },
    { valid: true, year: 2026, month: 1, day: 1, hour: 0, minute: 60, second: 0 },
    { valid: true, year: 2026, month: 1, day: 1, hour: 0, minute: 0, second: 60 }
  ];
  const clockCases = [];
  F.DATETIME_PATTERNS.forEach((pattern, p) => clocks.forEach((c) => clockCases.push([p, pattern, c])));
  const clockInput = `${clockCases.length}\n` + clockCases.map(([p, , c]) =>
    [p, c.valid ? 1 : 0, c.year, c.month, c.day, c.hour, c.minute, c.second].join(' ')).join('\n') + '\n';

  const input = clockInput + cases.map(([f, v]) => [
    f.integerDigits, f.decimalDigits || 0, f.signedValue ? 1 : 0, f.leadingZeros ? 1 : 0,
    f.thousandsSep ? f.thousandsSep.charCodeAt(0) : 0, f.valueKind === 'DURATION' ? 1 : 0,
    PATTERN_ENUM[f.durationPattern] || 0, f.durationUnit === 'MS' ? 1 : 0, String(v)
  ].join(' ')).join('\n') + '\n';
  const allLines = execFileSync(exe, { input }).toString().trimEnd().split('\n');
  const clockLines = allLines.slice(0, clockCases.length);
  const lines = allLines.slice(clockCases.length);
  check(lines.length === cases.length, `líneas ${lines.length} != casos ${cases.length}`);

  clockCases.forEach(([, pattern, c], i) => {
    const f = { valueKind: 'DATETIME', dateTimePattern: pattern };
    const js = F.dateTime(f, c);
    const want = `${F.sample(f)}|${F.overflow(f)}|${js.text}|${js.ok ? 1 : 0}`;
    if (clockLines[i] !== want) {
      failures++;
      console.log(`  FAIL reloj ${pattern} ${JSON.stringify(c)}\n       firmware ${clockLines[i]}\n       designer ${want}`);
    }
  });
  console.log(`== relojes firmware vs Designer: ${clockCases.length} casos`);

  let shown = 0;
  cases.forEach(([f, v], i) => {
    const [s, o, t, ok] = (lines[i] || '').split('|');
    const js = F.format(f, v);
    const want = `${F.sample(f)}|${F.overflow(f)}|${js.text}|${js.ok ? 1 : 0}`;
    const got = `${s}|${o}|${t}|${ok}`;
    if (got !== want) {
      failures++;
      if (shown++ < 15) console.log(`  FAIL ${JSON.stringify(f)} ${v}\n       firmware ${got}\n       designer ${want}`);
    }
  });
  console.log(`== grilla firmware vs Designer: ${cases.length} casos (${formats.length} formatos × ${values.length} valores)`);

  // Texto: literales del codegen y utf8ToGlcd del firmware.
  const textCpp = `#include "JWPLC_UI_Format.h"
#include <cstdio>
#include <cstring>
static const char *LITERALS[] = {
${TEXTS.map((t) => `  "${C.cppEscape(t)}",`).join('\n')}
};
static const char *UTF8[] = {
${TEXTS.map((t) => `  "${[...Buffer.from(t, 'utf8')].map((b) => `\\${b.toString(8).padStart(3, '0')}`).join('')}",`).join('\n')}
};
static void hex(const char *s) { for (; *s; ++s) printf("%02X", (unsigned)(unsigned char)*s); }
int main() {
  for (size_t i = 0; i < sizeof(LITERALS) / sizeof(LITERALS[0]); ++i) {
    char out[128];
    JWPLCUIFormat::utf8ToGlcd(UTF8[i], out, sizeof(out));
    hex(LITERALS[i]); printf("|"); hex(out); printf("\\n");
  }
  return 0;
}
`;
  fs.writeFileSync(path.join(dir, 'text.cpp'), textCpp);
  const textExe = path.join(dir, 'text');
  execFileSync('g++', ['-std=c++17', '-O1', '-Wall', '-I', STUB, '-I', SRC,
    path.join(dir, 'text.cpp'), path.join(SRC, 'JWPLC_UI_Format.cpp'), '-o', textExe], { stdio: 'inherit' });
  const textLines = execFileSync(textExe).toString().trimEnd().split('\n');
  // Corte a la mitad de una tilde (strncpy): «Presi» + 0xC3 -> «Presi».
  const truncCpp = `#include "JWPLC_UI_Format.h"
#include <cstdio>
int main() { char out[32]; JWPLCUIFormat::utf8ToGlcd("Presi\\303", out, sizeof(out)); printf("%s", out); return 0; }
`;
  fs.writeFileSync(path.join(dir, 'trunc.cpp'), truncCpp);
  execFileSync('g++', ['-std=c++17', '-I', STUB, '-I', SRC, path.join(dir, 'trunc.cpp'), path.join(SRC, 'JWPLC_UI_Format.cpp'), '-o', path.join(dir, 'trunc')]);
  check(execFileSync(path.join(dir, 'trunc')).toString() === 'Presi', 'setText: una tilde cortada al final se descarta');

  TEXTS.forEach((t, i) => {
    const want = canvasBytes(t).map((b) => b.toString(16).toUpperCase().padStart(2, '0')).join('');
    const [literal, runtime] = (textLines[i] || '').split('|');
    if (literal !== want || runtime !== want) {
      failures++;
      console.log(`  FAIL texto ${JSON.stringify(t)}\n       lienzo   ${want}\n       literal  ${literal}\n       setText  ${runtime}`);
    }
  });
  console.log(`== texto: lienzo == literal del codegen == setText del firmware: ${TEXTS.length} casos`);
  fs.rmSync(dir, { recursive: true, force: true });
}

console.log(failures ? `\nVALUE-FORMAT PARITY: FAIL (${failures})` : '\nVALUE-FORMAT PARITY: PASS');
process.exit(failures ? 1 : 0);

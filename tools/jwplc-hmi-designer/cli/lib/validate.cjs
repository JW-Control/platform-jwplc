// Validación de un proyecto .jwhmi ya normalizado (normalize.cjs), en node puro.
//
// Devuelve { errors, warnings }, cada uno [{ path, message }]. Un error impide
// generar el header; un aviso no (pero conviene corregirlo). Los textos dicen
// qué valor se esperaba para que un agente pueda corregir sin abrir la UI.
// Las comprobaciones que necesitan las métricas reales del lienzo (campos que
// salen de la pantalla o se pisan) están en designer.cjs.
'use strict';

const fs = require('fs');
const path = require('path');
const vm = require('vm');
const spec = require('./spec.cjs');
const icons = require('./icons.cjs');

const POC = path.resolve(__dirname, '..', '..', 'poc');
const SYMBOL = /^[A-Za-z_][A-Za-z0-9_]*$/;
const CPP_RESERVED = new Set(('alignas alignof and asm auto bitand bitor bool break case catch char char16_t char32_t class compl const ' +
  'constexpr const_cast continue decltype default delete do double dynamic_cast else enum explicit export extern false float for ' +
  'friend goto if inline int long mutable namespace new noexcept not nullptr operator or private protected public register ' +
  'reinterpret_cast return short signed sizeof static static_assert static_cast struct switch template this thread_local throw ' +
  'true try typedef typeid typename union unsigned using virtual void volatile wchar_t while xor setup loop main tft ' +
  'byte boolean word String HIGH LOW INPUT OUTPUT').split(' '));

let menuModule = null;
// designer-menu.js y designer-cp437.js son puros: se cargan en una caja de node
// como en tests/codegen-core.test.cjs, así la validación del MENU es la del Designer.
function designerModules() {
  if (menuModule) return menuModule;
  const context = { window: {}, console };
  context.self = context.window;
  vm.createContext(context);
  for (const file of ['designer-cp437.js', 'gfx-classic-font.js', 'designer-menu.js']) {
    vm.runInContext(fs.readFileSync(path.join(POC, file), 'utf8'), context, { filename: file });
  }
  menuModule = { menu: context.window.JWPLCHMIMenu, cp437: context.window.JWPLCCp437 };
  return menuModule;
}

function levenshtein(a, b) {
  const dp = Array.from({ length: a.length + 1 }, (_, i) => [i, ...Array(b.length).fill(0)]);
  for (let j = 1; j <= b.length; j += 1) dp[0][j] = j;
  for (let i = 1; i <= a.length; i += 1) {
    for (let j = 1; j <= b.length; j += 1) {
      dp[i][j] = Math.min(dp[i - 1][j] + 1, dp[i][j - 1] + 1, dp[i - 1][j - 1] + (a[i - 1].toLowerCase() === b[j - 1].toLowerCase() ? 0 : 1));
    }
  }
  return dp[a.length][b.length];
}

// Sinónimos frecuentes (en inglés o de otras librerías) de los valores enumerados.
const ALIASES = { MIDDLE: 'CENTER', CENTRE: 'CENTER', MID: 'CENTER', ROW: 'INLINE', HORIZONTAL: 'INLINE', COLUMN: 'STACKED', VERTICAL: 'STACKED',
  STACK: 'STACKED', LED: 'PILOT', LAMP: 'PILOT', CIRCLE: 'ELLIPSE', SQUARE: 'RECT', RECTANGLE: 'RECT', LABEL: 'RAW_TEXT', NUMBER: 'VALUE',
  TIME: 'DURATION', CLOCK: 'DATETIME', DATE: 'DATETIME', PROGRESS: 'BAR', GAUGE: 'BAR', SWITCH: 'BOOL', BUTTON: 'BOOL',
  IMAGE: 'ICON', IMAGEN: 'ICON', ICONO: 'ICON', SYMBOL: 'ICON', BITMAP: 'ICON' };

function closest(word, candidates) {
  const alias = ALIASES[String(word).toUpperCase()];
  if (alias && candidates.includes(alias)) return alias;
  let best = null;
  let bestScore = Infinity;
  for (const c of candidates) {
    const score = levenshtein(word, c);
    if (score < bestScore) { best = c; bestScore = score; }
  }
  return bestScore <= Math.max(2, Math.floor(word.length / 3)) ? best : null;
}

function createReport() {
  const errors = [];
  const warnings = [];
  return {
    errors,
    warnings,
    error: (p, message) => errors.push({ path: p, message }),
    warn: (p, message) => warnings.push({ path: p, message })
  };
}

function describeEnum(values) {
  return values.map((v) => (v === '' ? '""' : JSON.stringify(v))).join(' | ');
}

// Comprueba las propiedades de un objeto contra su tabla de spec.cjs.
function checkProps(object, props, where, report, options = {}) {
  for (const [key, def] of Object.entries(props)) {
    const value = object[key];
    if (value === undefined || value === null) {
      if (def.required) report.error(`${where}.${key}`, `falta «${key}» (${def.desc})`);
      continue;
    }
    const at = `${where}.${key}`;
    switch (def.kind) {
      case 'int':
      case 'number': {
        if (typeof value !== 'number' || !Number.isFinite(value)) { report.error(at, `debe ser un número (recibido ${JSON.stringify(value)})`); break; }
        if (def.kind === 'int' && !Number.isInteger(value)) { report.error(at, `debe ser un entero (recibido ${value})`); break; }
        if (def.min !== undefined && value < def.min) report.error(at, `mínimo ${def.min} (recibido ${value})`);
        if (def.max !== undefined && value > def.max) report.error(at, `máximo ${def.max} (recibido ${value})`);
        break;
      }
      case 'bool':
        if (typeof value !== 'boolean') report.error(at, `debe ser true o false (recibido ${JSON.stringify(value)})`);
        break;
      case 'string':
        if (typeof value !== 'string') report.error(at, `debe ser texto (recibido ${JSON.stringify(value)})`);
        break;
      case 'color':
        if (value === 'TRANSPARENT' && key === 'backgroundColor') break;
        if (!Number.isInteger(value) || value < 0 || value > 0xFFFF) {
          report.error(at, `color no válido ${JSON.stringify(value)}: usa RGB565 (0..65535 o "0xF800"), "#RRGGBB" o un nombre (${Object.keys(spec.NAMED_COLORS).slice(0, 8).join(', ')}…)`);
        }
        break;
      case 'symbol':
        if (typeof value !== 'string' || (value !== '' && !SYMBOL.test(value))) {
          report.error(at, `«${value}» no es un identificador C++ (letras sin tilde, dígitos y _, sin empezar por dígito)`);
        } else if (CPP_RESERVED.has(value)) {
          report.error(at, `«${value}» es una palabra reservada o un nombre del core; usa otro`);
        }
        break;
      case 'enum':
        if (!def.enum.includes(value)) {
          const hint = typeof value === 'string' ? closest(value, def.enum.filter((v) => typeof v === 'string' && v)) : null;
          report.error(at, `valor ${JSON.stringify(value)} no válido; usa ${describeEnum(def.enum)}${hint ? ` (¿${hint}?)` : ''}`);
        }
        break;
      case 'array':
        if (!Array.isArray(value)) report.error(at, 'debe ser un arreglo');
        break;
      case 'object':
        if (typeof value !== 'object' || Array.isArray(value)) report.error(at, 'debe ser un objeto');
        break;
      default:
        break;
    }
  }
  if (options.unknown !== false) {
    const known = Object.keys(props);
    for (const key of Object.keys(object)) {
      if (props[key] || (options.allow || []).includes(key)) continue;
      const hint = closest(key, known);
      report.warn(`${where}.${key}`, `propiedad desconocida «${key}» (se ignora)${hint ? `; ¿quisiste decir «${hint}»?` : ''}`);
    }
  }
}

function noGlyph(report, at, text) {
  if (!text) return;
  const missing = designerModules().cp437?.unsupported(String(text)) || [];
  if (missing.length) report.warn(at, `«${missing.join(' ')}» no existe en la fuente de la TFT (se ve sin tilde o como ?)`);
}

// Tipo C++ de la variable de cada campo (como designer-codegen-core.js).
function cppType(field) {
  if (field.type === 'TEXT') return 'char[]';
  if (field.type === 'BOOL') return 'bool';
  if (field.type === 'BAR') return 'float';
  if (field.type === 'ICON') return field.iconVarType === 'uint8' ? 'uint8_t' : 'bool';
  if (field.type === 'VALUE') {
    if (field.valueKind === 'DATETIME') return null;
    const integer = field.valueKind === 'DURATION' || ([',', '.', ' '].includes(field.thousandsSep) && !(Number(field.decimalDigits) > 0));
    if (!integer) return 'float';
    return field.signedValue ? 'int32_t' : 'uint32_t';
  }
  return null;
}

function validateField(field, i, project, report) {
  const where = `fields[${i}]${field && field.id ? ` (${field.id})` : ''}`;
  if (!field || typeof field !== 'object' || Array.isArray(field)) { report.error(where, 'cada objeto debe ser un objeto JSON'); return; }
  const def = spec.TYPES[field.type];
  if (!def) {
    const hint = typeof field.type === 'string' ? closest(field.type, Object.keys(spec.TYPES)) : null;
    report.error(`${where}.type`, `tipo ${JSON.stringify(field.type)} desconocido; usa ${Object.keys(spec.TYPES).join(' | ')}${hint ? ` (¿${hint}?)` : ''}`);
    return;
  }
  checkProps(field, def.props, where, report, { allow: field.type === 'NAV_BAR' ? [] : ['key'] });

  if (field.type !== 'NAV_BAR' && Number.isInteger(field.page) && field.page >= project.pages.length) {
    report.error(`${where}.page`, `la página ${field.page} no existe (hay ${project.pages.length}: 0..${project.pages.length - 1})`);
  }

  switch (field.type) {
    case 'TEXT':
    case 'VALUE':
    case 'BOOL':
    case 'BAR':
    case 'ICON':
      noGlyph(report, where, `${field.label || ''}${field.unit || ''}${field.falseText || ''}${field.trueText || ''}${field.barLabel || ''}${field.barUnit || ''}`);
      break;
    case 'RAW_TEXT':
      if (field.font === 'GLCD') noGlyph(report, `${where}.text`, field.text);
      else if (/[^\x20-\x7e]/.test(field.text || '')) report.warn(`${where}.text`, `las fuentes ${field.font} sólo tienen ASCII: tildes, ñ y ° sólo con font "GLCD"`);
      break;
    default:
      break;
  }

  if (field.type === 'VALUE') {
    if (field.valueKind === 'DURATION' && Number(field.decimalDigits) > 0) report.warn(`${where}.decimalDigits`, 'DURATION no usa decimales');
  }
  if (field.type === 'BAR') {
    if (Number(field.barMax) <= Number(field.barMin)) report.error(`${where}.barMax`, `barMax (${field.barMax}) debe ser mayor que barMin (${field.barMin})`);
    const caps = { barPreset: 'LINEAR', barFillMode: 'SOLID', barOrientation: 'HORIZONTAL' };
    for (const [key, ok] of Object.entries(caps)) {
      if (field[key] !== undefined && field[key] !== ok) report.warn(`${where}.${key}`, `el firmware sólo dibuja ${key} = ${ok}; se generará así`);
    }
  }
  if (field.type === 'ICON') validateIcon(field, where, project, report);
  if (spec.SHAPE_TYPES.includes(field.type)) {
    if (field.type !== 'LINE' && (field.x === field.x2 || field.y === field.y2)) report.warn(where, 'la caja tiene ancho o alto 0: no se verá');
  }
  if (field.type === 'MENU' && Array.isArray(field.nodes)) {
    field.nodes.forEach((n, k) => {
      if (n && typeof n === 'object') checkProps(n, spec.MENU_NODE, `${where}.nodes[${k}]${n.label ? ` (${n.label})` : ''}`, report);
    });
    if (field.colors && typeof field.colors === 'object') {
      for (const [key, value] of Object.entries(field.colors)) {
        if (!(key in spec.MENU_COLORS)) report.warn(`${where}.colors.${key}`, `color desconocido; usa ${Object.keys(spec.MENU_COLORS).join(', ')}`);
        else if (!Number.isInteger(value) || value < 0 || value > 0xFFFF) report.error(`${where}.colors.${key}`, `color no válido ${JSON.stringify(value)}`);
      }
    }
  }
}

function validateIcon(field, where, project, report) {
  const info = icons.iconInfo(field.iconId, project.iconAssets);
  if (!info) {
    const ids = icons.listIcons().map((i) => i.id);
    const wanted = String(field.iconId || '');
    const byName = ids.filter((id) => id.split('/').pop() === wanted.split('/').pop().toLowerCase());
    const hint = byName.length ? byName.join(' o ') : closest(wanted, ids);
    report.error(`${where}.iconId`, `icono ${JSON.stringify(field.iconId)} no está en el catálogo${hint ? ` (¿${hint}?)` : ''}; lista: jwhmi icons`);
    return;
  }
  if (!info.sizes.includes(Number(field.iconSize))) {
    report.error(`${where}.iconSize`, `${info.id} no tiene ${field.iconSize} px; tamaños: ${info.sizes.join(', ')}`);
  }
  const states = info.stateNames.length;
  if (field.iconVarType !== 'uint8' && states > 2) {
    report.warn(`${where}.iconVarType`, `${info.id} tiene ${states} estados (${info.stateNames.join(', ')}) y con bool sólo se ven 0 y 1; usa "uint8"`);
  }
  if (Number(field.iconState) >= states) report.warn(`${where}.iconState`, `${info.id} tiene los estados 0..${states - 1}`);
}

function validateProject(project) {
  const report = createReport();
  if (!project || typeof project !== 'object') { report.error('$', 'el proyecto debe ser un objeto JSON'); return report; }
  if (project.format !== 'JWPLC_HMI_PROJECT') report.error('format', 'debe ser "JWPLC_HMI_PROJECT"');
  if (project.version !== 1) report.error('version', 'debe ser 1');
  checkProps(project, spec.PROJECT, '$', report);

  // Páginas
  const pages = Array.isArray(project.pages) ? project.pages : [];
  if (!pages.length) report.error('pages', 'hace falta al menos una página');
  if (pages.length > spec.LIMITS.pages) report.error('pages', `máximo ${spec.LIMITS.pages} páginas (hay ${pages.length})`);
  const pageNames = new Map();
  pages.forEach((page, i) => {
    const where = `pages[${i}]`;
    if (!page || typeof page !== 'object') { report.error(where, 'cada página debe ser un objeto o un texto'); return; }
    checkProps(page, spec.PAGE, where, report);
    if (page.id !== i) report.error(`${where}.id`, `los ids de página deben ser 0..N-1 en orden (esperado ${i})`);
    const key = String(page.name || '').trim().toUpperCase();
    if (key && pageNames.has(key)) report.warn(`${where}.name`, `nombre repetido con pages[${pageNames.get(key)}]`);
    pageNames.set(key, i);
    noGlyph(report, `${where}.name`, page.name);
  });

  if (project.display && typeof project.display === 'object') checkProps(project.display, spec.DISPLAY, 'display', report);

  // Objetos
  const fields = Array.isArray(project.fields) ? project.fields : [];
  if (fields.length > spec.LIMITS.fields) report.error('fields', `máximo ${spec.LIMITS.fields} objetos (hay ${fields.length}, contando la NAV_BAR)`);
  fields.forEach((field, i) => validateField(field, i, project, report));

  const objects = fields.filter((f) => f && typeof f === 'object' && spec.TYPES[f.type]);
  // Ids únicos (los de NAV_BAR no cuentan).
  const ids = new Map();
  objects.forEach((f) => {
    if (f.type === 'NAV_BAR' || !f.id) return;
    const i = fields.indexOf(f);
    if (ids.has(f.id)) report.error(`fields[${i}].id`, `el id ${f.id} ya lo usa fields[${ids.get(f.id)}]`);
    else ids.set(f.id, i);
  });
  // Variables únicas entre campos (designer-codegen.js validationIssues); el MENU sí puede
  // usar la variable de un campo si el tipo coincide (lo revisa designer-menu.js).
  const variables = new Map();
  objects.forEach((f) => {
    if (!cppType(f) || !f.variable) return;
    const i = fields.indexOf(f);
    if (variables.has(f.variable)) {
      report.error(`fields[${i}].variable`, `la variable ${f.variable} ya la usa fields[${variables.get(f.variable)}]: cada campo declara la suya (para mostrar un valor en dos lugares usa dos variables y asígnalas igual en el sketch)`);
    } else variables.set(f.variable, i);
  });
  // NAV_BAR
  const navs = objects.filter((f) => f.type === 'NAV_BAR');
  if (navs.length > 1) report.error('fields', 'sólo puede haber una NAV_BAR');
  if (navs.length && pages.length < 2) report.warn(`fields[${fields.indexOf(navs[0])}]`, 'NAV_BAR con una sola página: no hay a dónde navegar');
  // MENU: reglas del Designer (designer-menu.js validate).
  const menus = objects.filter((f) => f.type === 'MENU' && Array.isArray(f.nodes));
  if (menus.length) {
    const { menu } = designerModules();
    const copy = JSON.parse(JSON.stringify(menus));
    const result = menu.validate(copy, objects.filter((f) => f.type !== 'MENU'));
    const where = (text) => {
      const m = menus.find((x) => text.startsWith(`${x.name || x.id}`));
      return m ? `fields[${fields.indexOf(m)}] (${m.id})` : 'fields';
    };
    result.errors.forEach((text) => report.error(where(text), text));
    result.warnings.forEach((text) => report.warn(where(text), text));
  }
  // PixelMaps
  const maps = Array.isArray(project.pixelMaps) ? project.pixelMaps : [];
  if (maps.length > spec.LIMITS.pixelMaps) report.error('pixelMaps', `máximo ${spec.LIMITS.pixelMaps} capas`);
  maps.forEach((map, i) => {
    const where = `pixelMaps[${i}]`;
    if (!map || typeof map !== 'object') { report.error(where, 'debe ser un objeto'); return; }
    checkProps(map, spec.PIXELMAP, where, report, { allow: ['key'] });
    if (Number.isInteger(map.page) && map.page >= pages.length) report.error(`${where}.page`, `la página ${map.page} no existe`);
    const colors = new Set();
    (Array.isArray(map.pixels) ? map.pixels : []).forEach((p, k) => {
      if (!p || !Number.isInteger(p.x) || !Number.isInteger(p.y) || p.x < 0 || p.y < 0) { report.error(`${where}.pixels[${k}]`, 'cada píxel es {x, y, color} con x, y enteros ≥ 0 relativos al origen'); return; }
      if (map.x + p.x >= spec.SCREEN.width || map.y + p.y >= spec.SCREEN.height) report.warn(`${where}.pixels[${k}]`, 'fuera de la pantalla');
      if (!Number.isInteger(p.color) || p.color < 0 || p.color > 0xFFFF) report.error(`${where}.pixels[${k}].color`, 'color no válido');
      colors.add(p.color);
    });
    if (colors.size > 1) report.warn(where, `una capa PIXEL tiene un solo color (hay ${colors.size}); usa una capa por color`);
  });
  return report;
}

module.exports = { validateProject, designerModules, cppType, SYMBOL };

// Normalización de un proyecto .jwhmi escrito a mano (o por un agente).
//
// Acepta la forma corta documentada en agent/GUIA_AGENTES.md y devuelve un
// proyecto JWPLC_HMI_PROJECT v1 completo, como el que guarda el Designer:
//   - format/version/target, display y navigationBar por defecto;
//   - páginas como texto o con `fields` anidados; ids 0..N-1;
//   - colores como número, "0xF800", "#FF8800" o nombre ("RED");
//   - valores por defecto de cada tipo (spec.cjs), ids y variables únicos;
//   - nodos de MENU anidados con `children`;
//   - ICON: tamaño disponible más cercano y bool/uint8_t según sus estados;
//   - layerOrder por orden del arreglo (el último queda encima).
// Nunca lanza: lo que no puede arreglar lo deja para validate.cjs.
'use strict';

const spec = require('./spec.cjs');
const icons = require('./icons.cjs');

const clone = (value) => JSON.parse(JSON.stringify(value === undefined ? null : value));
const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

// --- Colores -----------------------------------------------------------------

function rgb888To565(r, g, b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

// Devuelve un número RGB565, 'TRANSPARENT' o el valor original si no se entiende.
function parseColor(value) {
  if (typeof value === 'number') return value;
  if (typeof value !== 'string') return value;
  const text = value.trim();
  const upper = text.toUpperCase().replace(/^TFT_/, '');
  if (upper === 'TRANSPARENT') return 'TRANSPARENT';
  if (Object.prototype.hasOwnProperty.call(spec.NAMED_COLORS, upper)) return spec.NAMED_COLORS[upper];
  if (/^0x[0-9a-f]{1,4}$/i.test(text)) return parseInt(text, 16);
  let m = /^#([0-9a-f]{6})$/i.exec(text);
  if (m) {
    const n = parseInt(m[1], 16);
    return rgb888To565((n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF);
  }
  m = /^#([0-9a-f])([0-9a-f])([0-9a-f])$/i.exec(text);
  if (m) return rgb888To565(parseInt(m[1] + m[1], 16), parseInt(m[2] + m[2], 16), parseInt(m[3] + m[3], 16));
  if (/^\d+$/.test(text)) return Number(text);
  return value;
}

function convertColors(object, props) {
  for (const [key, def] of Object.entries(props)) {
    if (def.kind === 'color' && object[key] !== undefined) object[key] = parseColor(object[key]);
  }
}

function applyDefaults(object, props) {
  for (const [key, def] of Object.entries(props)) {
    if (object[key] === undefined && def.default !== undefined) object[key] = clone(def.default);
  }
}

// --- Identificadores ---------------------------------------------------------

function symbolize(text, fallback) {
  const cleaned = String(text || '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/[^A-Za-z0-9_]/g, '_');
  if (!cleaned) return fallback;
  return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
}

function uniqueName(base, used) {
  let candidate = base;
  let k = 2;
  while (used.has(candidate)) candidate = `${base}_${k++}`;
  used.add(candidate);
  return candidate;
}

// --- Partes ------------------------------------------------------------------

function normalizePages(project) {
  const fromPages = [];
  let pages = Array.isArray(project.pages) && project.pages.length ? project.pages : [{ name: 'Principal' }];
  pages = pages.map((page, index) => {
    const p = typeof page === 'string' ? { name: page } : (isObject(page) ? { ...page } : { name: `Página ${index + 1}` });
    if (p.id === undefined) p.id = index;
    if (p.color !== undefined) p.color = parseColor(p.color);
    if (Array.isArray(p.fields)) {
      p.fields.forEach((field) => fromPages.push({ field, pageId: p.id }));
    }
    delete p.fields;
    return p;
  });

  // Ids contiguos 0..N-1 (como designer-project.js normalizePageIds).
  const sorted = pages.slice().sort((a, b) => Number(a.id) - Number(b.id));
  const remap = new Map(sorted.map((page, index) => [Number(page.id), index]));
  project.pages = sorted.map((page) => ({ ...page, id: remap.get(Number(page.id)) }));

  const fields = Array.isArray(project.fields) ? project.fields.slice() : [];
  fromPages.forEach(({ field, pageId }) => {
    fields.push(isObject(field) ? { ...field, page: Number(pageId) } : field);
  });
  project.fields = fields.map((field) => {
    if (!isObject(field) || field.page === undefined || !remap.has(Number(field.page))) return field;
    return { ...field, page: remap.get(Number(field.page)) };
  });
  if (Array.isArray(project.pixelMaps)) {
    project.pixelMaps = project.pixelMaps.map((map) => (isObject(map) && remap.has(Number(map.page || 0)) ? { ...map, page: remap.get(Number(map.page || 0)) } : map));
  }
}

function flattenMenuNodes(nodes, parent, out, usedKeys) {
  (Array.isArray(nodes) ? nodes : []).forEach((raw) => {
    if (!isObject(raw)) { out.push(raw); return; }
    const node = { ...raw };
    const children = node.children;
    delete node.children;
    if (node.type) node.type = String(node.type).toUpperCase();
    if (node.key === undefined || node.key === null || node.key === '') node.key = uniqueName(`n${out.length + 1}`, usedKeys);
    else usedKeys.add(String(node.key));
    if (parent !== undefined && node.parent === undefined) node.parent = parent;
    out.push(node);
    if (Array.isArray(children)) flattenMenuNodes(children, node.key, out, usedKeys);
  });
}

function normalizeMenu(field) {
  field.colors = { ...spec.MENU_COLORS, ...(isObject(field.colors) ? field.colors : {}) };
  for (const key of Object.keys(field.colors)) field.colors[key] = parseColor(field.colors[key]);
  const usedKeys = new Set((Array.isArray(field.nodes) ? field.nodes : []).filter((n) => isObject(n) && n.key).map((n) => String(n.key)));
  const flat = [];
  flattenMenuNodes(field.nodes, undefined, flat, usedKeys);
  field.nodes = flat.map((node) => {
    if (!isObject(node)) return node;
    const n = { ...node };
    if (n.parent === undefined || n.parent === '') n.parent = null;
    applyDefaults(n, spec.MENU_NODE);
    if (n.valueType === undefined) n.valueType = (spec.MENU_KINDS[n.type] || [''])[0];
    if (n.valueType === 'int32' && node.decimals === undefined) n.decimals = 0;
    return n;
  });
}

function navDefaults(field, raw) {
  const vertical = field.position === 'left' || field.position === 'right';
  if (field.width === undefined) field.width = vertical ? 24 : 320;
  if (vertical && raw.height === undefined) field.height = 170;
  if (field.x === undefined) field.x = field.position === 'right' ? spec.SCREEN.width - field.width - 2 : (field.position === 'left' ? 2 : 0);
  if (field.y === undefined) field.y = field.position === 'top' ? 2 : (field.position === 'bottom' ? spec.SCREEN.height - field.height - 1 : 0);
  field.orientation = vertical ? 'vertical' : 'horizontal';
}

function normalizeField(raw, index, ctx) {
  if (!isObject(raw)) return raw;
  const field = { ...raw };
  if (typeof field.type === 'string') field.type = field.type.trim().toUpperCase();
  const def = spec.TYPES[field.type];
  if (!def) return field;

  if (field.type === 'NAV_BAR' && !field.position) field.position = 'bottom';
  if (field.type === 'NAV_BAR' && field.preset === 'counter' && field.height === undefined) field.height = 12;
  if (field.type === 'NAV_BAR' && field.preset === 'counter' && field.width === undefined) field.width = 36;
  convertColors(field, def.props);
  if (field.type === 'VALUE' && ['DURATION', 'DATETIME'].includes(field.valueKind) && field.decimalDigits === undefined) field.decimalDigits = 0;
  applyDefaults(field, def.props);
  delete field.key; // el Designer asigna las claves al abrir

  if (field.type === 'NAV_BAR') {
    navDefaults(field, raw);
    field.key = 'NAV_BAR';
    return field;
  }

  // Ids y variables escritos a mano se respetan (validate.cjs avisa si se repiten);
  // los generados evitan todos los existentes.
  const prefix = field.type === 'MENU' ? 'MENU' : `FIELD_${field.type}`;
  field.id = field.id ? String(field.id) : uniqueName(`${prefix}_${index + 1}`, ctx.ids);
  if (!field.name) field.name = field.type === 'RAW_TEXT' ? String(field.text || field.id).slice(0, 24) : field.id;

  if (spec.FIELD_TYPES.includes(field.type)) {
    const clock = field.type === 'VALUE' && field.valueKind === 'DATETIME';
    if (!field.variable && !clock) field.variable = uniqueName(symbolize(field.id.replace(/^FIELD_/, '').toLowerCase(), `var${index + 1}`), ctx.variables);
    if (!field.variable && clock) field.variable = 'reloj';
  }

  if (field.type === 'BOOL') {
    const pilot = field.boolStyle === 'PILOT';
    if (pilot) field.stateColors = true;
    if (pilot || field.stateColors) {
      const bg = Number.isFinite(Number(field.backgroundColor)) ? Number(field.backgroundColor) : 0;
      if (field.falseColor === undefined) field.falseColor = pilot ? 0xF800 : field.valueColor;
      if (field.trueColor === undefined) field.trueColor = 0x07E0;
      if (field.falseBackground === undefined) field.falseBackground = bg;
      if (field.trueBackground === undefined) field.trueBackground = bg;
    }
    // designer-bool.js ensureBoolState
    field.capacity = Math.max(1, String(field.falseText).length, String(field.trueText).length);
    field.preview = field.boolValue ? field.trueText : field.falseText;
  }
  if (field.type === 'BAR') {
    if (!raw.label && field.label === '') field.label = field.barLabel;
    // designer-bar.js applyFirmwareCaps: el fondo del campo es el de la pista.
    field.backgroundColor = Number(field.trackColor) & 0xFFFF;
  }
  // Muestra que ocupa todo lo reservado: en la vista previa se ve el ancho real del campo.
  if (field.type === 'VALUE' && field.preview === undefined) field.preview = valueSample(field);
  if (field.type === 'TEXT' && field.preview === undefined) field.preview = 'X'.repeat(Math.max(1, Math.min(39, Number(field.capacity) || 1)));
  if (field.type === 'MENU') normalizeMenu(field);
  if (field.type === 'ICON') normalizeIcon(field, raw, ctx);
  return field;
}

// Como app.js addIconField: tamaño que exista y uint8_t con más de 2 estados.
function normalizeIcon(field, raw, ctx) {
  field.iconId = String(field.iconId || '').trim();
  const info = icons.iconInfo(field.iconId, ctx.iconAssets);
  if (!info) return;
  if (!raw.name) field.name = `${info.name} (${field.id})`;
  if (info.sizes.length) field.iconSize = icons.resolveSize(info, field.iconSize);
  if (field.iconVarType === undefined) field.iconVarType = info.stateNames.length > 2 ? 'uint8' : 'bool';
}

function valueSample(field) {
  if (field.valueKind === 'DATETIME') return '';
  const digits = Math.max(1, Math.min(9, Number(field.integerDigits) || 1));
  if (field.valueKind === 'DURATION') return field.durationUnit === 'MS' ? '3723000' : '3723';
  const decimals = Math.max(0, Math.min(6, Number(field.decimalDigits) || 0));
  const integer = '123456789'.slice(0, digits);
  return `${field.signedValue ? '-' : ''}${integer}${decimals ? `.${'5'.repeat(decimals)}` : ''}`;
}

function assignLayerOrder(fields) {
  const layered = fields.filter((f) => isObject(f) && f.type !== 'NAV_BAR');
  const n = layered.length;
  layered.forEach((field, i) => {
    if (field.layerOrder === undefined) field.layerOrder = n - i;
  });
}

function navigationBarConfig(fields) {
  const nav = fields.find((f) => isObject(f) && f.type === 'NAV_BAR');
  if (!nav) return { enabled: false, preset: 'industrial-color', position: 'bottom', height: 22, showIndicator: true, showArrows: true };
  const cfg = { enabled: !nav.hidden, showIndicator: true, showArrows: true };
  for (const key of Object.keys(spec.TYPES.NAV_BAR.props)) {
    if (['type', 'name', 'key', 'label', 'global', 'hidden', 'locked', 'layerOrder'].includes(key)) continue;
    if (nav[key] !== undefined) cfg[key] = nav[key];
  }
  return cfg;
}

function normalizePixelMap(raw) {
  if (!isObject(raw)) return raw;
  const map = { ...raw };
  applyDefaults(map, spec.PIXELMAP);
  if (Array.isArray(map.pixels)) {
    map.pixels = map.pixels.map((p) => (isObject(p) ? { ...p, color: parseColor(p.color === undefined ? 0xFFFF : p.color) } : p));
  }
  return map;
}

// options.savedAt: fecha fija (tests); por defecto, ahora.
function normalizeProject(input, options = {}) {
  const project = isObject(input) ? clone(input) : {};
  delete project.$schema;
  normalizePages(project);

  const explicit = (key) => new Set(project.fields.filter((f) => isObject(f) && f[key]).map((f) => String(f[key])));
  const iconAssets = isObject(project.iconAssets) ? project.iconAssets : undefined;
  const ctx = { ids: explicit('id'), variables: explicit('variable'), iconAssets };
  const fields = project.fields.map((field, index) => normalizeField(field, index, ctx));
  assignLayerOrder(fields);

  const display = { ...(isObject(project.display) ? project.display : {}) };
  applyDefaults(display, spec.DISPLAY);

  return {
    format: project.format || spec.PROJECT.format.default,
    version: project.version === undefined ? spec.PROJECT.version.default : project.version,
    target: { ...spec.TARGET },
    projectName: String(project.projectName || spec.PROJECT.projectName.default).slice(0, spec.LIMITS.nameLength),
    savedAt: options.savedAt || project.savedAt || new Date().toISOString(),
    activePage: Number.isInteger(project.activePage) && project.activePage < project.pages.length ? project.activePage : 0,
    navigationBar: navigationBarConfig(fields),
    display,
    pages: project.pages,
    fields,
    declarativeOnly: true,
    pixelMaps: (Array.isArray(project.pixelMaps) ? project.pixelMaps : []).map(normalizePixelMap),
    ...(iconAssets ? { iconAssets } : {})
  };
}

module.exports = { normalizeProject, parseColor, symbolize };

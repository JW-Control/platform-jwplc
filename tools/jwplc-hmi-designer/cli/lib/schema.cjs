// Genera, desde spec.cjs, el esquema JSON (schema/jwhmi.schema.json) y la
// referencia en Markdown (agent/REFERENCIA_JWHMI.md). No editar esos archivos
// a mano: `node cli/jwhmi.cjs schema` los reescribe.
'use strict';

const spec = require('./spec.cjs');
const icons = require('./icons.cjs');

const COLOR_SCHEMA = {
  description: 'RGB565 (0..65535), "0xF800", "#RRGGBB", "#RGB" o un nombre: ' + Object.keys(spec.NAMED_COLORS).join(', '),
  oneOf: [
    { type: 'integer', minimum: 0, maximum: 65535 },
    { type: 'string', pattern: '^(0[xX][0-9a-fA-F]{1,4}|#[0-9a-fA-F]{6}|#[0-9a-fA-F]{3}|(TFT_)?[A-Za-z]+)$' }
  ]
};

function propSchema(def) {
  const out = { description: def.desc };
  switch (def.kind) {
    case 'int': Object.assign(out, { type: 'integer' }); break;
    case 'number': Object.assign(out, { type: 'number' }); break;
    case 'bool': Object.assign(out, { type: 'boolean' }); break;
    case 'string': Object.assign(out, def.default === null ? { type: ['string', 'null'] } : { type: 'string' }); break;
    case 'symbol': Object.assign(out, { type: 'string', pattern: '^([A-Za-z_][A-Za-z0-9_]*)?$' }); break;
    case 'color': Object.assign(out, COLOR_SCHEMA, { description: `${def.desc} ${COLOR_SCHEMA.description}` }); break;
    case 'enum': out.enum = def.enum; break;
    case 'object': out.type = def.default === null ? ['object', 'null'] : 'object'; break;
    case 'array': out.type = 'array'; break;
    default: break;
  }
  if (def.min !== undefined) out.minimum = def.min;
  if (def.max !== undefined) out.maximum = def.max;
  const dflt = def.default !== undefined ? def.default : def.doc;
  if (dflt !== undefined && def.kind !== 'array' && def.kind !== 'object') out.default = dflt;
  return out;
}

function objectSchema(props, extra = {}) {
  const properties = {};
  const required = [];
  for (const [key, def] of Object.entries(props)) {
    properties[key] = propSchema(def);
    if (def.required) required.push(key);
  }
  return { type: 'object', properties, ...(required.length ? { required } : {}), ...extra };
}

function buildSchema() {
  const node = objectSchema(spec.MENU_NODE);
  node.properties.children = { type: 'array', items: { $ref: '#/definitions/menuNode' }, description: spec.MENU_NODE.children.desc };
  node.properties.parent = { type: ['string', 'null'], description: spec.MENU_NODE.parent.desc };

  const fieldTypes = Object.keys(spec.TYPES);
  const field = {
    type: 'object',
    required: ['type'],
    properties: { type: { enum: fieldTypes, description: 'Tipo de objeto.' } },
    allOf: fieldTypes.map((type) => {
      const then = objectSchema(spec.TYPES[type].props);
      delete then.type;
      then.properties.type = { const: type };
      if (type === 'MENU') {
        then.properties.nodes = { type: 'array', items: { $ref: '#/definitions/menuNode' }, minItems: 1, description: spec.TYPES.MENU.props.nodes.desc };
        then.properties.colors = { type: 'object', description: spec.TYPES.MENU.props.colors.desc,
          properties: Object.fromEntries(Object.keys(spec.MENU_COLORS).map((k) => [k, { ...COLOR_SCHEMA, default: spec.MENU_COLORS[k] }])) };
      }
      return { if: { properties: { type: { const: type } } }, then };
    })
  };

  const page = objectSchema(spec.PAGE);
  page.properties.fields = { type: 'array', items: { $ref: '#/definitions/field' }, description: spec.PAGE.fields.desc };
  const pixelMap = objectSchema(spec.PIXELMAP);
  pixelMap.properties.pixels = { type: 'array', description: spec.PIXELMAP.pixels.desc,
    items: { type: 'object', required: ['x', 'y'], properties: { x: { type: 'integer', minimum: 0 }, y: { type: 'integer', minimum: 0 }, color: COLOR_SCHEMA } } };

  const project = objectSchema(spec.PROJECT);
  project.properties.pages = { type: 'array', minItems: 1, maxItems: spec.LIMITS.pages, description: spec.PROJECT.pages.desc,
    items: { oneOf: [{ type: 'string' }, { $ref: '#/definitions/page' }] } };
  project.properties.fields = { type: 'array', maxItems: spec.LIMITS.fields, items: { $ref: '#/definitions/field' }, description: spec.PROJECT.fields.desc };
  project.properties.pixelMaps = { type: 'array', maxItems: spec.LIMITS.pixelMaps, items: { $ref: '#/definitions/pixelMap' }, description: spec.PROJECT.pixelMaps.desc };
  project.properties.display = { $ref: '#/definitions/display' };
  project.properties.format = { const: 'JWPLC_HMI_PROJECT', description: spec.PROJECT.format.desc };
  project.properties.version = { const: 1, description: spec.PROJECT.version.desc };

  return {
    $schema: 'http://json-schema.org/draft-07/schema#',
    $id: 'https://github.com/JW-Control/platform-jwplc/tools/jwplc-hmi-designer/schema/jwhmi.schema.json',
    title: 'JWPLC HMI Designer · proyecto .jwhmi (JWPLC_HMI_PROJECT v1)',
    description: 'Generado desde cli/lib/spec.cjs (node cli/jwhmi.cjs schema). Acepta la forma corta para agentes: ' +
      'la CLI la normaliza al formato completo del Designer. Pantalla ST7789 320×170 RGB565.',
    ...project,
    definitions: { field, menuNode: node, page, display: objectSchema(spec.DISPLAY), pixelMap }
  };
}

// --- Referencia Markdown -------------------------------------------------------

const cell = (s) => String(s ?? '').replace(/\|/g, '\\|').replace(/\n/g, ' ');

function fmtDefault(def) {
  const value = def.default !== undefined ? def.default : def.doc;
  if (value === undefined || value === null) return '';
  if (def.kind === 'color' && typeof value === 'number') return `\`0x${value.toString(16).toUpperCase().padStart(4, '0')}\``;
  if (Array.isArray(value)) return value.length ? `\`${JSON.stringify(value)}\`` : '`[]`';
  return `\`${JSON.stringify(value)}\``;
}

function fmtKind(def) {
  if (def.kind === 'enum') return def.enum.map((v) => `\`${JSON.stringify(v)}\``).join(' · ');
  const range = def.min !== undefined || def.max !== undefined ? ` ${def.min ?? ''}..${def.max ?? ''}` : '';
  return { int: 'entero', number: 'número', bool: 'bool', string: 'texto', color: 'color', symbol: 'identificador C++', object: 'objeto', array: 'arreglo' }[def.kind] + range;
}

function table(props) {
  const rows = ['| Propiedad | Tipo / valores | Por defecto | Descripción |', '|---|---|---|---|'];
  for (const [key, def] of Object.entries(props)) {
    if (def.internal) continue;
    rows.push(`| \`${key}\`${def.required ? ' **(obligatoria)**' : ''} | ${cell(fmtKind(def))} | ${fmtDefault(def)} | ${cell(def.desc)} |`);
  }
  return rows.join('\n');
}

function buildReference() {
  const out = [
    '<!-- Generado por `node tools/jwplc-hmi-designer/cli/jwhmi.cjs schema` desde cli/lib/spec.cjs. No editar a mano. -->',
    '# Referencia del formato .jwhmi',
    '',
    'Proyecto del JWPLC HMI Designer (`JWPLC_HMI_PROJECT` v1) para la TFT ST7789 de 320×170 px (RGB565) del JWPLC Basic.',
    'Cómo usarlo paso a paso: [GUIA_AGENTES.md](GUIA_AGENTES.md). Esquema JSON: [`schema/jwhmi.schema.json`](../schema/jwhmi.schema.json).',
    '',
    'Las propiedades internas que el Designer guarda pero que no hace falta escribir no aparecen aquí; la CLI las completa.',
    '',
    '## Límites',
    '',
    `- Pantalla: ${spec.SCREEN.width}×${spec.SCREEN.height} px, origen arriba a la izquierda.`,
    `- Páginas: 1 a ${spec.LIMITS.pages}. Objetos: ${spec.LIMITS.fields} (contando la NAV_BAR). Capas PIXEL: ${spec.LIMITS.pixelMaps}.`,
    `- MENU: uno por página, ${spec.LIMITS.menus} en el proyecto, ${spec.LIMITS.menuNodes} nodos y ${spec.LIMITS.menuDepth} niveles por MENU.`,
    '',
    '## Colores',
    '',
    'Número RGB565 (`63488`), texto hexadecimal (`"0xF800"`), `"#RRGGBB"` / `"#RGB"` (se convierte a RGB565) o un nombre:',
    '',
    Object.entries(spec.NAMED_COLORS).map(([k, v]) => `\`${k}\` = \`0x${v.toString(16).toUpperCase().padStart(4, '0')}\``).join(' · '),
    '',
    '## Proyecto',
    '',
    table(spec.PROJECT),
    '',
    '## Páginas (`pages[]`)',
    '',
    'Cada página puede ser un texto (`"Principal"`) o un objeto:',
    '',
    table(spec.PAGE),
    '',
    '## Pantalla IDLE y arranque (`display`)',
    '',
    table(spec.DISPLAY),
    '',
    '## Objetos (`fields[]`)',
    '',
    'Cada objeto tiene `type` y las propiedades de su tipo. Las coordenadas son de la esquina superior izquierda.',
    ''
  ];
  for (const [type, def] of Object.entries(spec.TYPES)) {
    if (spec.SHAPE_TYPES.includes(type) && type !== 'LINE') continue;
    if (type === 'LINE') {
      out.push('### Figuras: `LINE`, `RECT`, `ELLIPSE`, `TRIANGLE`, `POLYGON`', '');
      spec.SHAPE_TYPES.forEach((t) => out.push(`- \`${t}\`: ${spec.TYPES[t].desc}`));
      out.push('', 'Se dibujan una vez al entrar a la página (gráfica estática).', '', table(def.props), '');
      continue;
    }
    out.push(`### \`${type}\``, '', def.desc, '', table(def.props), '');
    if (type === 'MENU') {
      out.push('#### Nodos de MENU (`nodes[]`)', '', table(spec.MENU_NODE), '',
        'Colores del MENU (`colors`), por defecto:', '',
        Object.entries(spec.MENU_COLORS).map(([k, v]) => `\`${k}\` = \`0x${v.toString(16).toUpperCase().padStart(4, '0')}\``).join(' · '), '');
    }
    if (type === 'ICON') {
      out.push('#### Iconos del catálogo (`iconId`)', '',
        'El estado `k` de la variable muestra la imagen `k` (bool: 0 = false, 1 = true). También: `jwhmi icons [categoría]`.', '',
        '| `iconId` | Nombre | Tamaños (px) | Estados (0..N-1) |', '|---|---|---|---|',
        ...icons.listIcons().map((i) => `| \`${i.id}\` | ${cell(i.name)} | ${i.sizes.join(', ')} | ${cell(i.stateNames.join(' · '))} |`), '');
    }
  }
  out.push('## Capas de píxeles (`pixelMaps[]`)', '', table(spec.PIXELMAP), '',
    '## Fuentes de `RAW_TEXT`', '', spec.FONTS.map((f) => `\`${f}\``).join(' · '), '');
  return out.join('\n');
}

module.exports = { buildSchema, buildReference };

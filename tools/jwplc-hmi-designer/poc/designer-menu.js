(() => {
  'use strict';

  // A15-8: objeto MENU (árbol de menús/submenús para ver y configurar
  // variables con la botonera). Firmware: JWPLC_UI_Menu.h / JWPLC_UI_Menu.cpp.
  //
  // - El pintor (paint/layout) replica drawFull()/drawRow()/drawHeader() del
  //   firmware línea por línea: cambiar uno obliga a cambiar el otro
  //   (tests/menu-smoke.cjs compara el lienzo con la simulación del firmware).
  // - El codegen es puro: generate(menus, context) no lee el DOM.
  // - El modelo se trata como inmutable (copy-on-write de nodes/colors) porque
  //   el historial de app.js copia los campos de forma superficial.

  const MAX_MENUS = 4;
  const MAX_NODES = 64;
  const MAX_DEPTH = 3;
  const MAX_VISIBLE_ROWS = 16;
  const ROW_PAD_X = 3;
  const ROW_PAD_Y = 2;
  const SCREEN_W = 320;
  const SCREEN_H = 170;

  const NODE_TYPES = {
    SUBMENU: { label: 'Submenú', icon: '▸', kinds: [] },
    VIEW: { label: 'Ver', icon: '◉', kinds: ['float', 'int32', 'bool'] },
    EDIT_NUM: { label: 'Editar número', icon: '±', kinds: ['float', 'int32'] },
    EDIT_BOOL: { label: 'Editar ON/OFF', icon: '⏻', kinds: ['bool'] },
    // Acción: OK vuelve a la pantalla IDLE (JWPLC_UIMenuGoIdle). Sin variable.
    GO_IDLE: { label: 'Ir al inicio', icon: '⏎', kinds: [] }
  };

  // Ítems sin variable vinculada.
  const hasVariable = (n) => n.type !== 'SUBMENU' && n.type !== 'GO_IDLE';

  const DEFAULT_COLORS = Object.freeze({
    text: 0xFFFF,
    background: 0x0000,
    value: 0x07FF,
    selectText: 0x0000,
    selectBackground: 0x07FF,
    editText: 0x0000,
    editBackground: 0xFD20,
    title: 0x0000,
    titleBackground: 0xFFFF,
    frame: 0xFFFF
  });

  // De arriba hacia abajo, como se ve el menú en la TFT.
  const COLOR_LABELS = [
    ['titleBackground', 'Título · fondo'],
    ['title', 'Título · texto'],
    ['text', 'Ítem · texto'],
    ['value', 'Ítem · valor'],
    ['selectBackground', 'Selección · fondo'],
    ['selectText', 'Selección · texto'],
    ['editBackground', 'Edición · fondo'],
    ['editText', 'Edición · texto'],
    ['background', 'Fondo del menú'],
    ['frame', 'Marco']
  ];

  // ---------------------------------------------------------------------------
  // Utilidades
  // ---------------------------------------------------------------------------

  const clamp = (v, lo, hi) => Math.min(hi, Math.max(lo, v));
  const num = (v, fallback = 0) => (Number.isFinite(Number(v)) ? Number(v) : fallback);

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim()
      .normalize('NFD').replace(/[̀-ͯ]/g, '')
      .replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  // Literal C++ con los bytes cp437 que dibuja la TFT (tildes, ñ, °; Alpha15 I3).
  function cppString(value) {
    return `"${window.JWPLCCp437.cppEscape(value ?? '')}"`;
  }

  // Texto plano para #error (el compilador lo muestra tal cual).
  function cppPlainString(value) {
    return `"${String(value ?? '').replace(/\\/g, '\\\\').replace(/"/g, '\\"')}"`;
  }

  function cppFloat(value) {
    const n = num(value);
    const text = String(n);
    return /[.eE]/.test(text) ? `${text}f` : `${text}.0f`;
  }

  function hex565(value) {
    return `0x${(num(value) & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToCss(value) {
    const v = num(value) & 0xFFFF;
    const h = (x) => Math.round(x).toString(16).padStart(2, '0');
    return `#${h(((v >> 11) & 0x1F) * 255 / 31)}${h(((v >> 5) & 0x3F) * 255 / 63)}${h((v & 0x1F) * 255 / 31)}`;
  }

  function escapeHtml(text) {
    return String(text ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  }

  // ---------------------------------------------------------------------------
  // Modelo
  // ---------------------------------------------------------------------------

  let nodeSerial = 0;

  function newNodeKey(nodes) {
    const used = new Set((nodes || []).map((n) => n.key));
    let key;
    do {
      nodeSerial += 1;
      key = `n${Date.now().toString(36)}${nodeSerial}`;
    } while (used.has(key));
    return key;
  }

  function defaultNode(type, parent, nodes, serial) {
    const base = {
      key: newNodeKey(nodes),
      parent: parent || null,
      type,
      label: NODE_TYPES[type].label,
      variable: '',
      valueType: NODE_TYPES[type].kinds[0] || '',
      initial: 0,
      min: 0,
      max: 100,
      step: 1,
      decimals: 1,
      unit: '',
      falseText: 'OFF',
      trueText: 'ON'
    };
    if (type === 'SUBMENU') base.label = 'Submenú';
    if (type === 'VIEW') Object.assign(base, { label: 'Medición', variable: `medicion${serial}`, initial: 21.5, unit: 'C' });
    if (type === 'EDIT_NUM') Object.assign(base, { label: 'Consigna', variable: `consigna${serial}`, initial: 50, min: 0, max: 100, step: 0.5, unit: '%' });
    if (type === 'EDIT_BOOL') Object.assign(base, { label: 'Habilitar', variable: `habilitar${serial}`, initial: 0 });
    return base;
  }

  function createDefault(key, serial, existingFields = []) {
    const nodes = [];
    const consigna = defaultNode('EDIT_NUM', null, nodes, serial);
    nodes.push(consigna);
    const bomba = defaultNode('EDIT_BOOL', null, nodes, serial);
    Object.assign(bomba, { label: 'Bomba', variable: `bomba${serial}`, falseText: 'PARADA', trueText: 'MARCHA' });
    nodes.push(bomba);
    const avanzado = defaultNode('SUBMENU', null, nodes, serial);
    avanzado.label = 'Avanzado';
    nodes.push(avanzado);
    const retardo = defaultNode('EDIT_NUM', avanzado.key, nodes, serial);
    Object.assign(retardo, { label: 'Retardo', variable: `retardo${serial}`, valueType: 'int32', initial: 30, min: 0, max: 600, step: 5, decimals: 0, unit: 's' });
    nodes.push(retardo);

    const usedNames = new Set(existingFields.map((f) => f.variable).filter(Boolean));
    nodes.forEach((n) => {
      if (!n.variable) return;
      let candidate = n.variable;
      let suffix = 2;
      while (usedNames.has(candidate)) candidate = `${n.variable}_${suffix++}`;
      n.variable = candidate;
      usedNames.add(candidate);
    });

    return {
      key,
      type: 'MENU',
      name: `MENÚ ${serial}`,
      id: `MENU_${serial}`,
      title: 'Ajustes',
      x: 10,
      y: 20,
      width: 300,
      height: 124,
      textSize: 1,
      frame: true,
      colors: { ...DEFAULT_COLORS },
      nodes
    };
  }

  function ensureState(field) {
    if (!field || field.type !== 'MENU') return field;
    if (!Array.isArray(field.nodes)) field.nodes = [];
    if (!field.colors || typeof field.colors !== 'object') field.colors = { ...DEFAULT_COLORS };
    else if (Object.keys(DEFAULT_COLORS).some((k) => !Number.isFinite(Number(field.colors[k])))) {
      field.colors = { ...DEFAULT_COLORS, ...field.colors };
    }
    if (typeof field.title !== 'string') field.title = 'Menú';
    if (!Number.isFinite(Number(field.width))) field.width = 300;
    if (!Number.isFinite(Number(field.height))) field.height = 124;
    if (![1, 2].includes(Number(field.textSize))) field.textSize = 1;
    if (typeof field.frame !== 'boolean') field.frame = true;
    return field;
  }

  function childrenOf(nodes, parentKey) {
    return nodes.filter((n) => (n.parent || null) === (parentKey || null));
  }

  function nodeByKey(nodes, key) {
    return nodes.find((n) => n.key === key) || null;
  }

  // Nivel del menú en el que aparece el nodo (raíz = 1).
  function levelOf(nodes, node) {
    let level = 1;
    let p = node?.parent ? nodeByKey(nodes, node.parent) : null;
    while (p) {
      level += 1;
      p = p.parent ? nodeByKey(nodes, p.parent) : null;
    }
    return level;
  }

  // Orden DFS: padres antes que hijos (lo exige el firmware).
  function dfsOrder(nodes) {
    const out = [];
    const walk = (parentKey) => {
      childrenOf(nodes, parentKey).forEach((n) => {
        out.push(n);
        if (n.type === 'SUBMENU') walk(n.key);
      });
    };
    walk(null);
    return out;
  }

  function descendantsOf(nodes, key) {
    const out = new Set([key]);
    let grew = true;
    while (grew) {
      grew = false;
      nodes.forEach((n) => {
        if (n.parent && out.has(n.parent) && !out.has(n.key)) {
          out.add(n.key);
          grew = true;
        }
      });
    }
    return out;
  }

  // ---------------------------------------------------------------------------
  // Geometría y dibujo (espejo de JWPLC_UI_Menu.cpp)
  // ---------------------------------------------------------------------------

  function layout(field) {
    const ts = Number(field.textSize) >= 2 ? 2 : 1;
    const border = field.frame ? 1 : 0;
    const rowH = 8 * ts + 2 * ROW_PAD_Y;
    const hasTitle = String(field.title || '').length > 0;
    const headerH = hasTitle ? rowH : 0;
    const avail = num(field.height) - 2 * border - headerH;
    const rows = Math.min(MAX_VISIBLE_ROWS, avail > 0 ? Math.trunc(avail / rowH) : 0);
    return { ts, cw: 6 * ts, border, rowH, hasTitle, headerH, rows };
  }

  function glcdText(painter, text, x, y, ts, fg, bg) {
    const font = window.JWPLCGfxClassicFont;
    if (!font) return;
    let cx = x;
    for (const ch of String(text)) {
      const glyph = font.glyphFor(ch.charCodeAt(0));
      for (let col = 0; col < 6; col++) {
        const bits = col < 5 ? glyph[col] : 0;
        for (let row = 0; row < 8; row++) {
          const on = (bits >> row) & 1;
          const c = on ? fg : bg;
          if (ts === 1) painter.pixel(cx + col, y + row, c);
          else painter.fillRect(cx + col * ts, y + row * ts, ts, ts, c);
        }
      }
      cx += 6 * ts;
    }
  }

  function formatValue(node, value) {
    if (node.type === 'SUBMENU') return '>';
    if (node.type === 'GO_IDLE') return '<';
    if (node.valueType === 'bool') {
      const state = Boolean(num(value));
      const text = state ? node.trueText : node.falseText;
      return text ?? (state ? 'ON' : 'OFF');
    }
    let out = node.valueType === 'int32'
      ? String(Math.trunc(num(value)))
      : num(value).toFixed(clamp(Math.trunc(num(node.decimals, 1)), 0, 6));
    if (node.unit) out += ` ${node.unit}`;
    return out;
  }

  // Valor de prueba de cada variable: el del primer nodo que la usa.
  function previewValues(field) {
    const map = new Map();
    dfsOrder(field.nodes).forEach((n) => {
      if (n.type !== 'SUBMENU' && n.variable && !map.has(n.variable)) map.set(n.variable, num(n.initial));
    });
    return map;
  }

  // Estado de vista previa: el nivel del nodo elegido en el árbol.
  const uiState = new Map(); // field.key -> { nodeKey, editing }

  function previewState(field) {
    const ui = uiState.get(field.key) || {};
    const node = ui.nodeKey ? nodeByKey(field.nodes, ui.nodeKey) : null;
    const parentKey = node ? (node.parent || null) : null;
    const siblings = childrenOf(field.nodes, parentKey);
    const selected = node ? Math.max(0, siblings.indexOf(node)) : 0;
    const depth = parentKey ? levelOf(field.nodes, nodeByKey(field.nodes, parentKey)) + 1 : 1;
    const editing = Boolean(ui.editing && node && (node.type === 'EDIT_NUM' || node.type === 'EDIT_BOOL'));
    return { parentKey, selected, depth, editing, focused: true };
  }

  function paint(field, painter, state = null) {
    ensureState(field);
    const L = layout(field);
    const c = field.colors;
    const st = state || previewState(field);
    const values = previewValues(field);
    const x0 = num(field.x);
    const y0 = num(field.y);
    const W = num(field.width);
    const H = num(field.height);

    painter.fillRect(x0, y0, W, H, c.background);
    if (field.frame) painter.drawRect(x0, y0, W, H, c.frame);

    const siblings = childrenOf(field.nodes, st.parentKey);
    const count = siblings.length;
    let scroll = 0;
    if (L.rows > 0 && st.selected >= L.rows) scroll = st.selected - L.rows + 1;

    const x = x0 + L.border;
    const w = W - 2 * L.border;

    // drawHeader()
    if (L.hasTitle) {
      const y = y0 + L.border;
      painter.fillRect(x, y, w, L.rowH, c.titleBackground);
      const parent = st.parentKey ? nodeByKey(field.nodes, st.parentKey) : null;
      let text = `${st.depth > 1 ? '< ' : ''}${st.depth > 1 ? (parent?.label || '') : field.title}`.slice(0, 47);
      const up = scroll > 0;
      const down = scroll + L.rows < count;
      const arrowsW = (up || down) ? 2 * L.cw : 0;
      const maxChars = Math.max(0, Math.trunc((w - 2 * ROW_PAD_X - arrowsW) / L.cw));
      text = text.slice(0, maxChars);
      glcdText(painter, text, x + ROW_PAD_X, y + ROW_PAD_Y, L.ts, c.title, c.titleBackground);
      if (arrowsW > 0) {
        const arrows = `${up ? '\x1E' : ' '}${down ? '\x1F' : ' '}`;
        glcdText(painter, arrows, x + w - ROW_PAD_X - arrowsW, y + ROW_PAD_Y, L.ts, c.title, c.titleBackground);
      }
    }

    // drawRow()
    for (let row = 0; row < L.rows; row++) {
      const pos = scroll + row;
      const node = siblings[pos] || null;
      const y = y0 + L.border + L.headerH + row * L.rowH;
      const selected = node && pos === st.selected;
      const editingRow = selected && st.editing;
      let bg = c.background;
      let fg = c.text;
      let vfg = c.value;
      if (editingRow) {
        bg = c.editBackground; fg = c.editText; vfg = c.editText;
      } else if (selected && st.focused) {
        bg = c.selectBackground; fg = c.selectText; vfg = c.selectText;
      }
      painter.fillRect(x, y, w, L.rowH, bg);
      if (selected && !st.focused && !editingRow) painter.drawRect(x, y, w, L.rowH, c.selectBackground);
      if (!node) continue;

      let value = formatValue(node, values.get(node.variable)).slice(0, 31);
      if (editingRow && node.valueType !== 'bool') value = `<${value}>`.slice(0, 31);
      const maxValueChars = Math.trunc((w - 2 * ROW_PAD_X) / L.cw);
      if (value.length > maxValueChars) value = value.slice(0, Math.max(0, maxValueChars));
      const valueW = value.length * L.cw;
      let label = String(node.label || '').slice(0, 39);
      const labelChars = Math.max(0, Math.trunc((w - 2 * ROW_PAD_X - valueW - (valueW > 0 ? L.cw : 0)) / L.cw));
      label = label.slice(0, labelChars);
      glcdText(painter, label, x + ROW_PAD_X, y + ROW_PAD_Y, L.ts, fg, bg);
      if (valueW > 0) glcdText(painter, value, x + w - ROW_PAD_X - valueW, y + ROW_PAD_Y, L.ts, vfg, bg);
    }
  }

  // Render aislado a un framebuffer RGB565 (pruebas de paridad).
  function renderToArray(field, state = null) {
    const fb = new Uint16Array(SCREEN_W * SCREEN_H);
    const px = (x, y, c) => { if (x >= 0 && y >= 0 && x < SCREEN_W && y < SCREEN_H) fb[y * SCREEN_W + x] = c; };
    paint(field, {
      pixel: px,
      fillRect: (x, y, w, h, c) => { for (let j = y; j < y + h; j++) for (let i = x; i < x + w; i++) px(i, j, c); },
      drawRect: (x, y, w, h, c) => {
        for (let i = x; i < x + w; i++) { px(i, y, c); px(i, y + h - 1, c); }
        for (let j = y; j < y + h; j++) { px(x, j, c); px(x + w - 1, j, c); }
      }
    }, state);
    return fb;
  }

  // ---------------------------------------------------------------------------
  // Validación y codegen (puro)
  // ---------------------------------------------------------------------------

  const CPP_TYPE = { float: 'float', int32: 'int32_t', bool: 'bool' };

  function fieldVariableTypes(fields) {
    const map = new Map();
    fields.forEach((f) => {
      if (!f.variable) return;
      if (f.type === 'VALUE' || f.type === 'BAR') map.set(f.variable, 'float');
      else if (f.type === 'BOOL') map.set(f.variable, 'bool');
      else if (f.type === 'ICON') map.set(f.variable, f.iconVarType === 'uint8' ? 'uint8' : 'bool');
      else if (f.type === 'TEXT') map.set(f.variable, 'char[]');
    });
    return map;
  }

  function validate(menus, fields = []) {
    const errors = [];
    const warnings = [];
    if (menus.length > MAX_MENUS) errors.push(`Hay ${menus.length} MENU; el firmware admite ${MAX_MENUS}.`);
    const pagesSeen = new Map();
    const fieldVars = fieldVariableTypes(fields);
    const menuVars = new Map();
    const ids = new Set(fields.map((f) => f.id));

    menus.forEach((m) => {
      ensureState(m);
      const name = m.name || m.id;
      const page = Number(m.page || 0);
      if (pagesSeen.has(page)) errors.push(`${name}: la página ya tiene el MENU ${pagesSeen.get(page)} (uno por página).`);
      pagesSeen.set(page, name);
      if (ids.has(m.id) && fields.find((f) => f.id === m.id && f !== m)) errors.push(`${name}: el ID ${m.id} ya lo usa otro objeto.`);
      if (m.nodes.length === 0) errors.push(`${name}: el árbol está vacío.`);
      if (m.nodes.length > MAX_NODES) errors.push(`${name}: ${m.nodes.length} nodos; máximo ${MAX_NODES}.`);
      const L = layout(m);
      if (L.rows < 1) errors.push(`${name}: el alto no alcanza para una fila (mínimo ${2 * L.border + L.headerH + L.rowH} px).`);
      if (num(m.x) < 0 || num(m.y) < 0 || num(m.x) + num(m.width) > SCREEN_W || num(m.y) + num(m.height) > SCREEN_H) {
        warnings.push(`${name}: sale de la pantalla (320×170).`);
      }

      // Alpha15 I3: tildes, ñ y ° sí existen en la fuente GLCD; sólo se avisa
      // de lo que no tiene glifo (se dibuja sin tilde o como '?').
      const noGlyphWarning = (where, text) => {
        const missing = window.JWPLCCp437?.unsupported(text) || [];
        if (missing.length) warnings.push(`${where}: «${missing.join(' ')}» no existe en la fuente de la TFT (se ve sin tilde o como ?).`);
      };

      m.nodes.forEach((n) => {
        const where = `${name} › ${n.label || n.type}`;
        if (n.parent && !nodeByKey(m.nodes, n.parent)) errors.push(`${where}: padre inexistente.`);
        if (levelOf(m.nodes, n) > MAX_DEPTH) errors.push(`${where}: supera la profundidad ${MAX_DEPTH}.`);
        if (n.type === 'SUBMENU') {
          if (childrenOf(m.nodes, n.key).length === 0) warnings.push(`${where}: submenú vacío (no se puede abrir).`);
          noGlyphWarning(where, n.label);
          return;
        }
        if (n.type === 'GO_IDLE') {
          noGlyphWarning(where, n.label);
          return;
        }
        const variable = sanitizeSymbol(n.variable, '');
        if (!variable) { errors.push(`${where}: falta la variable C++.`); return; }
        if (!NODE_TYPES[n.type].kinds.includes(n.valueType)) errors.push(`${where}: tipo ${n.valueType} no válido para ${n.type}.`);
        const type = n.valueType;
        if (fieldVars.has(variable) && fieldVars.get(variable) !== type) {
          errors.push(`${where}: ${variable} ya es ${fieldVars.get(variable)} en un campo; aquí se usa como ${type}.`);
        }
        if (menuVars.has(variable) && menuVars.get(variable) !== type) {
          errors.push(`${where}: ${variable} se usa como ${menuVars.get(variable)} y como ${type}.`);
        }
        menuVars.set(variable, type);
        noGlyphWarning(where, `${n.label}${n.unit || ''}${n.falseText || ''}${n.trueText || ''}`);
        if (n.type === 'EDIT_NUM') {
          if (num(n.max) < num(n.min)) errors.push(`${where}: máximo menor que el mínimo.`);
          if (!(num(n.step) > 0)) errors.push(`${where}: el paso debe ser mayor que 0.`);
          if (type === 'int32' && [n.min, n.max, n.step].some((v) => !Number.isInteger(num(v)))) {
            errors.push(`${where}: int32 requiere mínimo, máximo y paso enteros.`);
          }
        }
      });
    });
    return { errors, warnings };
  }

  function nodeSymbols(menu) {
    const used = new Set();
    const map = new Map();
    dfsOrder(menu.nodes).forEach((n) => {
      const base = `${menu.id}_${sanitizeSymbol(n.label, 'NODO').toUpperCase()}`;
      let sym = base;
      let k = 2;
      while (used.has(sym)) sym = `${base}_${k++}`;
      used.add(sym);
      map.set(n.key, sym);
    });
    return map;
  }

  function nodeCpp(menu, n, symbols) {
    const parent = n.parent ? symbols.get(n.parent) : 'JWPLC_UI_MENU_ROOT';
    const label = cppString(n.label);
    const variable = `&${sanitizeSymbol(n.variable, 'menuVar')}`;
    const unit = n.unit ? cppString(n.unit) : 'nullptr';
    const decimals = clamp(Math.trunc(num(n.decimals, 1)), 0, 6);
    if (n.type === 'SUBMENU') return `JWPLC_UIMenuSubmenu(${parent}, ${label})`;
    if (n.type === 'GO_IDLE') return `JWPLC_UIMenuGoIdle(${parent}, ${label})`;
    if (n.type === 'VIEW') {
      if (n.valueType === 'bool') return `JWPLC_UIMenuView(${parent}, ${label}, ${variable}, ${cppString(n.falseText)}, ${cppString(n.trueText)})`;
      if (n.valueType === 'int32') return `JWPLC_UIMenuView(${parent}, ${label}, ${variable}, ${unit})`;
      return `JWPLC_UIMenuView(${parent}, ${label}, ${variable}, ${decimals}, ${unit})`;
    }
    if (n.type === 'EDIT_BOOL') return `JWPLC_UIMenuEditBool(${parent}, ${label}, ${variable}, ${cppString(n.falseText)}, ${cppString(n.trueText)})`;
    if (n.valueType === 'int32') {
      return `JWPLC_UIMenuEditNum(${parent}, ${label}, ${variable}, ${Math.trunc(num(n.min))}, ${Math.trunc(num(n.max))}, ${Math.max(1, Math.trunc(num(n.step, 1)))}, ${unit})`;
    }
    return `JWPLC_UIMenuEditNum(${parent}, ${label}, ${variable}, ${cppFloat(n.min)}, ${cppFloat(n.max)}, ${cppFloat(n.step)}, ${decimals}, ${unit})`;
  }

  function initialCpp(type, value) {
    if (type === 'bool') return num(value) ? 'true' : 'false';
    if (type === 'int32') return String(Math.trunc(num(value)));
    return cppFloat(value);
  }

  // generate(menus, { fields, pageSymbol(pageId) }) ->
  //   { errors, warnings, variables, declarations, setup }
  function generate(menus, context = {}) {
    const fields = context.fields || [];
    const pageSymbol = context.pageSymbol || ((id) => String(id));
    const { errors, warnings } = validate(menus, fields);
    if (!menus.length) return { errors, warnings, variables: '', declarations: '', setup: '' };

    const fieldVars = fieldVariableTypes(fields);
    const declared = new Set();
    const variableChunks = [];
    const declChunks = [];

    const guard =
      `// MENU (A15-8): requiere JWPLC_Display con soporte de MENU.\n` +
      `#ifndef JWPLC_UI_HAS_MENU\n` +
      `#error "El MENU requiere JWPLC_Display con soporte de MENU (package JWPLC Alpha15 o posterior)."\n` +
      `#endif\n` +
      errors.map((e) => `#error ${cppPlainString(`MENU: ${e}`)}\n`).join('');

    const menuEnum = `enum HMIMenuId : uint8_t\n{\n${menus.map((m, i) => `    ${m.id} = ${i},`).join('\n')}\n};`;
    declChunks.push(guard + '\n' + menuEnum);

    const menuEntries = [];
    menus.forEach((m) => {
      const ordered = dfsOrder(m.nodes);
      const symbols = nodeSymbols(m);

      const vars = [];
      ordered.forEach((n) => {
        if (!hasVariable(n)) return;
        const variable = sanitizeSymbol(n.variable, 'menuVar');
        if (declared.has(variable) || fieldVars.has(variable)) return;
        declared.add(variable);
        vars.push(`${CPP_TYPE[n.valueType] || 'float'} ${variable} = ${initialCpp(n.valueType, n.initial)};`);
      });
      if (vars.length) variableChunks.push(`// MENU ${m.title || m.name}\n${vars.join('\n')}`);

      const nodesName = `HMI_${m.id}_NODES`;
      declChunks.push(
        `// Nodos de ${m.id}: el índice es el que informa JWPLC_Display.menuChanged().\n` +
        `enum : uint8_t\n{\n${ordered.map((n, i) => `    ${symbols.get(n.key)} = ${i},`).join('\n')}\n};\n\n` +
        `static const JWPLC_UIMenuNode ${nodesName}[] =\n{\n${ordered.map((n) => `    ${nodeCpp(m, n, symbols)}`).join(',\n')}\n};`);

      const c = { ...DEFAULT_COLORS, ...m.colors };
      menuEntries.push(
        `    JWPLC_UIMenuField(\n` +
        `        ${m.id},\n` +
        `        ${Math.trunc(num(m.x))}, ${Math.trunc(num(m.y))}, ${Math.trunc(num(m.width))}, ${Math.trunc(num(m.height))},\n` +
        `        ${m.title ? cppString(m.title) : 'nullptr'},\n` +
        `        ${nodesName},\n` +
        `        sizeof(${nodesName}) / sizeof(${nodesName}[0]),\n` +
        `        ${pageSymbol(Number(m.page || 0))},\n` +
        `        ${Number(m.textSize) >= 2 ? 2 : 1},\n` +
        `        ${m.frame ? 'true' : 'false'},\n` +
        `        JWPLC_UIMenuColors(\n` +
        `            ${[c.text, c.background, c.value, c.selectText, c.selectBackground, c.editText, c.editBackground, c.title, c.titleBackground, c.frame].map(hex565).join(',\n            ')}))`);
    });

    declChunks.push(`static const JWPLC_UIMenu HMI_MENUS[] =\n{\n${menuEntries.join(',\n')}\n};`);

    return {
      errors,
      warnings,
      variables: variableChunks.join('\n\n'),
      declarations: declChunks.join('\n\n'),
      setup: `    JWPLC_Display.setMenus(\n        HMI_MENUS,\n        sizeof(HMI_MENUS) / sizeof(HMI_MENUS[0]));\n`
    };
  }

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function allMenus() {
    return (editor()?.getAllFields?.() || []).filter((f) => f.type === 'MENU' && !f.hidden);
  }

  // Integra el bloque MENU en el header generado por el resto de capas.
  function patchHeader(text) {
    const menus = allMenus();
    if (!menus.length || !text.includes('void jwplcHMISetup()')) return text;
    const symbols = new Map((window.JWPLCHMICodegen?.getPageSymbols?.() || []).map((p) => [p.id, p.symbol]));
    const out = generate(menus, {
      fields: (editor()?.getAllFields?.() || []).filter((f) => f.type !== 'MENU'),
      pageSymbol: (id) => symbols.get(id) || String(id)
    });

    // Idempotente por partes: otras capas pueden volver a aplicarse sobre el
    // mismo texto y regenerar sólo la sección de variables.
    if (out.variables && !text.includes(out.variables)) {
      text = text.replace(/(\/\/ Variables HMI\n[\s\S]*?)(\n\n\/\/ Definición declarativa)/, (all, head, tail) =>
        `${head.trimEnd()}\n\n${out.variables}${tail}`);
    }
    if (!text.includes('static const JWPLC_UIMenu HMI_MENUS[]')) {
      text = text.replace('void jwplcHMISetup()\n{\n', `${out.declarations}\n\nvoid jwplcHMISetup()\n{\n`);
    }
    if (!text.includes('JWPLC_Display.setMenus(')) {
      text = text.replace('void jwplcHMISetup()\n{\n', `void jwplcHMISetup()\n{\n${out.setup}`);
    }
    return text;
  }

  // ---------------------------------------------------------------------------
  // Inspector
  // ---------------------------------------------------------------------------

  // Sin DOM (node/vm en tests/codegen-core.test.cjs) sólo se publica la parte pura.
  const anchor = typeof document !== 'undefined' ? document.getElementById('textFieldControlsSection') : null;
  if (!anchor) {
    window.JWPLCHMIMenu = { createDefault, ensureState, paint, renderToArray, layout, generate, validate, patchHeader, dfsOrder };
    return;
  }

  const section = document.createElement('section');
  section.id = 'menuInspectorSection';
  section.className = 'inspector-section';
  section.hidden = true;
  section.style.display = 'none';
  section.innerHTML = `
    <div class="inspector-title">
      <h2>Inspector — MENU</h2>
      <span>
        <button type="button" class="icon-button" data-menu-action="duplicate" title="Duplicar">⧉</button>
        <button type="button" class="icon-button" data-menu-action="delete" title="Eliminar">🗑</button>
      </span>
    </div>
    <div class="insp-wrap">
      <div class="bar-nav-header insp-nav-header">
        <div class="bar-main-tabs" role="tablist">
          <button type="button" class="bar-main-tab active" data-menu-main="design">Diseño</button>
          <button type="button" class="bar-main-tab" data-menu-main="tree">Árbol y datos</button>
          <button type="button" class="bar-main-tab" data-menu-main="tech">Técnico</button>
        </div>
        <div class="bar-sub-tabs" data-menu-subtabs>
          <button type="button" class="bar-sub-tab active" data-menu-sub="shape">Forma</button>
          <button type="button" class="bar-sub-tab" data-menu-sub="content">Contenido</button>
          <button type="button" class="bar-sub-tab" data-menu-sub="appearance">Apariencia</button>
          <button type="button" class="bar-sub-tab" data-menu-sub="placement">Ubicación</button>
        </div>
      </div>
      <div class="insp-body">
        <div class="insp-panel" data-menu-panel="design:shape">
          <div class="bar-card">
            <div class="bar-card-section-title">Tamaño de texto</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-menu-set="textSize" data-value="1"><strong>Aa</strong><span>1× · filas de 12 px</span></button>
              <button type="button" class="bar-visual-btn" data-menu-set="textSize" data-value="2"><strong style="font-size:15px">Aa</strong><span>2× · filas de 20 px</span></button>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Marco</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-menu-set="frame" data-value="0"><span>Sin marco</span></button>
              <button type="button" class="bar-visual-btn" data-menu-set="frame" data-value="1"><span>Con marco</span></button>
            </div>
          </div>
          <div class="bar-card">
            <div class="insp-readouts insp-readouts-grid">
              <span>Filas visibles <strong data-menu-readout="rows">—</strong></span>
              <span>Caracteres por fila <strong data-menu-readout="chars">—</strong></span>
            </div>
          </div>
        </div>

        <div class="insp-panel" data-menu-panel="design:content" hidden>
          <div class="bar-card">
            <label class="field-label">Título de la raíz (vacío = sin barra de título)
              <input class="field-input" data-menu-prop="title" type="text" maxlength="40" />
            </label>
            <div class="insp-note">En los submenús la barra muestra "&lt; Nombre del submenú". Las flechas ▲▼ aparecen cuando hay más ítems que filas.</div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Teclas en la placa</div>
            <div class="insp-note">UP/DOWN mueve · OK o RIGHT entra al submenú · OK edita y confirma · LEFT/RIGHT ±10 pasos al editar · ESC cancela o sube · ESC en la raíz vuelve al selector de páginas.</div>
          </div>
        </div>

        <div class="insp-panel" data-menu-panel="design:appearance" hidden>
          <div class="bar-card" data-menu-colors></div>
        </div>

        <div class="insp-panel" data-menu-panel="design:placement" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Página</div>
            <div class="insp-chip-row" data-menu-pages></div>
          </div>
          <div class="bar-card">
            <div class="insp-two">
              <label class="field-label">X (px)<input class="field-input" data-menu-prop="x" type="number" min="0" max="319" /></label>
              <label class="field-label">Y (px)<input class="field-input" data-menu-prop="y" type="number" min="0" max="169" /></label>
              <label class="field-label">Ancho (px)<input class="field-input" data-menu-prop="width" type="number" min="40" max="320" /></label>
              <label class="field-label">Alto (px)<input class="field-input" data-menu-prop="height" type="number" min="14" max="170" /></label>
            </div>
            <div class="insp-note">Con varias páginas, evita la esquina superior derecha: allí se dibuja el indicador NN/TT.</div>
          </div>
        </div>

        <div class="insp-panel" data-menu-panel="tree" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Agregar <span class="insp-hint" data-menu-readout="addTarget"></span></div>
            <div class="insp-chip-row">
              <button type="button" class="insp-chip" data-menu-add="SUBMENU">▸ Submenú</button>
              <button type="button" class="insp-chip" data-menu-add="VIEW">◉ Ver</button>
              <button type="button" class="insp-chip" data-menu-add="EDIT_NUM">± Editar nº</button>
              <button type="button" class="insp-chip" data-menu-add="EDIT_BOOL">⏻ Editar ON/OFF</button>
              <button type="button" class="insp-chip" data-menu-add="GO_IDLE" title="Con OK vuelve a la pantalla IDLE (útil con «Volver a la IDLE: Nunca»)">⏎ Ir al inicio</button>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Árbol <span class="insp-hint" data-menu-readout="nodes"></span></div>
            <div class="menu-tree" data-menu-tree></div>
          </div>
          <div class="bar-card" data-menu-node-editor></div>
        </div>

        <div class="insp-panel" data-menu-panel="tech" hidden>
          <div class="bar-card">
            <label class="field-label">Nombre del objeto (sólo Designer)<input class="field-input" data-menu-prop="name" type="text" maxlength="24" /></label>
            <label class="field-label">ID C++ (enum HMIMenuId)<input class="field-input code-input" data-menu-prop="id" type="text" maxlength="32" /></label>
          </div>
          <div class="bar-card">
            <div class="bar-card-header"><span class="bar-card-title">Uso desde el sketch</span>
              <button type="button" class="icon-button" data-menu-action="copy" title="Copiar">⧉</button></div>
            <pre class="insp-contract" data-menu-contract></pre>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Diagnóstico</div>
            <div class="insp-readouts insp-readouts-grid">
              <span>Nodos <strong data-menu-readout="nodeCount">—</strong></span>
              <span>Profundidad <strong data-menu-readout="depth">—</strong></span>
              <span>RAM de la tabla <strong data-menu-readout="ram">—</strong></span>
              <span>Filas visibles <strong data-menu-readout="rows2">—</strong></span>
            </div>
            <div class="menu-issues" data-menu-issues></div>
          </div>
        </div>
      </div>
    </div>`;
  anchor.insertAdjacentElement('beforebegin', section);

  const tabs = { main: 'design', sub: 'shape' };

  function selectedMenu() {
    const f = editor()?.getSelectedField?.();
    return f?.type === 'MENU' ? ensureState(f) : null;
  }

  function ui(field) {
    if (!uiState.has(field.key)) uiState.set(field.key, { nodeKey: null, editing: false });
    return uiState.get(field.key);
  }

  // Copy-on-write: el historial guarda copias superficiales de los campos.
  function mutate(fn, commit = true) {
    const field = selectedMenu();
    if (!field) return;
    field.nodes = field.nodes.map((n) => ({ ...n }));
    field.colors = { ...field.colors };
    fn(field);
    editor()?.render?.();
    if (commit) editor()?.commitHistory?.();
  }

  function applyTabs() {
    section.querySelectorAll('[data-menu-main]').forEach((b) => b.classList.toggle('active', b.dataset.menuMain === tabs.main));
    section.querySelectorAll('[data-menu-sub]').forEach((b) => b.classList.toggle('active', b.dataset.menuSub === tabs.sub));
    section.querySelector('[data-menu-subtabs]').hidden = tabs.main !== 'design';
    const key = tabs.main === 'design' ? `design:${tabs.sub}` : tabs.main;
    section.querySelectorAll('[data-menu-panel]').forEach((p) => { p.hidden = p.dataset.menuPanel !== key; });
  }

  function addTargetParent(field) {
    const node = ui(field).nodeKey ? nodeByKey(field.nodes, ui(field).nodeKey) : null;
    if (!node) return null;
    return node.type === 'SUBMENU' ? node.key : (node.parent || null);
  }

  // --- Apariencia: dos mini-modelos con los colores de cada parte ----------
  // Las vistas usan el mismo pintor que el lienzo (paint), en tamaño 1× y
  // escala 2. Cada fila de controles queda a la altura de la fila que pinta.
  const PREVIEW_W = 80;
  const PREVIEW_SCALE = 2;
  const PREVIEW_NODES = [
    { key: 'p1', parent: null, type: 'VIEW', label: 'Temp', variable: 'p1', valueType: 'float', initial: 21.5, decimals: 1, unit: '' },
    { key: 'p2', parent: null, type: 'EDIT_NUM', label: 'Nivel', variable: 'p2', valueType: 'int32', initial: 50, decimals: 0, unit: '' },
    { key: 'p3', parent: null, type: 'EDIT_BOOL', label: 'Bomba', variable: 'p3', valueType: 'bool', initial: 0, falseText: 'OFF', trueText: 'ON' }
  ];
  const COLOR_LETTER = { titleBackground: 'F', title: 'T', text: 'T', value: 'V', selectBackground: 'F', selectText: 'T', editBackground: 'F', editText: 'T', background: 'F', frame: 'M' };
  const COLOR_NAME = Object.fromEntries(COLOR_LABELS);
  const CASES = [
    {
      id: 'nav',
      title: 'Caso 1 · Navegando',
      editing: false,
      rows: [
        { label: 'Título', keys: ['titleBackground', 'title'] },
        { label: 'Ítem', keys: ['text', 'value'] },
        { label: 'Selección', keys: ['selectBackground', 'selectText'] },
        { label: 'Fondo', keys: ['background'] }
      ]
    },
    {
      id: 'edit',
      title: 'Caso 2 · Editando un valor (OK)',
      editing: true,
      rows: [null, null, { label: 'Edición', keys: ['editBackground', 'editText'] }, null]
    }
  ];
  let activeColorKey = 'titleBackground';

  // Editor de color fijo bajo las vistas (designer-color-editor.js): en vivo
  // mientras se arrastra y al historial al soltar.
  const colorEditor = window.JWPLCColorEditor?.create({
    paletteAttr: 'data-menu-palette',
    onInput: (v) => mutate((f) => { f.colors[activeColorKey] = v; }, false),
    onCommit: (v) => mutate((f) => { f.colors[activeColorKey] = v; })
  });

  function swatchHtml(key) {
    return `<button type="button" class="menu-swatch" data-menu-swatch="${key}" title="${COLOR_NAME[key]}">` +
      `<span>${COLOR_LETTER[key]}</span></button>`;
  }

  function buildAppearance() {
    const host = section.querySelector('[data-menu-colors]');
    if (host.dataset.built) return;
    host.dataset.built = '1';
    host.innerHTML = CASES.map((c) => `
      <div class="menu-case">
        <div class="bar-card-section-title">${c.title}</div>
        <div class="menu-case-body zone-host" data-menu-case-body="${c.id}">
          <canvas class="menu-case-canvas" data-menu-case="${c.id}"></canvas>
          <div class="menu-case-legend" data-menu-legend="${c.id}">
            ${c.rows.map((r, i) => `<div class="menu-legend-row"${r ? ` data-menu-legend-row="${i}" data-keys="${r.keys.join(',')}"` : ''}>${r
              ? `<span class="menu-legend-label">${r.label}</span>${r.keys.map(swatchHtml).join('')}`
              : ''}</div>`).join('')}
          </div>
        </div>
      </div>`).join('') + `
      <div class="menu-legend-row menu-frame-row" data-menu-frame-row>
        <span class="menu-legend-label">Marco</span>${swatchHtml('frame')}
      </div>
      <div class="menu-palette">
        <div class="insp-note">Clic en un cuadro (o en el texto de la fila) para editar ese color: el contorno y el ▶ marcan lo que cambia en cada vista.</div>
        <div data-menu-color-editor></div>
      </div>`;
    if (colorEditor) host.querySelector('[data-menu-color-editor]').appendChild(colorEditor.el);
  }

  function drawCasePreview(canvas, field, c) {
    const L = layout({ ...field, textSize: 1 });
    const h = 2 * L.border + L.rowH * 4;
    const preview = {
      ...field, x: 0, y: 0, width: PREVIEW_W, height: h, textSize: 1,
      title: field.title || 'Menú', nodes: PREVIEW_NODES
    };
    const fb = renderToArray(preview, { parentKey: null, selected: 1, depth: 1, editing: c.editing, focused: true });
    canvas.width = PREVIEW_W * PREVIEW_SCALE;
    canvas.height = h * PREVIEW_SCALE;
    const ctx = canvas.getContext('2d');
    const img = ctx.createImageData(canvas.width, canvas.height);
    for (let y = 0; y < canvas.height; y++) {
      for (let x = 0; x < canvas.width; x++) {
        const v = fb[Math.floor(y / PREVIEW_SCALE) * SCREEN_W + Math.floor(x / PREVIEW_SCALE)];
        const o = (y * canvas.width + x) * 4;
        img.data[o] = ((v >> 11) & 0x1F) * 255 / 31;
        img.data[o + 1] = ((v >> 5) & 0x3F) * 255 / 63;
        img.data[o + 2] = (v & 0x1F) * 255 / 31;
        img.data[o + 3] = 255;
      }
    }
    ctx.putImageData(img, 0, 0);
    canvas.dataset.logicalW = String(PREVIEW_W);
    canvas.dataset.logicalH = String(h);
    caseZones.set(c.id, zonesFor(fb, L, field, c, h));
    // Las filas de controles miden lo mismo que las filas pintadas.
    const legend = section.querySelector(`[data-menu-legend="${c.id}"]`);
    legend.style.paddingTop = `${L.border * PREVIEW_SCALE}px`;
    legend.style.setProperty('--menu-row-h', `${L.rowH * PREVIEW_SCALE}px`);
  }

  // Zonas de la vista para las flechas de la leyenda: cada fila pintada
  // (banda) y, dentro de ella, el texto de cada color de texto.
  const caseZones = new Map();
  const TEXT_KEYS = new Set(['title', 'text', 'value', 'selectText', 'editText']);

  function zonesFor(fb, L, field, c, h) {
    const bands = [0, 1, 2, 3].map((i) => ({ x: L.border, y: L.border + i * L.rowH, w: PREVIEW_W - 2 * L.border, h: Math.min(L.rowH, h - L.border - (L.border + i * L.rowH)) }));
    const keys = {};
    c.rows.forEach((r, i) => {
      if (!r) return;
      r.keys.forEach((key) => {
        const band = bands[i];
        if (!TEXT_KEYS.has(key)) { keys[key] = { zone: band }; return; }
        const color = num(field.colors[key]) & 0xFFFF;
        let minX = Infinity; let maxX = -1; let minY = Infinity; let maxY = -1;
        for (let y = band.y; y < band.y + band.h; y++) {
          for (let x = band.x; x < band.x + band.w; x++) {
            if (fb[y * SCREEN_W + x] !== color) continue;
            minX = Math.min(minX, x); maxX = Math.max(maxX, x); minY = Math.min(minY, y); maxY = Math.max(maxY, y);
          }
        }
        keys[key] = { zone: maxX < 0 ? band : { x: minX, y: minY, w: maxX - minX + 1, h: maxY - minY + 1 } };
      });
    });
    return { bands, keys };
  }

  // Indicador A1 (designer-zone-indicator.js): contorno neutro y ▶ en la zona
  // del color activo. El Marco no tiene fila por caso: se marca en ambas vistas.
  function drawMenuArrows() {
    CASES.forEach((c) => {
      const body = section.querySelector(`[data-menu-case-body="${c.id}"]`);
      const zones = caseZones.get(c.id);
      const canvas = body?.querySelector('canvas');
      if (!body || !zones || !canvas) return;
      let rects = [];
      body.querySelectorAll('[data-menu-legend-row]').forEach((row) => {
        const active = row.dataset.keys.split(',').includes(activeColorKey) && Boolean(zones.keys[activeColorKey]);
        row.classList.toggle('is-active', active);
        if (active) rects = [zones.keys[activeColorKey].zone];
      });
      if (activeColorKey === 'frame') {
        rects = [{ x: 0, y: 0, w: Number(canvas.dataset.logicalW), h: Number(canvas.dataset.logicalH) }];
      }
      window.JWPLCZoneIndicator?.draw(body, canvas, rects);
    });
    section.querySelector('[data-menu-frame-row]')?.classList.toggle('is-active', activeColorKey === 'frame');
  }

  function syncAppearance(field) {
    buildAppearance();
    CASES.forEach((c) => drawCasePreview(section.querySelector(`[data-menu-case="${c.id}"]`), field, c));
    section.querySelector('[data-menu-frame-row]').hidden = !field.frame;
    Object.keys(DEFAULT_COLORS).forEach((key) => {
      const v = num(field.colors[key]) & 0xFFFF;
      section.querySelectorAll(`[data-menu-swatch="${key}"]`).forEach((sw) => {
        sw.style.background = rgb565ToCss(v);
        sw.style.color = ((v >> 11) + ((v >> 5) & 0x3F) / 2 + (v & 0x1F)) > 40 ? '#000' : '#fff';
        sw.classList.toggle('active', key === activeColorKey);
      });
    });
    colorEditor?.set(num(field.colors[activeColorKey]) & 0xFFFF, COLOR_NAME[activeColorKey]);
    requestAnimationFrame(drawMenuArrows);
  }

  function renderTree(field) {
    const host = section.querySelector('[data-menu-tree]');
    const selectedKey = ui(field).nodeKey;
    const signature = JSON.stringify([field.nodes.map((n) => [n.key, n.parent, n.type, n.label, n.variable]), selectedKey]);
    if (host.dataset.signature === signature) return;
    host.dataset.signature = signature;
    const rows = [];
    const walk = (parentKey, depth) => {
      const siblings = childrenOf(field.nodes, parentKey);
      siblings.forEach((n, i) => {
        rows.push(`
          <div class="menu-tree-row${n.key === selectedKey ? ' active' : ''}" data-menu-node="${n.key}" style="padding-left:${6 + (depth - 1) * 14}px">
            <span class="menu-tree-icon">${NODE_TYPES[n.type].icon}</span>
            <span class="menu-tree-label">${escapeHtml(n.label)}</span>
            <span class="menu-tree-var">${n.type === 'SUBMENU' ? `${childrenOf(field.nodes, n.key).length} ítems` : n.type === 'GO_IDLE' ? '→ IDLE' : escapeHtml(n.variable)}</span>
            <span class="menu-tree-btns">
              <button type="button" data-menu-move="-1" ${i === 0 ? 'disabled' : ''} title="Subir">▲</button>
              <button type="button" data-menu-move="1" ${i === siblings.length - 1 ? 'disabled' : ''} title="Bajar">▼</button>
              <button type="button" data-menu-remove title="Eliminar">✕</button>
            </span>
          </div>`);
        if (n.type === 'SUBMENU') walk(n.key, depth + 1);
      });
    };
    walk(null, 1);
    host.innerHTML = rows.join('') || '<div class="insp-note">Árbol vacío: agrega un ítem.</div>';
  }

  function chips(attr, values, current) {
    return values.map(([v, text]) =>
      `<button type="button" class="insp-chip${String(v) === String(current) ? ' active' : ''}" ${attr}="${v}">${text}</button>`).join('');
  }

  function renderNodeEditor(field) {
    const host = section.querySelector('[data-menu-node-editor]');
    const node = ui(field).nodeKey ? nodeByKey(field.nodes, ui(field).nodeKey) : null;
    const signature = node ? `${node.key}|${node.type}|${node.valueType}|${ui(field).editing}` : 'none';
    if (host.dataset.signature !== signature) {
      host.dataset.signature = signature;
      if (!node) {
        host.innerHTML = '<div class="insp-note">Elige un ítem del árbol para editar su texto y su variable.</div>';
        return;
      }
      const t = node.type;
      const kinds = NODE_TYPES[t].kinds;
      const isBool = node.valueType === 'bool';
      host.innerHTML = `
        <div class="bar-card-section-title">${NODE_TYPES[t].icon} ${NODE_TYPES[t].label} · nivel ${levelOf(field.nodes, node)}</div>
        <label class="field-label">Texto del ítem<input class="field-input" data-node-prop="label" type="text" maxlength="39" /></label>
        ${t === 'GO_IDLE' ? '<div class="insp-note">Con OK vuelve a la pantalla IDLE (<code>JWPLC_UIMenuGoIdle</code>). Úsalo con «Volver a la IDLE: Nunca» para que la salida la decida el menú.</div>' : ''}
        ${!hasVariable(node) ? '' : `
          <label class="field-label">Variable C++<input class="field-input code-input" data-node-prop="variable" type="text" maxlength="32" /></label>
          ${kinds.length > 1 ? `<span class="bar-visual-group-title insp-gap">Tipo</span><div class="insp-chip-row">${chips('data-node-kind', kinds.map((k) => [k, k === 'int32' ? 'int32_t' : k]), node.valueType)}</div>` : `<div class="insp-readouts"><span>Tipo <code class="insp-type-chip">${CPP_TYPE[node.valueType]}</code></span></div>`}
          ${isBool ? `
            <div class="insp-bool-states insp-gap">
              <label class="insp-state-card"><span>FALSE</span><input class="field-input" data-node-prop="falseText" type="text" maxlength="12" /></label>
              <span class="insp-arrow">→</span>
              <label class="insp-state-card is-on"><span>TRUE</span><input class="field-input" data-node-prop="trueText" type="text" maxlength="12" /></label>
            </div>
            <div class="insp-chip-row insp-gap">${chips('data-node-bool-preset', [['OFF|ON', 'OFF/ON'], ['NO|SI', 'NO/SI'], ['PARADA|MARCHA', 'PARADA/MARCHA'], ['CERRADO|ABIERTO', 'CERRADO/ABIERTO']], `${node.falseText}|${node.trueText}`)}</div>
            <span class="bar-visual-group-title insp-gap">Valor inicial / de prueba</span>
            <div class="bar-visual-grid-2">${[[0, node.falseText || 'OFF'], [1, node.trueText || 'ON']].map(([v, text]) =>
              `<button type="button" class="bar-visual-btn${Number(Boolean(num(node.initial))) === v ? ' active' : ''}" data-node-initial-bool="${v}"><strong>${escapeHtml(text)}</strong></button>`).join('')}</div>
          ` : `
            <div class="insp-two insp-gap">
              <label class="field-label">Unidad<input class="field-input" data-node-prop="unit" type="text" maxlength="8" /></label>
              <label class="field-label">Valor inicial / de prueba<input class="field-input" data-node-prop="initial" type="number" step="any" /></label>
            </div>
            ${node.valueType === 'float' ? `<span class="bar-visual-group-title insp-gap">Decimales</span><div class="insp-chip-row">${chips('data-node-decimals', [[0, '0'], [1, '1'], [2, '2'], [3, '3']], node.decimals)}</div>` : ''}
            ${t === 'EDIT_NUM' ? `
              <div class="insp-two insp-gap">
                <label class="field-label">Mínimo<input class="field-input" data-node-prop="min" type="number" step="any" /></label>
                <label class="field-label">Máximo<input class="field-input" data-node-prop="max" type="number" step="any" /></label>
                <label class="field-label">Paso (UP/DOWN)<input class="field-input" data-node-prop="step" type="number" step="any" min="0" /></label>
                <div class="insp-readouts"><span>LEFT/RIGHT <strong data-menu-readout="bigStep"></strong></span></div>
              </div>` : ''}
          `}
          ${(t === 'EDIT_NUM' || t === 'EDIT_BOOL') ? `
            <label class="field-label-checkbox insp-gap"><input type="checkbox" data-node-preview-edit ${ui(field).editing ? 'checked' : ''} /><span>Ver en el lienzo como "editando"</span></label>` : ''}
        `}`;
    }
    if (!node) return;
    host.querySelectorAll('[data-node-prop]').forEach((input) => {
      if (document.activeElement === input) return;
      const v = node[input.dataset.nodeProp];
      input.value = v ?? '';
    });
    const big = host.querySelector('[data-menu-readout="bigStep"]');
    if (big) big.textContent = `±${formatValue({ ...node, unit: node.unit, type: 'EDIT_NUM' }, num(node.step) * 10)}`;
  }

  function contractText(field) {
    const symbols = nodeSymbols(field);
    const editable = dfsOrder(field.nodes).filter((n) => n.type === 'EDIT_NUM' || n.type === 'EDIT_BOOL');
    const cases = editable.map((n) =>
      `        case ${symbols.get(n.key)}:\n            // ${sanitizeSymbol(n.variable, 'var')} cambió: guardar en FRAM, aplicar, etc.\n            break;`).join('\n');
    return `// Las variables del menú están declaradas en JWPLC_HMI_Generated.h.\n` +
      `// El menú escribe la variable al confirmar con OK.\n` +
      `void loop()\n{\n` +
      `    uint8_t nodo;\n` +
      `    if (JWPLC_Display.menuChanged(${field.id}, &nodo))\n    {\n` +
      `        switch (nodo)\n        {\n${cases || '        // (sin ítems editables)'}\n        default:\n            break;\n        }\n    }\n}`;
  }

  function sync() {
    const field = selectedMenu();
    if (!field) return;
    applyTabs();

    section.querySelectorAll('[data-menu-prop]').forEach((input) => {
      if (document.activeElement !== input) input.value = field[input.dataset.menuProp] ?? '';
    });
    section.querySelectorAll('[data-menu-set="textSize"]').forEach((b) => b.classList.toggle('active', Number(b.dataset.value) === Number(field.textSize)));
    section.querySelectorAll('[data-menu-set="frame"]').forEach((b) => b.classList.toggle('active', (b.dataset.value === '1') === Boolean(field.frame)));

    const L = layout(field);
    const set = (name, text) => section.querySelectorAll(`[data-menu-readout="${name}"]`).forEach((el) => { el.textContent = text; });
    set('rows', String(L.rows));
    set('rows2', String(L.rows));
    set('chars', String(Math.max(0, Math.trunc((num(field.width) - 2 * L.border - 2 * ROW_PAD_X) / L.cw))));
    set('nodes', `${field.nodes.length}/${MAX_NODES}`);
    set('nodeCount', `${field.nodes.length}/${MAX_NODES}`);
    set('depth', `${Math.max(0, ...field.nodes.map((n) => levelOf(field.nodes, n) + (n.type === 'SUBMENU' ? 1 : 0)))}/${MAX_DEPTH}`);
    set('ram', `${field.nodes.length * 36} B`);

    const target = addTargetParent(field);
    const targetNode = target ? nodeByKey(field.nodes, target) : null;
    set('addTarget', targetNode ? `en "${targetNode.label}"` : 'en la raíz');
    const targetLevel = targetNode ? levelOf(field.nodes, targetNode) + 1 : 1;
    const submenuBtn = section.querySelector('[data-menu-add="SUBMENU"]');
    submenuBtn.disabled = targetLevel >= MAX_DEPTH;
    submenuBtn.title = submenuBtn.disabled ? `Profundidad máxima ${MAX_DEPTH}: un submenú aquí no tendría dónde abrir.` : '';
    section.querySelectorAll('[data-menu-add]').forEach((b) => {
      if (field.nodes.length >= MAX_NODES) b.disabled = true;
      else if (b.dataset.menuAdd !== 'SUBMENU') b.disabled = false;
    });

    syncAppearance(field);

    const pagesHost = section.querySelector('[data-menu-pages]');
    const pages = editor()?.getPages?.() || [];
    pagesHost.innerHTML = pages.map((p) =>
      `<button type="button" class="insp-chip${Number(p.id) === Number(field.page || 0) ? ' active' : ''}" data-menu-page="${p.id}">${String(Number(p.id) + 1).padStart(2, '0')} · ${escapeHtml(p.name)}</button>`).join('');

    renderTree(field);
    renderNodeEditor(field);

    section.querySelector('[data-menu-contract]').textContent = contractText(field);
    const fields = editor()?.getAllFields?.() || [];
    const { errors, warnings } = validate(fields.filter((f) => f.type === 'MENU'), fields.filter((f) => f.type !== 'MENU'));
    section.querySelector('[data-menu-issues]').innerHTML =
      errors.map((e) => `<div class="menu-issue is-error">✕ ${escapeHtml(e)}</div>`).join('') +
      warnings.map((w) => `<div class="menu-issue">! ${escapeHtml(w)}</div>`).join('') ||
      '<div class="menu-issue is-ok">✓ Sin problemas</div>';
  }

  // --- Eventos ---------------------------------------------------------------

  section.addEventListener('click', (e) => {
    const t = e.target;
    const field = selectedMenu();
    if (!field) return;

    const main = t.closest('[data-menu-main]');
    if (main) { tabs.main = main.dataset.menuMain; applyTabs(); return; }
    const sub = t.closest('[data-menu-sub]');
    if (sub) { tabs.sub = sub.dataset.menuSub; applyTabs(); requestAnimationFrame(drawMenuArrows); return; }

    const action = t.closest('[data-menu-action]')?.dataset.menuAction;
    if (action === 'duplicate') {
      window.JWPLCDialog.alert('Un solo MENU por página: duplica el menú moviéndolo antes a otra página.');
      return;
    }
    if (action === 'delete') { editor()?.deleteSelectedField?.(); return; }
    if (action === 'copy') { navigator.clipboard?.writeText(contractText(field)).catch(() => {}); return; }

    const set = t.closest('[data-menu-set]');
    if (set) {
      mutate((f) => {
        if (set.dataset.menuSet === 'textSize') f.textSize = Number(set.dataset.value);
        else f.frame = set.dataset.value === '1';
      });
      return;
    }
    // Clic en el cuadro o en el texto de la fila: el texto elige el primer
    // color de la fila (o el que ya está activo si es de esa fila).
    const swatch = t.closest('[data-menu-swatch]');
    const legendRow = t.closest('[data-menu-legend-row], [data-menu-frame-row]');
    if (swatch || legendRow) {
      if (swatch) activeColorKey = swatch.dataset.menuSwatch;
      else {
        const keys = legendRow.hasAttribute('data-menu-frame-row') ? ['frame'] : legendRow.dataset.keys.split(',');
        if (!keys.includes(activeColorKey)) activeColorKey = keys[0];
      }
      syncAppearance(field);
      return;
    }
    const page = t.closest('[data-menu-page]');
    if (page) {
      const target = Number(page.dataset.menuPage);
      const busy = (editor()?.getAllFields?.() || []).some((f) => f.type === 'MENU' && f !== field && Number(f.page || 0) === target);
      if (busy) { window.JWPLCDialog.alert('Esa página ya tiene un MENU.'); return; }
      editor()?.moveSelectedFieldToPage?.(target);
      return;
    }

    const add = t.closest('[data-menu-add]');
    if (add && !add.disabled) {
      const parent = addTargetParent(field);
      mutate((f) => {
        const serial = f.nodes.length + 1;
        const node = defaultNode(add.dataset.menuAdd, parent, f.nodes, serial);
        const used = new Set([...f.nodes.map((n) => n.variable), ...(editor()?.getAllFields?.() || []).map((x) => x.variable)]);
        if (node.variable) {
          let candidate = node.variable;
          let k = 2;
          while (used.has(candidate)) candidate = `${node.variable}_${k++}`;
          node.variable = candidate;
        }
        f.nodes.push(node);
        ui(f).nodeKey = node.key;
        ui(f).editing = false;
      });
      return;
    }

    const row = t.closest('[data-menu-node]');
    if (row) {
      const key = row.dataset.menuNode;
      if (t.closest('[data-menu-remove]')) {
        mutate((f) => {
          const doomed = descendantsOf(f.nodes, key);
          f.nodes = f.nodes.filter((n) => !doomed.has(n.key));
          if (doomed.has(ui(f).nodeKey)) ui(f).nodeKey = null;
        });
        return;
      }
      const move = t.closest('[data-menu-move]');
      if (move) {
        mutate((f) => {
          const node = nodeByKey(f.nodes, key);
          const siblings = childrenOf(f.nodes, node.parent);
          const i = siblings.indexOf(node);
          const other = siblings[i + Number(move.dataset.menuMove)];
          if (!other) return;
          const a = f.nodes.indexOf(node);
          const b = f.nodes.indexOf(other);
          [f.nodes[a], f.nodes[b]] = [f.nodes[b], f.nodes[a]];
        });
        return;
      }
      ui(field).nodeKey = key;
      ui(field).editing = false;
      editor()?.render?.();
      return;
    }

    const kind = t.closest('[data-node-kind]');
    if (kind) { mutate((f) => { nodeByKey(f.nodes, ui(f).nodeKey).valueType = kind.dataset.nodeKind; }); return; }
    const decimals = t.closest('[data-node-decimals]');
    if (decimals) { mutate((f) => { nodeByKey(f.nodes, ui(f).nodeKey).decimals = Number(decimals.dataset.nodeDecimals); }); return; }
    const preset = t.closest('[data-node-bool-preset]');
    if (preset) {
      const [falseText, trueText] = preset.dataset.nodeBoolPreset.split('|');
      mutate((f) => { Object.assign(nodeByKey(f.nodes, ui(f).nodeKey), { falseText, trueText }); });
      section.querySelector('[data-menu-node-editor]').dataset.signature = '';
      return;
    }
    const initBool = t.closest('[data-node-initial-bool]');
    if (initBool) {
      mutate((f) => { nodeByKey(f.nodes, ui(f).nodeKey).initial = Number(initBool.dataset.nodeInitialBool); });
      section.querySelector('[data-menu-node-editor]').dataset.signature = '';
    }
  });

  section.addEventListener('change', (e) => {
    const t = e.target;
    if (t.matches('[data-node-preview-edit]')) {
      const field = selectedMenu();
      if (field) { ui(field).editing = t.checked; editor()?.render?.(); }
      return;
    }
  });

  const NUMERIC_PROPS = new Set(['x', 'y', 'width', 'height', 'initial', 'min', 'max', 'step']);

  function applyInput(t, commit) {
    if (t.matches('[data-menu-prop]')) {
      const prop = t.dataset.menuProp;
      mutate((f) => {
        if (prop === 'x') f.x = clamp(Math.trunc(num(t.value)), 0, SCREEN_W - 1);
        else if (prop === 'y') f.y = clamp(Math.trunc(num(t.value)), 0, SCREEN_H - 1);
        else if (prop === 'width') f.width = clamp(Math.trunc(num(t.value, 40)), 20, SCREEN_W);
        else if (prop === 'height') f.height = clamp(Math.trunc(num(t.value, 14)), 8, SCREEN_H);
        else if (prop === 'id') f.id = sanitizeSymbol(t.value, f.id).toUpperCase();
        else f[prop] = t.value;
      }, commit);
      return;
    }
    if (t.matches('[data-node-prop]')) {
      const prop = t.dataset.nodeProp;
      mutate((f) => {
        const node = nodeByKey(f.nodes, ui(f).nodeKey);
        if (!node) return;
        if (NUMERIC_PROPS.has(prop)) { if (t.value !== '' && Number.isFinite(Number(t.value))) node[prop] = Number(t.value); }
        else if (prop === 'variable') node.variable = sanitizeSymbol(t.value, '');
        else node[prop] = t.value;
      }, commit);
    }
  }

  section.addEventListener('input', (e) => applyInput(e.target, false));
  section.addEventListener('change', (e) => {
    if (e.target.matches('[data-menu-prop], [data-node-prop]')) applyInput(e.target, true);
  });

  window.addEventListener('jwplc:editor-refresh', sync);

  window.JWPLCHMIMenu = {
    createDefault,
    ensureState,
    paint,
    renderToArray,
    layout,
    generate,
    validate,
    patchHeader,
    dfsOrder,
    selectNode: (field, nodeKey, editing = false) => {
      ui(field).nodeKey = nodeKey;
      ui(field).editing = editing;
    },
    // Nodo seleccionado en el inspector (objeto vivo). Lo usa el modo OpenPLC
    // (designer-openplc-host.js) para guardar plcBinding en el nodo.
    selectedNode: (field) => (field && ui(field).nodeKey ? nodeByKey(field.nodes || [], ui(field).nodeKey) : null)
  };
})();

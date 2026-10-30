(() => {
  'use strict';

  if (window.JWPLCHMICodegen) return;

  const codeOutput = document.getElementById('codeOutput');
  const contractTab = document.getElementById('contractTab');
  const statusTab = document.getElementById('statusTab');
  const generateButton = document.getElementById('generateButton');
  const fieldIdInput = document.getElementById('fieldId');
  const fieldVariableInput = document.getElementById('fieldVariable');

  if (!codeOutput || !fieldIdInput || !fieldVariableInput) return;

  let warningFieldKey = null;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function allFields() {
    return (editor()?.getAllFields?.() || []).filter((field) => field.type !== 'NAV_BAR' && field.type !== 'MENU');
  }

  function pages() {
    return editor()?.getPages?.() || [{ id: 0, name: 'Principal' }];
  }

  function selectedField() {
    return editor()?.getSelectedField?.() || null;
  }

  function sanitizeSymbol(value, fallback) {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  }

  function sanitizePageToken(value, fallback) {
    const normalized = String(value || '')
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .trim()
      .replace(/[^A-Za-z0-9_]+/g, '_')
      .replace(/^_+|_+$/g, '')
      .toUpperCase();
    const token = normalized || fallback;
    return /^[A-Z_]/.test(token) ? token : `_${token}`;
  }

  function pageSymbols() {
    const used = new Set();
    return pages().map((page, index) => {
      const fallback = `PAGINA_${Number(page?.id ?? index) + 1}`;
      const token = sanitizePageToken(page?.name, fallback);
      let symbol = `PAGE_${token}`;
      if (used.has(symbol)) {
        symbol = `${symbol}_${Number(page?.id ?? index) + 1}`;
      }
      used.add(symbol);
      return { page, symbol };
    });
  }

  function serialFor(field, fallbackIndex = 0) {
    const match = String(field?.key || '').match(/-(\d+)$/);
    return match ? Number(match[1]) : fallbackIndex + 1;
  }

  function fieldFallbackId(field, index) {
    const serial = serialFor(field, index);
    if (field?.type === 'ICON') return `FIELD_ICON_${serial}`;
    if (field?.type === 'VALUE') return `FIELD_VALUE_${serial}`;
    if (field?.type === 'BOOL') return `FIELD_BOOL_${serial}`;
    if (field?.type === 'BAR') return `FIELD_BAR_${serial}`;
    return `FIELD_TEXT_${serial}`;
  }

  function variableFallback(field, index) {
    const serial = serialFor(field, index);
    if (field?.type === 'ICON') return `icono${serial}`;
    if (field?.type === 'VALUE') return `valor${serial}`;
    if (field?.type === 'BOOL') return `estado${serial}`;
    if (field?.type === 'BAR') return `nivel${serial}`;
    return `texto${serial}`;
  }

  function canonicalFor(field, property, value, index) {
    const fallback = property === 'id'
      ? fieldFallbackId(field, index)
      : variableFallback(field, index);
    return sanitizeSymbol(value, fallback);
  }

  function pageForField(field) {
    const pageId = Number(field?.page || 0);
    return pages().find((page) => Number(page.id) === pageId) || {
      id: pageId,
      name: `Página ${pageId + 1}`
    };
  }

  function pageComment(page) {
    const name = String(page?.name || `Página ${Number(page?.id || 0) + 1}`)
      .replace(/[\r\n]+/g, ' ')
      .trim();
    return `Página ${String(Number(page?.id || 0) + 1).padStart(2, '0')} · ${name}`;
  }

  function duplicateFor(field, property, candidate) {
    const fields = allFields();
    const currentIndex = Math.max(0, fields.indexOf(field));
    const canonical = canonicalFor(field, property, candidate, currentIndex);

    for (let index = 0; index < fields.length; index += 1) {
      const other = fields[index];
      if (other === field) continue;
      const otherCanonical = canonicalFor(other, property, other[property], index);
      if (otherCanonical === canonical) {
        return { field: other, canonical };
      }
    }

    return null;
  }

  function ensureWarningNode(input, kind) {
    const id = `a11-duplicate-${kind}`;
    let node = document.getElementById(id);
    if (node) return node;
    node = document.createElement('span');
    node.id = id;
    node.className = 'a11-duplicate-warning';
    node.hidden = true;
    input.insertAdjacentElement('afterend', node);
    return node;
  }

  function injectStyles() {
    if (document.getElementById('a11-codegen-guard-style')) return;
    const style = document.createElement('style');
    style.id = 'a11-codegen-guard-style';
    style.textContent = `
      .a11-duplicate-warning {
        display:block;
        margin-top:5px;
        color:#ff8d88;
        font-size:10px;
        line-height:1.35;
      }
      .a11-duplicate-warning[hidden] { display:none; }
      .field-input.a11-duplicate-input {
        border-color:#e45f5a !important;
        box-shadow:0 0 0 1px rgba(228,95,90,.25) inset;
      }
    `;
    document.head.appendChild(style);
  }

  function clearWarning(input, kind) {
    input.classList.remove('a11-duplicate-input');
    const node = ensureWarningNode(input, kind);
    node.hidden = true;
    node.textContent = '';
  }

  function showDuplicateWarning(input, kind, duplicate) {
    const page = pageForField(duplicate.field);
    const objectName = duplicate.field.name || duplicate.field.type || 'objeto';
    const human = kind === 'id' ? 'ID C++' : 'Variable C++';
    const node = ensureWarningNode(input, kind);
    input.classList.add('a11-duplicate-input');
    node.textContent = `No se aplicó: ${human} “${duplicate.canonical}” ya existe en ${pageComment(page)} (${objectName}).`;
    node.hidden = false;
    warningFieldKey = selectedField()?.key || null;
  }

  function guardUniqueInput(input, property, kind) {
    if (input.dataset.a11UniqueGuard === '1') return;
    input.dataset.a11UniqueGuard = '1';

    ['input', 'change'].forEach((eventName) => {
      input.addEventListener(eventName, (event) => {
        const field = selectedField();
        if (!field) return;
        const duplicate = duplicateFor(field, property, input.value);
        if (!duplicate) {
          clearWarning(input, kind);
          return;
        }

        event.preventDefault();
        event.stopImmediatePropagation();
        input.value = String(field[property] || '');
        showDuplicateWarning(input, kind, duplicate);
        editor()?.render?.();
      }, true);
    });
  }

  function validationIssues() {
    const fields = allFields();
    const issues = [];

    ['id', 'variable'].forEach((property) => {
      const seen = new Map();
      fields.forEach((field, index) => {
        // Un reloj (fecha/hora del RTC) no declara variable.
        if (property === 'variable' && isClockField(field)) return;
        const canonical = canonicalFor(field, property, field[property], index);
        const previous = seen.get(canonical);
        if (previous) {
          issues.push({ property, canonical, first: previous, second: field });
        } else {
          seen.set(canonical, field);
        }
      });
    });

    return issues;
  }

  function variableDeclaration(field, index) {
    if (['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'].includes(field.type)) return '';
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE' && window.JWPLCHMICodegenCore?.valueDeclaration) return window.JWPLCHMICodegenCore.valueDeclaration(field, variable);
    if (field.type === 'VALUE' || field.type === 'BAR') return `float ${variable} = 0.0f;`;
    if (field.type === 'BOOL') return `bool ${variable} = false;`;
    if (field.type === 'ICON') return window.JWPLCIconsCore.iconDeclaration(field, variable, window.JWPLCHMIIcons?.iconInfo?.(field.iconId)?.stateNames || window.JWPLCIconsCore.stateNamesFor(window.JWPLCIconCatalog, field.iconId));
    return window.JWPLCHMICodegenCore.textDeclaration(field, variable);
  }

  const STATIC_TYPES = ['LINE', 'RECT', 'ELLIPSE', 'TRIANGLE', 'POLYGON', 'RAW_TEXT'];

  function isStaticObject(field) {
    return STATIC_TYPES.includes(field?.type);
  }

  function hex565(value, fallback = 0xFFFF) {
    const number = Number.isFinite(Number(value)) ? Number(value) : fallback;
    return `0x${(number & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  // Alpha15 I3: bytes cp437 de la fuente GLCD (tildes, ñ, °); saltos -> espacio.
  function cppStringLiteral(value) {
    return window.JWPLCCp437.cppEscape(String(value || '').replace(/[\r\n]+/g, ' '));
  }

  // Helpers C++ que el header incluye sólo si algún objeto los usa.
  // Se implementan con primitivas que existen en libJWPLC_TFT.a
  // (drawFastHLine, drawPixel, drawLine), sin depender de TFT_eSPI.
  const STATIC_HELPERS = {
    fonts: '',
    swap: `static inline void jwplcHMISwap(int16_t &a, int16_t &b)
{
    const int16_t t = a;
    a = b;
    b = t;
}`,
    fillTriangle: `static void jwplcHMIFillTriangle(JWPLC_TFTClass &tft,
    int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.fillTriangle(x0, y0, x1, y1, x2, y2, color);
#else
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }
    if (y1 > y2) { jwplcHMISwap(y2, y1); jwplcHMISwap(x2, x1); }
    if (y0 > y1) { jwplcHMISwap(y0, y1); jwplcHMISwap(x0, x1); }

    int16_t a;
    int16_t b;
    if (y0 == y2)
    {
        a = b = x0;
        if (x1 < a) a = x1; else if (x1 > b) b = x1;
        if (x2 < a) a = x2; else if (x2 > b) b = x2;
        tft.drawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    const int32_t dx01 = x1 - x0, dy01 = y1 - y0;
    const int32_t dx02 = x2 - x0, dy02 = y2 - y0;
    const int32_t dx12 = x2 - x1, dy12 = y2 - y1;
    int32_t sa = 0;
    int32_t sb = 0;
    const int16_t last = (y1 == y2) ? y1 : (int16_t)(y1 - 1);
    int16_t y = y0;

    for (; y <= last; y++)
    {
        a = (int16_t)(x0 + sa / dy01);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx01;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }

    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++)
    {
        a = (int16_t)(x1 + sa / dy12);
        b = (int16_t)(x0 + sb / dy02);
        sa += dx12;
        sb += dx02;
        if (a > b) jwplcHMISwap(a, b);
        tft.drawFastHLine(a, y, b - a + 1, color);
    }
#endif
}`,
    fillEllipse: `static void jwplcHMIFillEllipse(JWPLC_TFTClass &tft,
    int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.fillEllipse(cx, cy, rx, ry, color);
#else
    if (rx <= 0 || ry <= 0)
    {
        tft.drawFastHLine(cx - rx, cy, 2 * rx + 1, color);
        return;
    }
    for (int16_t dy = -ry; dy <= ry; dy++)
    {
        const float t = 1.0f - ((float)dy * (float)dy) / ((float)ry * (float)ry);
        const int16_t dx = (int16_t)((float)rx * sqrtf(t > 0.0f ? t : 0.0f) + 0.5f);
        tft.drawFastHLine(cx - dx, cy + dy, 2 * dx + 1, color);
    }
#endif
}`,
    microText: `// Microfuente 3x5 (celda 4x6) usada por la barra de navegación.
static const uint8_t JWPLC_HMI_MICRO_FONT[][4] =
{
    {'0', 0x1F, 0x11, 0x1F}, {'1', 0x00, 0x1F, 0x00}, {'2', 0x1D, 0x15, 0x17},
    {'3', 0x15, 0x15, 0x1F}, {'4', 0x07, 0x04, 0x1F}, {'5', 0x17, 0x15, 0x1D},
    {'6', 0x1F, 0x15, 0x1D}, {'7', 0x01, 0x1D, 0x03}, {'8', 0x1F, 0x15, 0x1F},
    {'9', 0x17, 0x15, 0x1F}, {'A', 0x1E, 0x05, 0x1E}, {'B', 0x1F, 0x15, 0x0A},
    {'C', 0x0E, 0x11, 0x11}, {'D', 0x1F, 0x11, 0x0E}, {'E', 0x1F, 0x15, 0x11},
    {'F', 0x1F, 0x05, 0x01}, {'G', 0x0E, 0x15, 0x1D}, {'H', 0x1F, 0x04, 0x1F},
    {'I', 0x11, 0x1F, 0x11}, {'J', 0x08, 0x10, 0x0F}, {'K', 0x1F, 0x04, 0x1B},
    {'L', 0x1F, 0x10, 0x10}, {'M', 0x1F, 0x02, 0x1F}, {'N', 0x1F, 0x06, 0x1F},
    {'O', 0x0E, 0x11, 0x0E}, {'P', 0x1F, 0x05, 0x02}, {'Q', 0x0E, 0x11, 0x1E},
    {'R', 0x1F, 0x05, 0x1A}, {'S', 0x12, 0x15, 0x09}, {'T', 0x01, 0x1F, 0x01},
    {'U', 0x0F, 0x10, 0x0F}, {'V', 0x07, 0x18, 0x07}, {'W', 0x1F, 0x08, 0x1F},
    {'X', 0x1B, 0x04, 0x1B}, {'Y', 0x03, 0x1C, 0x03}, {'Z', 0x19, 0x15, 0x13},
    {':', 0x00, 0x0A, 0x00}, {'.', 0x00, 0x10, 0x00}, {'-', 0x04, 0x04, 0x04},
    {'/', 0x18, 0x06, 0x01}, {'_', 0x10, 0x10, 0x10}, {'<', 0x04, 0x0A, 0x11},
    {'>', 0x11, 0x0A, 0x04}
};

static void jwplcHMIDrawMicroText(JWPLC_TFTClass &tft, int16_t x, int16_t y, const char *text, uint16_t color)
{
    for (; *text; ++text, x += 4)
    {
        char c = *text;
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        for (size_t g = 0; g < sizeof(JWPLC_HMI_MICRO_FONT) / sizeof(JWPLC_HMI_MICRO_FONT[0]); ++g)
        {
            if (JWPLC_HMI_MICRO_FONT[g][0] != (uint8_t)c) continue;
            for (int16_t col = 0; col < 3; ++col)
                for (int16_t row = 0; row < 5; ++row)
                    if (JWPLC_HMI_MICRO_FONT[g][1 + col] & (1u << row))
                        tft.drawPixel(x + col, y + row, color);
            break;
        }
    }
}`,
    drawEllipse: `static void jwplcHMIDrawEllipse(JWPLC_TFTClass &tft,
    int16_t cx, int16_t cy, int16_t rx, int16_t ry, uint16_t color)
{
#if JWPLC_HMI_TFT_NATIVE_SHAPES
    tft.drawEllipse(cx, cy, rx, ry, color);
#else
    if (rx <= 0 || ry <= 0)
    {
        tft.drawLine(cx - rx, cy - ry, cx + rx, cy + ry, color);
        return;
    }
    // Elipse por punto medio (misma lógica que el preview del Designer).
    const int32_t rx2 = (int32_t)rx * rx;
    const int32_t ry2 = (int32_t)ry * ry;
    int32_t x = 0;
    int32_t y = ry;
    int32_t px = 0;
    int32_t py = 2 * rx2 * y;
    int32_t p = ry2 - rx2 * ry + rx2 / 4;
    while (px <= py)
    {
        tft.drawPixel(cx + x, cy + y, color);
        tft.drawPixel(cx - x, cy + y, color);
        tft.drawPixel(cx + x, cy - y, color);
        tft.drawPixel(cx - x, cy - y, color);
        x++;
        px += 2 * ry2;
        if (p < 0)
        {
            p += ry2 + px;
        }
        else
        {
            y--;
            py -= 2 * rx2;
            p += ry2 + px - py;
        }
    }
    p = (int32_t)(ry2 * (x + 0.5f) * (x + 0.5f) + rx2 * (y - 1) * (y - 1) - rx2 * ry2);
    while (y >= 0)
    {
        tft.drawPixel(cx + x, cy + y, color);
        tft.drawPixel(cx - x, cy + y, color);
        tft.drawPixel(cx + x, cy - y, color);
        tft.drawPixel(cx - x, cy - y, color);
        y--;
        py -= 2 * rx2;
        if (p > 0)
        {
            p += rx2 - py;
        }
        else
        {
            x++;
            px += 2 * ry2;
            p += rx2 - py + px;
        }
    }
#endif
}`
  };

  function shapeVertices(field) {
    const apiVerts = window.JWPLCHMIEditor?.getShapeVertices?.(field);
    if (Array.isArray(apiVerts) && apiVerts.length >= 3) {
      return apiVerts.map((p) => ({ x: Math.round(p.x), y: Math.round(p.y) }));
    }
    const cx = (field.x + field.x2) / 2;
    const cy = (field.y + field.y2) / 2;
    const rx = Math.abs(field.x2 - field.x) / 2;
    const ry = Math.abs(field.y2 - field.y) / 2;
    const numSides = field.type === 'TRIANGLE' ? 3 : field.type === 'RECT' ? 4 : Math.max(3, Math.min(20, Math.trunc(Number(field.sides) || 5)));
    const pts = [];
    for (let i = 0; i < numSides; i++) {
      const angle = (i * 2 * Math.PI / numSides) - Math.PI / 2;
      pts.push({ x: Math.round(cx + rx * Math.cos(angle)), y: Math.round(cy + ry * Math.sin(angle)) });
    }
    return pts;
  }

  function insetPolygon(pts, s) {
    if (!Array.isArray(pts) || pts.length < 3 || s <= 0) return pts ? [...pts] : null;
    const n = pts.length;
    let cx = 0, cy = 0;
    for (const p of pts) { cx += p.x; cy += p.y; }
    cx /= n; cy /= n;

    const lines = [];
    for (let i = 0; i < n; i++) {
      const p1 = pts[i];
      const p2 = pts[(i + 1) % n];
      const dx = p2.x - p1.x;
      const dy = p2.y - p1.y;
      const len = Math.hypot(dx, dy);
      if (len < 1e-4) return null;
      const ux = dx / len;
      const uy = dy / len;
      let nx = -uy, ny = ux;
      const midX = (p1.x + p2.x) / 2, midY = (p1.y + p2.y) / 2;
      if (nx * (cx - midX) + ny * (cy - midY) < 0) {
        nx = -nx; ny = -ny;
      }
      lines.push({ p: { x: p1.x + s * nx, y: p1.y + s * ny }, u: { x: ux, y: uy } });
    }

    const out = [];
    for (let i = 0; i < n; i++) {
      const prev = lines[(i - 1 + n) % n];
      const curr = lines[i];
      const det = prev.u.x * (-curr.u.y) - prev.u.y * (-curr.u.x);
      if (Math.abs(det) < 1e-5) {
        out.push({ x: curr.p.x, y: curr.p.y });
        continue;
      }
      const dx = curr.p.x - prev.p.x;
      const dy = curr.p.y - prev.p.y;
      const t = (dx * (-curr.u.y) - dy * (-curr.u.x)) / det;
      const pt = {
        x: Math.round(prev.p.x + t * prev.u.x),
        y: Math.round(prev.p.y + t * prev.u.y)
      };
      if ((pt.x - cx) * (pts[i].x - cx) + (pt.y - cy) * (pts[i].y - cy) < -1e-3) {
        return null;
      }
      out.push(pt);
    }
    return out;
  }

  // Devuelve { lines: string[], helpers: Set<string> } para un objeto estático.
  // Las líneas usan una referencia local `tft` (JWPLC_TFTClass &).
  function staticDrawCode(field) {
    const helpers = new Set();
    const lines = [];
    const c = hex565(field.frameColor);
    const fc = hex565(field.fillColor ?? field.frameColor);
    const r = Math.max(0, Math.round(Number(field.cornerRadius) || 0));
    const rot = ((Number(field.rotation) || 0) % 360 + 360) % 360;
    const borderOn = field.borderEnabled !== false;

    if (field.type === 'LINE') {
      const verts = window.JWPLCHMIEditor?.getShapeVertices?.(field);
      const p0 = Array.isArray(verts) && verts[0] ? verts[0] : { x: field.x, y: field.y };
      const p1 = Array.isArray(verts) && verts[1] ? verts[1] : { x: field.x2, y: field.y2 };
      lines.push(`tft.drawLine(${Math.round(p0.x)}, ${Math.round(p0.y)}, ${Math.round(p1.x)}, ${Math.round(p1.y)}, ${c});`);
      return { lines, helpers };
    }

    if (field.type === 'RECT' && rot === 0) {
      const minX = Math.min(field.x, field.x2);
      const minY = Math.min(field.y, field.y2);
      const w = Math.abs(field.x2 - field.x);
      const h = Math.abs(field.y2 - field.y);
      const strokeSize = Math.max(1, Math.trunc(Number(field.size) || 1));
      if (r > 0) {
        if (field.fill) lines.push(`tft.fillRoundRect(${minX}, ${minY}, ${w}, ${h}, ${r}, ${fc});`);
        if (borderOn) {
          for (let s = 0; s < strokeSize; s++) {
            const rw = w - 2 * s; const rh = h - 2 * s;
            if (rw <= 0 || rh <= 0) break;
            lines.push(`tft.drawRoundRect(${minX + s}, ${minY + s}, ${rw}, ${rh}, ${Math.max(0, r - s)}, ${c});`);
          }
        }
      } else {
        if (field.fill) lines.push(`tft.fillRect(${minX}, ${minY}, ${w}, ${h}, ${fc});`);
        if (borderOn) {
          for (let s = 0; s < strokeSize; s++) {
            const rw = w - 2 * s; const rh = h - 2 * s;
            if (rw <= 0 || rh <= 0) break;
            lines.push(`tft.drawRect(${minX + s}, ${minY + s}, ${rw}, ${rh}, ${c});`);
          }
        }
      }
      return { lines, helpers };
    }

    if (field.type === 'ELLIPSE') {
      const cx = Math.round(Math.min(field.x, field.x2) + Math.abs(field.x2 - field.x) / 2);
      const cy = Math.round(Math.min(field.y, field.y2) + Math.abs(field.y2 - field.y) / 2);
      const rx = Math.round(Math.abs(field.x2 - field.x) / 2);
      const ry = Math.round(Math.abs(field.y2 - field.y) / 2);
      const strokeSize = Math.max(1, Math.trunc(Number(field.size) || 1));
      if (rx === ry) {
        if (field.fill) lines.push(`tft.fillCircle(${cx}, ${cy}, ${rx}, ${fc});`);
        if (borderOn) {
          for (let s = 0; s < strokeSize; s++) {
            if (rx - s <= 0) break;
            lines.push(`tft.drawCircle(${cx}, ${cy}, ${rx - s}, ${c});`);
          }
        }
      } else {
        if (field.fill) {
          helpers.add('fillEllipse');
          lines.push(`jwplcHMIFillEllipse(tft, ${cx}, ${cy}, ${rx}, ${ry}, ${fc});`);
        }
        if (borderOn) {
          helpers.add('drawEllipse');
          for (let s = 0; s < strokeSize; s++) {
            if (rx - s <= 0 || ry - s <= 0) break;
            lines.push(`jwplcHMIDrawEllipse(tft, ${cx}, ${cy}, ${rx - s}, ${ry - s}, ${c});`);
          }
        }
      }
      return { lines, helpers };
    }

    if (['RECT', 'TRIANGLE', 'POLYGON'].includes(field.type)) {
      const pts = shapeVertices(field);
      if (field.fill && pts.length >= 3) {
        helpers.add('swap');
        helpers.add('fillTriangle');
        for (let i = 1; i < pts.length - 1; i++) {
          lines.push(`jwplcHMIFillTriangle(tft, ${pts[0].x}, ${pts[0].y}, ${pts[i].x}, ${pts[i].y}, ${pts[i + 1].x}, ${pts[i + 1].y}, ${fc});`);
        }
      }
      if (borderOn) {
        const strokeSize = Math.max(1, Math.trunc(Number(field.size) || 1));
        for (let s = 0; s < strokeSize; s++) {
          const layerPts = s === 0 ? pts : insetPolygon(pts, s);
          if (!layerPts) break;
          for (let i = 0; i < layerPts.length; i++) {
            const p1 = layerPts[i];
            const p2 = layerPts[(i + 1) % layerPts.length];
            lines.push(`tft.drawLine(${p1.x}, ${p1.y}, ${p2.x}, ${p2.y}, ${c});`);
          }
        }
      }
      return { lines, helpers };
    }

    if (field.type === 'RAW_TEXT') {
      const RAW_BG_PAD = 2;
      const fg = hex565(field.textColor ?? 0xFFFF);
      const bg = hex565(field.backgroundColor ?? 0x0000);
      const raw = String(field.text || '');
      const text = cppStringLiteral(raw);
      const scale = Math.max(1, Number(field.size) || 1);
      const fonts = window.JWPLCDesignerFonts;
      const requestedFont = String(field.font || 'GLCD');
      const font = fonts ? fonts.get(fonts.effectiveId(requestedFont)) : null;

      const glcdLines = (out) => {
        if (!field.transparentBackground) {
          const w = raw.length > 0 ? raw.length * 6 * scale - scale : 0;
          out.push(`tft.fillRect(${field.x - RAW_BG_PAD}, ${field.y - RAW_BG_PAD}, ${w + RAW_BG_PAD * 2}, ${7 * scale + RAW_BG_PAD * 2}, ${bg});`);
        }
        out.push(
          `tft.setCursor(${field.x}, ${field.y});`,
          `tft.setTextSize(${scale});`,
          `tft.setTextColor(${fg});`,
          `tft.print("${text}");`);
      };

      if (font && font.kind === 'free') {
        // JWPLC_TFT >= 0.3.0: FreeFont con drawString (datum arriba-izquierda,
        // igual que el lienzo). Con 0.1/0.2 se dibuja en GLCD.
        helpers.add('fonts');
        lines.push('#if JWPLC_HMI_TFT_FONTS');
        if (!field.transparentBackground && fonts) {
          const w = fonts.measureWidth(font, raw, scale);
          const h = fonts.lineHeight(font, scale);
          lines.push(`tft.fillRect(${field.x - RAW_BG_PAD}, ${field.y - RAW_BG_PAD}, ${w + RAW_BG_PAD * 2}, ${h + RAW_BG_PAD * 2}, ${bg});`);
        }
        lines.push(
          `tft.setFont(${fonts.cppSymbol(font.id)});`,
          `tft.setTextDatum(JWPLC_TFTDatum::TOP_LEFT);`,
          `tft.setTextSize(${scale});`,
          `tft.setTextColor(${fg});`,
          `tft.drawString("${text}", ${field.x}, ${field.y});`,
          `tft.setFont(JWPLC_FONT_GLCD);`,
          '#else');
        glcdLines(lines);
        lines.push('#endif');
        return { lines, helpers };
      }

      if (requestedFont !== 'GLCD') {
        lines.push(`// Fuente ${requestedFont.replace(/[^A-Za-z0-9_]/g, '')} no incluida en el firmware; se dibuja con GLCD.`);
      }
      glcdLines(lines);
      return { lines, helpers };
    }

    return { lines, helpers };
  }

  function isClockField(field) {
    return Boolean(window.JWPLCHMICodegenCore?.isClockField?.(field));
  }

  function setterHint(field, index) {
    if (isStaticObject(field) || isClockField(field)) return '';
    const id = canonicalFor(field, 'id', field.id, index);
    const variable = canonicalFor(field, 'variable', field.variable, index);
    if (field.type === 'VALUE') return `JWPLC_Display.setValue(${id}, ${variable});`;
    if (field.type === 'BOOL') return `JWPLC_Display.setBool(${id}, ${variable});`;
    if (field.type === 'ICON') return window.JWPLCIconsCore.iconSetter(id, variable);
    if (field.type === 'BAR') return `JWPLC_Display.setBar(${id}, ${variable});`;
    return `JWPLC_Display.setText(${id}, ${variable});`;
  }

  function groupedChunks(renderer, commentIndent = '') {
    const fields = allFields();
    const chunks = [];

    pages().forEach((page) => {
      const entries = fields
        .map((field, index) => ({ field, index }))
        .filter(({ field }) => Number(field.page || 0) === Number(page.id));
      if (!entries.length) return;
      const lines = entries.map(({ field, index }) => renderer(field, index)).filter(Boolean);
      if (!lines.length) return;
      chunks.push(`${commentIndent}// ${pageComment(page)}\n${lines.join('\n')}`);
    });

    return chunks.join('\n\n');
  }

  function css565(col) {
    const text = String(col || '').trim();
    let r = 0, g = 0, b = 0;
    let m = text.match(/^#([0-9a-f]{6})$/i);
    if (m) {
      const n = parseInt(m[1], 16);
      r = (n >> 16) & 0xFF; g = (n >> 8) & 0xFF; b = n & 0xFF;
    } else if ((m = text.match(/^#([0-9a-f]{3})$/i))) {
      r = parseInt(m[1][0] + m[1][0], 16); g = parseInt(m[1][1] + m[1][1], 16); b = parseInt(m[1][2] + m[1][2], 16);
    } else if ((m = text.match(/^rgba?\(\s*([\d.]+)\s*,\s*([\d.]+)\s*,\s*([\d.]+)/i))) {
      r = Math.round(Number(m[1])); g = Math.round(Number(m[2])); b = Math.round(Number(m[3]));
    }
    return hex565(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
  }

  // Traduce las operaciones de JWPLCHMIPages.recordNavBar() a llamadas de
  // JWPLC_TFT. Mismo recorrido que el lienzo -> misma imagen en la TFT.
  function navBarOpsToCpp(ops, helpers) {
    const lines = [];
    const radius = (op) => Math.max(0, Math.min(Math.floor(Number(op.r) || 0), Math.floor(op.w / 2), Math.floor(op.h / 2)));
    ops.forEach((op) => {
      const x = Math.round(op.x), y = Math.round(op.y);
      if (op.op === 'fillRound') {
        const r = radius(op);
        lines.push(r > 0
          ? `tft.fillRoundRect(${x}, ${y}, ${Math.round(op.w)}, ${Math.round(op.h)}, ${r}, ${css565(op.col)});`
          : `tft.fillRect(${x}, ${y}, ${Math.round(op.w)}, ${Math.round(op.h)}, ${css565(op.col)});`);
      } else if (op.op === 'strokeRound') {
        const r = radius(op);
        lines.push(r > 0
          ? `tft.drawRoundRect(${x}, ${y}, ${Math.round(op.w)}, ${Math.round(op.h)}, ${r}, ${css565(op.col)});`
          : `tft.drawRect(${x}, ${y}, ${Math.round(op.w)}, ${Math.round(op.h)}, ${css565(op.col)});`);
      } else if (op.op === 'fillRect') {
        lines.push(`tft.fillRect(${x}, ${y}, ${Math.round(op.w)}, ${Math.round(op.h)}, ${css565(op.col)});`);
      } else if (op.op === 'line') {
        lines.push(op.vertical
          ? `tft.drawFastVLine(${x}, ${y}, ${Math.round(op.len)}, ${css565(op.col)});`
          : `tft.drawFastHLine(${x}, ${y}, ${Math.round(op.len)}, ${css565(op.col)});`);
      } else if (op.op === 'text' && op.str) {
        if (op.micro) {
          helpers.add('microText');
          lines.push(`jwplcHMIDrawMicroText(tft, ${x}, ${y}, "${cppStringLiteral(op.str)}", ${css565(op.col)});`);
        } else {
          lines.push(`tft.setCursor(${x}, ${y});`);
          lines.push(`tft.setTextColor(${css565(op.col)});`);
          lines.push(`tft.print("${cppStringLiteral(op.str)}");`);
        }
      }
    });
    return lines;
  }

  // Separa las operaciones de una página en la barra sin la pestaña activa y
  // la pestaña activa (marcas {op:'mark'} de recordNavBar).
  function splitNavOps(ops) {
    const bar = [];
    const tab = [];
    let inActive = false;
    ops.forEach((op) => {
      if (op.op === 'mark') { if (op.active) inActive = op.kind === 'start'; return; }
      (inActive ? tab : bar).push(op);
    });
    return { bar, tab };
  }

  function navBarFunctionBlock() {
    const hmiPages = window.JWPLCHMIPages;
    const pageList = pages();
    const helpers = new Set();
    if (!hmiPages?.recordNavBar || !hasNavBar()) return { block: '', helpers: [] };

    const indent = (body, pad) => body.map((line) => `${pad}${line}`);
    const tabLines = [];
    const barLines = [];
    pageList.forEach((page) => {
      const positioned = splitNavOps(hmiPages.recordNavBar(page.id, 'positioned'));
      const selected = splitNavOps(hmiPages.recordNavBar(page.id, 'selected'));
      tabLines.push(`    case ${Number(page.id)}:`);
      tabLines.push(`        if (selecting)`, `        {`);
      tabLines.push(...indent(navBarOpsToCpp(positioned.tab, helpers), '            '));
      tabLines.push(`        }`, `        else`, `        {`);
      tabLines.push(...indent(navBarOpsToCpp(selected.tab, helpers), '            '));
      tabLines.push(`        }`, `        break;`);
      barLines.push(`    case ${Number(page.id)}:`);
      barLines.push(...indent(navBarOpsToCpp(positioned.bar, helpers), '        '));
      barLines.push(`        break;`);
    });

    const lines = [];
    lines.push(`// Barra de Navegación HMI Autogenerada (ST7789)`);
    lines.push(`// Pestaña de la página actual: posicionada en el selector (◀ ▶, fondo de`);
    lines.push(`// color) o seleccionada dentro de la página (OK, fondo blanco y doble marco).`);
    lines.push(`static void jwplcDrawPageNavTab(uint8_t activePage, bool selecting)`);
    lines.push(`{`);
    lines.push(`    JWPLC_TFTClass &tft = JWPLC_Display.tft();`);
    lines.push(`    tft.setTextSize(1);`);
    lines.push(``);
    lines.push(`    switch (activePage)`);
    lines.push(`    {`);
    lines.push(...tabLines);
    lines.push(`    default:`);
    lines.push(`        break;`);
    lines.push(`    }`);
    lines.push(`}`);
    lines.push(``);
    lines.push(`// Se dibuja una vez por página desde jwplcUIPageEnter().`);
    lines.push(`static void jwplcDrawPageNavBar(uint8_t activePage)`);
    lines.push(`{`);
    lines.push(`    JWPLC_TFTClass &tft = JWPLC_Display.tft();`);
    lines.push(`    tft.setTextSize(1);`);
    lines.push(``);
    lines.push(`    switch (activePage)`);
    lines.push(`    {`);
    lines.push(...barLines);
    lines.push(`    default:`);
    lines.push(`        break;`);
    lines.push(`    }`);
    lines.push(``);
    lines.push(`    jwplcDrawPageNavTab(activePage, JWPLC_Display.isUserPageSelection());`);
    lines.push(`}`);
    lines.push(``);
    lines.push(`// Selector <-> contenido sin cambiar de página: el firmware redibuja sólo la`);
    lines.push(`// pestaña activa, con el bus tomado (Alpha15).`);
    lines.push(`extern "C" void jwplcUIPageModeChanged(uint8_t page, bool selecting)`);
    lines.push(`{`);
    lines.push(`    jwplcDrawPageNavTab(page, selecting);`);
    lines.push(`}`);
    return { block: lines.join('\n'), helpers: [...helpers] };
  }

  // Barra con pestañas dibujada por el Designer. El estilo "Contador" no
  // cuenta: lo dibuja el firmware (indicador NN/TT de Alpha11).
  function hasNavBar() {
    const cfg = window.JWPLCHMIPages?.getNavBarConfig?.();
    return Boolean(cfg?.enabled && cfg.preset !== 'counter' && pages().length > 1);
  }

  // Indicador NN/TT del firmware: en su sitio por defecto sin barra, movido
  // con el estilo "Contador" y oculto con una barra de pestañas (que ya
  // muestra la página activa). Idempotente: el texto puede llegar ya parchado.
  const INDICATOR_MARK = '    // Indicador NN/TT (barra de navegación)\n';
  function patchPageIndicator(text) {
    const at = text.indexOf(INDICATOR_MARK);
    if (at >= 0) text = text.slice(0, at) + text.slice(text.indexOf('#endif\n', at) + '#endif\n'.length);
    const cfg = window.JWPLCHMIPages?.getNavBarConfig?.();
    if (!cfg?.enabled || pages().length <= 1) return text;
    const call = cfg.preset === 'counter'
      ? `JWPLC_Display.setPageIndicator(true, ${Math.round(Number(cfg.x) || 0)}, ${Math.round(Number(cfg.y) || 0)});`
      : 'JWPLC_Display.setPageIndicator(false);';
    const block = `${INDICATOR_MARK}#ifdef JWPLC_UI_HAS_PAGE_INDICATOR_CONFIG\n    ${call}\n#endif\n`;
    return text.replace(/(    JWPLC_Display\.setUserPage\([^)]*\);\n)/, `$1${block}`);
  }

  function generatedUIUpdateBlock() {
    const fields = allFields();
    const symbols = new Map(
      pageSymbols().map(({ page, symbol }) => [Number(page.id), symbol]));
    const cases = [];

    pages().forEach((page) => {
      const lines = fields
        .map((field, index) => ({ field, index }))
        .filter(({ field }) => Number(field.page || 0) === Number(page.id) && !isStaticObject(field) && !isClockField(field))
        .map(({ field, index }) => `        ${setterHint(field, index)}`);
      if (!lines.length) return;

      const symbol = symbols.get(Number(page.id)) || String(Number(page.id));
      cases.push(
        `    case ${symbol}:\n` +
        `        // ${pageComment(page)}\n` +
        `${lines.join('\n')}\n` +
        `        break;`);
    });

    const body = cases.length
      ? `    switch (JWPLC_Display.userPage())\n` +
        `    {\n` +
        `${cases.join('\n\n')}\n\n` +
        `    default:\n` +
        `        break;\n` +
        `    }\n`
      : `    // Sin campos dinámicos.\n`;

    return `// Sincronización gráfica autogenerada (cada tick de refresco).\n` +
      `// Sólo setters: el runtime redibuja únicamente los campos que cambiaron.\n` +
      `// La lógica de aplicación debe permanecer en loop().\n` +
      `void jwplcUIUpdate()\n` +
      `{\n` +
      body +
      `}`;
  }

  // Gráfica estática: se dibuja una sola vez al entrar a cada página.
  // El runtime llama jwplcUIPageEnter() con el bus TFT tomado (batch),
  // después de limpiar la pantalla y antes de dibujar los campos.
  function generatedPageEnterBlock() {
    const fields = allFields();
    const symbols = new Map(
      pageSymbols().map(({ page, symbol }) => [Number(page.id), symbol]));
    const helpers = new Set();
    const cases = [];

    pages().forEach((page) => {
      const lines = [];
      fields
        .filter((field) => Number(field.page || 0) === Number(page.id) && isStaticObject(field) && !field.hidden)
        // En el lienzo, layerOrder menor = más al frente (app.js composeFramebuffer
        // recorre el arreglo al revés). Se dibuja de atrás hacia adelante.
        .sort((a, b) => (Number(b.layerOrder) || 0) - (Number(a.layerOrder) || 0))
        .forEach((field) => {
          const code = staticDrawCode(field);
          code.helpers.forEach((name) => helpers.add(name));
          if (!code.lines.length) return;
          lines.push(`        // ${String(field.name || field.type).replace(/[\r\n]+/g, ' ')}`);
          code.lines.forEach((line) => lines.push(`        ${line}`));
        });
      if (!lines.length) return;
      const symbol = symbols.get(Number(page.id)) || String(Number(page.id));
      cases.push(
        `    case ${symbol}:\n` +
        `${lines.join('\n')}\n` +
        `        break;`);
    });

    const navBar = hasNavBar();
    if (!cases.length && !navBar) return { block: '', helpers: [] };

    const parts = [];
    parts.push(`// Gráfica estática por página (se dibuja una vez al entrar a la página).`);
    parts.push(`// Para lógica propia al cambiar de página, define en el sketch:`);
    parts.push(`//   extern "C" void jwplcHMIUserPageEnter(uint8_t page) { ... }`);
    parts.push(`extern "C" void jwplcHMIUserPageEnter(uint8_t page) __attribute__((weak));`);
    parts.push(``);
    parts.push(`void jwplcUIPageEnter(uint8_t page)`);
    parts.push(`{`);
    if (cases.length) {
      parts.push(`    JWPLC_TFTClass &tft = JWPLC_Display.tft();`);
      parts.push(``);
      parts.push(`    switch (page)`);
      parts.push(`    {`);
      parts.push(cases.join('\n\n'));
      parts.push(``);
      parts.push(`    default:`);
      parts.push(`        break;`);
      parts.push(`    }`);
    }
    if (navBar) {
      parts.push(``);
      parts.push(`    jwplcDrawPageNavBar(page);`);
    }
    parts.push(``);
    parts.push(`    if (jwplcHMIUserPageEnter)`);
    parts.push(`    {`);
    parts.push(`        jwplcHMIUserPageEnter(page);`);
    parts.push(`    }`);
    parts.push(`}`);

    const order = ['fonts', 'swap', 'fillTriangle', 'fillEllipse', 'drawEllipse', 'microText'];
    return { block: parts.join('\n'), helpers: order.filter((name) => helpers.has(name)) };
  }

  // JWPLC_TFT >= 0.2.0 trae fillEllipse/drawEllipse/fillTriangle nativos.
  // Con 0.1.0 (package alpha12 publicado) se usa la implementación de respaldo.
  // JWPLC_TFT >= 0.3.0 trae fuentes (setFont/drawString, JWPLC_FONT_*).
  const NATIVE_SHAPES_GUARD = `#if defined(JWPLC_TFT_VERSION_MAJOR) && \\
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 2))
#define JWPLC_HMI_TFT_NATIVE_SHAPES 1
#else
#define JWPLC_HMI_TFT_NATIVE_SHAPES 0
#endif
#if defined(JWPLC_TFT_VERSION_MAJOR) && \\
    ((JWPLC_TFT_VERSION_MAJOR > 0) || (JWPLC_TFT_VERSION_MINOR >= 3))
#define JWPLC_HMI_TFT_FONTS 1
#else
#define JWPLC_HMI_TFT_FONTS 0
#endif`;

  function helpersBlock(names) {
    if (!names.length) return '';
    return `// Primitivas auxiliares (sólo usan API pública de JWPLC_TFT).\n` +
      `${NATIVE_SHAPES_GUARD}\n\n` +
      names.map((name) => STATIC_HELPERS[name]).filter(Boolean).join('\n\n');
  }

  function pageEnumBlock() {
    const lines = pageSymbols().map(({ page, symbol }) =>
      `    ${symbol} = ${Number(page?.id || 0)}`);
    return `enum HMIPageId : uint8_t\n{\n${lines.join(',\n')}\n};`;
  }

  function decorateHeader(text) {
    text = text
      .replace('// API pública JWPLC_UI · Alpha11 A11-3E', '// API pública JWPLC_UI · Alpha11 A11-4')
      .replace('// API pública JWPLC_UI · Alpha11 A11-3D', '// API pública JWPLC_UI · Alpha11 A11-4');

    if (text.includes('#pragma once')) return text;

    const marker = '// API pública JWPLC_UI · Alpha11 A11-4';
    const replacement = `${marker}\n// Destino recomendado: JWPLC_HMI_Generated.h\n// Archivo autogenerado: regenerar desde el Designer en lugar de editar a mano.\n\n#pragma once\n\n#include <JWPLC_Display.h>`;
    return text.replace(marker, replacement);
  }

  function groupGeneratedSections(text) {
    // Sólo los campos del runtime (TEXT/VALUE/BOOL/BAR) llevan HMIFieldId,
    // numerados de forma consecutiva.
    let fieldSerial = 0;
    const enumLines = groupedChunks((field, index) => {
      if (isStaticObject(field)) return '';
      fieldSerial += 1;
      const id = canonicalFor(field, 'id', field.id, index);
      return `    ${id} = ${fieldSerial},`;
    }, '    ');

    const variables = groupedChunks((field, index) => variableDeclaration(field, index));
    const navBar = navBarFunctionBlock();
    const pageEnter = generatedPageEnterBlock();
    const uiUpdate = generatedUIUpdateBlock();
    const helperNames = [...new Set([...pageEnter.helpers, ...navBar.helpers])];
    const fullUiBlock = [helpersBlock(helperNames), navBar.block, pageEnter.block, uiUpdate]
      .filter(Boolean)
      .join('\n\n');
    const pageEnum = pageEnumBlock();

    if (/enum HMIPageId : uint8_t\n\{[\s\S]*?\n\};/.test(text)) {
      text = text.replace(
        /enum HMIPageId : uint8_t\n\{[\s\S]*?\n\};/,
        pageEnum);
    } else {
      text = text.replace(
        /enum HMIFieldId : uint8_t/,
        `${pageEnum}\n\nenum HMIFieldId : uint8_t`);
    }

    text = text.replace(
      /enum HMIFieldId : uint8_t\n\{\n[\s\S]*?\n\};/,
      `enum HMIFieldId : uint8_t\n{\n${enumLines}\n};`);

    text = text.replace(
      /\/\/ Variables HMI\n[\s\S]*?\n\n\/\/ Definición declarativa/,
      `// Variables HMI\n${variables}\n\n// Definición declarativa`);

    // Sin punto final: el bloque emitido dice "autogenerada (cada tick ...)".
    // Con punto, una segunda pasada sobre un header sin gráfica estática
    // duplicaba jwplcUIUpdate().
    const generatedTailMarker = text.indexOf('// Sincronización gráfica autogenerada');
    const navBarTailMarker = text.indexOf('// Barra de Navegación HMI Autogenerada');
    const helpersTailMarker = text.indexOf('// Primitivas auxiliares (sólo usan API pública de JWPLC_TFT).');
    const pageEnterTailMarker = text.indexOf('// Gráfica estática por página');
    const legacyTailMarker = text.indexOf('// jwplcUIUpdate() NO se genera.');
    const settersTailMarker = text.indexOf('// Setters públicos que corresponden a este diseño:');
    const tailCandidates = [generatedTailMarker, navBarTailMarker, helpersTailMarker, pageEnterTailMarker, legacyTailMarker, settersTailMarker]
      .filter((value) => value >= 0);

    if (tailCandidates.length) {
      const tailStart = Math.min(...tailCandidates);
      text = `${text.slice(0, tailStart).trimEnd()}\n\n${fullUiBlock}`;
    } else {
      text = `${text.trimEnd()}\n\n${fullUiBlock}`;
    }

    if (!text.includes('JWPLC_Display.setUserRefreshMode(')) {
      text = text.replace(
        /(JWPLC_Display\.setFields\([\s\S]*?\);\n)/,
        `$1    JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);\n`);
    }

    const firstPage = pageSymbols()[0];
    if (firstPage) {
      text = text.replace(
        /JWPLC_Display\.setUserPage\([^)]*\);/,
        `JWPLC_Display.setUserPage(${firstPage.symbol});`);
    }

    return text;
  }

  function duplicateDiagnostic(text, issues) {
    if (!issues.length) return text;
    const lines = issues.map((issue) => {
      const firstPage = pageForField(issue.first);
      const secondPage = pageForField(issue.second);
      const type = issue.property === 'id' ? 'ID C++' : 'Variable C++';
      return `// - ${type} ${issue.canonical}: ${pageComment(firstPage)} (${issue.first.name || issue.first.type}) <-> ${pageComment(secondPage)} (${issue.second.name || issue.second.type})`;
    });
    return `// ERROR CODEGEN: existen identificadores C++ duplicados. Corrígelos en el Inspector.\n${lines.join('\n')}\n\n${text}`;
  }

  function patchGeneratedCode() {
    if (!codeOutput?.textContent.startsWith('// Código generado por JWPLC HMI Designer')) return;
    let text = decorateHeader(codeOutput.textContent);
    text = groupGeneratedSections(text);
    if (window.JWPLCHMIMenu?.patchHeader) text = window.JWPLCHMIMenu.patchHeader(text);
    text = patchPageIndicator(text);
    if (window.JWPLCHMIIdle?.patchHeader) text = window.JWPLCHMIIdle.patchHeader(text);
    text = duplicateDiagnostic(text, validationIssues());
    codeOutput.textContent = text;
  }

  function patchUI() {
    injectStyles();
    const selectedKey = selectedField()?.key || null;
    if (warningFieldKey && warningFieldKey !== selectedKey) {
      clearWarning(fieldIdInput, 'id');
      clearWarning(fieldVariableInput, 'variable');
      warningFieldKey = null;
    }
    patchGeneratedCode();
  }

  guardUniqueInput(fieldIdInput, 'id', 'id');
  guardUniqueInput(fieldVariableInput, 'variable', 'variable');

  window.addEventListener('jwplc:editor-refresh', patchUI);
  contractTab?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));
  statusTab?.addEventListener('click', () => setTimeout(patchUI, 0));
  generateButton?.addEventListener('click', () => setTimeout(patchGeneratedCode, 0));

  patchUI();

  window.JWPLCHMICodegen = {
    validateIdentifiers: validationIssues,
    // Fragmento C++ de un objeto estático, tal como se emite en jwplcUIPageEnter().
    staticDrawSnippet: (field) => {
      const code = staticDrawCode(field);
      return ['JWPLC_TFTClass &tft = JWPLC_Display.tft();', ...code.lines].join('\n');
    },
    getPageSymbols: () => pageSymbols().map(({ page, symbol }) => ({ id: Number(page.id), name: page.name, symbol })),
    refresh: patchGeneratedCode
  };
})();

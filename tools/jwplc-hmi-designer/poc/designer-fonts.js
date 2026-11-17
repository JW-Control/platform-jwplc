(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCDesignerFonts = api;
})(typeof self !== 'undefined' ? self : (typeof globalThis !== 'undefined' ? globalThis : this), () => {
  'use strict';

  // Catalogo de fuentes para RAW_TEXT. Los ids coinciden con las constantes
  // JWPLC_FONT_<id> de JWPLC_TFT 0.3.0 (TFT_eSPI como backend).
  //
  // - GLCD: bitmaps reales (gfx-classic-font.js).
  // - FreeFonts (SANS/SERIF/MONO 9 y 12 pt): glifos REALES de TFT_eSPI
  //   (designer-fonts-data.js, generado por scripts/generate-gfx-fonts.js).
  // - FONT2 / FONT4: glifos REALES de TFT_eSPI (Font16.c / Font32rle.c).
  // - Si faltan los datos, se aproxima con fuentes del navegador a 1 bit.

  const DPI_FACTOR = 141 / 72;
  const env = typeof window !== 'undefined' ? window : (typeof globalThis !== 'undefined' ? globalThis : {});
  const DATA = env.JWPLCGfxFontData || {};
  const BMP = env.JWPLCTftBitmapFontData || {};

  const FONTS = [
    { id: 'GLCD', group: 'Bitmap', label: 'Clasica 5x7 (GLCD)', kind: 'glcd', lineHeight: 8 },
    { id: 'FONT_16', group: 'Bitmap', label: 'Bitmap 16 px (Font 2)', kind: 'bitmap', css: 'sans-serif', px: 14, lineHeight: 16, exact: Boolean(BMP.FONT2 || BMP.FONT_16) },
    { id: 'FONT_26', group: 'Bitmap', label: 'Bitmap 26 px (Font 4)', kind: 'bitmap', css: 'sans-serif', px: 24, lineHeight: 26, exact: Boolean(BMP.FONT4 || BMP.FONT_26) },
    { id: 'FONT_7SEG_48', group: 'Display', label: '7-Segmentos 48 px', kind: 'free', css: 'monospace', px: 48, lineHeight: 48, exact: Boolean(DATA.FONT_7SEG_48 || DATA['7SEG_48']), numericOnly: true }
  ];

  const FAMILIES = [
    { key: 'SANS',  label: 'Sans',  css: 'Arial, Helvetica, sans-serif' },
    { key: 'SERIF', label: 'Serif', css: '"Times New Roman", Times, serif' },
    { key: 'MONO',  label: 'Mono',  css: '"Courier New", Courier, monospace' }
  ];

  // yAdvance real de TFT_eSPI FreeFonts (px a escala 1x):
  //   9pt → 18  | 12pt → 24  | 18pt → 34  | 24pt → 46
  const LINE_HEIGHTS = { 9: 18, 12: 24, 18: 34, 24: 46 };

  FAMILIES.forEach((family) => {
    [9, 12, 18, 24].forEach((pt) => {
      [false, true].forEach((bold) => {
        const id = `${family.key}_${bold ? 'BOLD_' : ''}${pt}`;
        FONTS.push({
          id,
          group: family.label,
          label: `${family.label}${bold ? ' Bold' : ''} ${pt}pt`,
          kind:  'free',
          css:   family.css,
          bold,
          px:        pt * DPI_FACTOR,
          lineHeight: LINE_HEIGHTS[pt],
          exact: Boolean(DATA[id])
        });
      });
    });
  });

  const ALIASES = {
    'FONT2': 'FONT_16',
    'FONT4': 'FONT_26',
    '7SEG_48': 'FONT_7SEG_48'
  };

  const byId = new Map(FONTS.map((font) => [font.id, font]));
  Object.entries(ALIASES).forEach(([alias, target]) => {
    const targetFont = byId.get(target);
    if (targetFont) byId.set(alias, targetFont);
  });

  function get(id) {
    if (!id) return byId.get('GLCD');
    const resolved = ALIASES[id] || id;
    return byId.get(resolved) || byId.get('GLCD');
  }

  function normalizeId(id) {
    if (!id) return 'GLCD';
    const resolved = ALIASES[id] || id;
    return byId.has(resolved) ? resolved : 'GLCD';
  }

  // JWPLC_TFT >= 0.5.0 soporta GLCD, FreeFonts, FONT_16, FONT_26 y FONT_7SEG_48.
  function isSupported(fontOrId) {
    const font = typeof fontOrId === 'string' ? get(fontOrId) : fontOrId;
    return Boolean(font) && (font.kind === 'glcd' || font.kind === 'free' || font.kind === 'bitmap');
  }

  function availableFontsFor(targetType) {
    const isNumeric = targetType === 'VALUE' || targetType === 'value' || targetType === 'numeric';
    return FONTS.filter((font) => isNumeric || !font.numericOnly);
  }

  // Id que realmente se dibuja (en el lienzo y en la TFT).
  function effectiveId(id) {
    const font = get(id);
    return isSupported(font) ? font.id : 'GLCD';
  }

  // Compatibilidad con proyectos antiguos: sin campo font => GLCD.
  function fieldFont(field) {
    return get(effectiveId(field && field.font));
  }

  // ---- Datos reales GFXfont -------------------------------------------------
  const decoded = new Map();

  function gfx(font) {
    if (!font.exact || font.kind !== 'free') return null;
    let entry = decoded.get(font.id);
    if (entry) return entry;
    const raw = DATA[font.id] || (font.id === 'FONT_7SEG_48' ? DATA['7SEG_48'] : null);
    const binary = atob(raw.bitmap);
    const bitmap = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i += 1) bitmap[i] = binary.charCodeAt(i);
    // Igual que TFT_eSPI::setFreeFont: mayor ascenso/descenso de los glifos
    // imprimibles con alto > 2.
    let ascent = 0;
    let descent = 0;
    for (let code = 32; code <= Math.min(raw.last, 126); code += 1) {
      if (code < raw.first) continue;
      const [, , height, , , yOffset] = raw.glyphs[code - raw.first];
      if (height > 2) {
        ascent = Math.max(ascent, -yOffset);
        descent = Math.max(descent, height + yOffset);
      }
    }
    entry = { raw, bitmap, ascent, descent };
    decoded.set(font.id, entry);
    return entry;
  }

  // Fonts 2 y 4 de TFT_eSPI: w*h bits contiguos por glifo, ancho variable.
  const decodedBitmap = new Map();

  function bmp(font) {
    if (!font.exact || font.kind !== 'bitmap') return null;
    let entry = decodedBitmap.get(font.id);
    if (entry) return entry;
    const raw = BMP[font.id] || (font.id === 'FONT_16' ? BMP.FONT2 : (font.id === 'FONT_26' ? BMP.FONT4 : null));
    const binary = atob(raw.bitmap);
    const bitmap = new Uint8Array(binary.length);
    for (let i = 0; i < binary.length; i += 1) bitmap[i] = binary.charCodeAt(i);
    entry = { raw, bitmap };
    decodedBitmap.set(font.id, entry);
    return entry;
  }

  function bitmapIndex(data, character) {
    const index = character.charCodeAt(0) - data.raw.first;
    return index >= 0 && index < data.raw.widths.length ? index : -1;
  }

  function glyphOf(data, character) {
    const code = character.charCodeAt(0);
    if (code < data.raw.first || code > data.raw.last) return null;
    return data.raw.glyphs[code - data.raw.first];
  }

  // ---- Aproximacion con canvas (FONT2 / FONT4) ------------------------------
  let scratch = null;
  function ctx2d() {
    if (typeof document === 'undefined') return null;
    if (!scratch) {
      scratch = document.createElement('canvas').getContext('2d', { willReadFrequently: true });
    }
    return scratch;
  }

  function cssFont(font, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    return `${font.bold ? 'bold ' : ''}${Math.round(font.px * scale)}px ${font.css}`;
  }

  // ---- API ------------------------------------------------------------------
  function measureWidth(font, text, size) {
    if (!text) return 0;
    const scale = Math.max(1, Math.trunc(size || 1));
    if (font.kind === 'glcd') return text.length * 6 * scale - scale;
    const bitmapData = bmp(font);
    if (bitmapData) {
      let width = 0;
      for (const character of text) {
        const index = bitmapIndex(bitmapData, character);
        if (index >= 0) width += bitmapData.raw.widths[index] * scale;
      }
      return width;
    }
    const data = gfx(font);
    if (data) {
      let width = 0;
      for (const character of text) {
        const glyph = glyphOf(data, character);
        if (glyph) width += glyph[3] * scale;
      }
      return width;
    }
    const ctx = ctx2d();
    if (!ctx) {
      const charW = (font.px || 8) * 0.6 * scale;
      return Math.ceil(text.length * charW);
    }
    ctx.font = cssFont(font, scale);
    return Math.ceil(ctx.measureText(text).width);
  }

  function lineHeight(font, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    if (font.kind === 'glcd') return 7 * scale;
    const bitmapData = bmp(font);
    if (bitmapData) return bitmapData.raw.height * scale;
    const data = gfx(font);
    if (data) return (data.ascent + data.descent) * scale;
    return font.lineHeight * scale;
  }

  const maskCache = new Map();

  function rasterizeExact(font, data, text, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    const width = Math.max(1, measureWidth(font, text, scale) + 2 * scale);
    const height = Math.max(1, lineHeight(font, scale));
    const mask = new Uint8Array(width * height);
    const baseline = data.ascent * scale;
    let cursor = 0;
    for (const character of text) {
      const glyph = glyphOf(data, character);
      if (!glyph) continue;
      const [offset, glyphWidth, glyphHeight, xAdvance, xOffset, yOffset] = glyph;
      let bit = 0;
      let bits = 0;
      let index = offset;
      for (let row = 0; row < glyphHeight; row += 1) {
        for (let column = 0; column < glyphWidth; column += 1) {
          if (!(bit & 7)) bits = data.bitmap[index++];
          bit += 1;
          if (bits & 0x80) {
            const px = cursor + (xOffset + column) * scale;
            const py = baseline + (yOffset + row) * scale;
            for (let dy = 0; dy < scale; dy += 1) {
              for (let dx = 0; dx < scale; dx += 1) {
                const x = px + dx;
                const y = py + dy;
                if (x >= 0 && x < width && y >= 0 && y < height) mask[y * width + x] = 1;
              }
            }
          }
          bits <<= 1;
        }
      }
      cursor += xAdvance * scale;
    }
    return { width, height, data: mask };
  }

  function rasterizeBitmap(font, data, text, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    const rows = data.raw.height;
    const width = Math.max(1, measureWidth(font, text, scale));
    const height = rows * scale;
    const mask = new Uint8Array(width * height);
    let cursor = 0;
    for (const character of text) {
      const index = bitmapIndex(data, character);
      if (index < 0) continue;
      const glyphWidth = data.raw.widths[index];
      const base = data.raw.offsets[index];
      for (let row = 0; row < rows; row += 1) {
        for (let column = 0; column < glyphWidth; column += 1) {
          const bit = row * glyphWidth + column;
          if (!((data.bitmap[base + (bit >> 3)] >> (7 - (bit & 7))) & 1)) continue;
          for (let dy = 0; dy < scale; dy += 1) {
            for (let dx = 0; dx < scale; dx += 1) {
              const x = cursor + column * scale + dx;
              const y = row * scale + dy;
              if (x < width && y < height) mask[y * width + x] = 1;
            }
          }
        }
      }
      cursor += glyphWidth * scale;
    }
    return { width, height, data: mask };
  }

  function rasterizeApprox(font, text, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    const width = Math.max(1, measureWidth(font, text, scale) + 2);
    const height = font.lineHeight * scale;
    const canvas = document.createElement('canvas');
    canvas.width = width;
    canvas.height = height;
    const ctx = canvas.getContext('2d', { willReadFrequently: true });
    ctx.font = cssFont(font, scale);
    ctx.textBaseline = 'alphabetic';
    ctx.fillStyle = '#fff';
    const ascent = Math.round(ctx.measureText('d').actualBoundingBoxAscent || font.px * 0.73);
    ctx.fillText(text, 0, ascent);
    const image = ctx.getImageData(0, 0, width, height).data;
    const data = new Uint8Array(width * height);
    for (let i = 0; i < data.length; i += 1) {
      data[i] = image[i * 4 + 3] >= 110 ? 1 : 0;
    }
    return { width, height, data };
  }

  // Devuelve { width, height, data(Uint8Array 0/1) } con el texto rasterizado
  // anclado por la esquina superior izquierda (TL_DATUM de TFT_eSPI).
  function rasterize(font, text, size) {
    const scale = Math.max(1, Math.trunc(size || 1));
    const key = `${font.id}|${scale}|${text}`;
    const cached = maskCache.get(key);
    if (cached) return cached;
    const gfxData = gfx(font);
    const bitmapData = bmp(font);
    const result = gfxData
      ? rasterizeExact(font, gfxData, text, scale)
      : (bitmapData ? rasterizeBitmap(font, bitmapData, text, scale) : rasterizeApprox(font, text, scale));
    if (maskCache.size > 200) maskCache.clear();
    maskCache.set(key, result);
    return result;
  }

  function fontFamilyOf(id) {
    if (!id || id === 'GLCD') return 'GLCD';
    const norm = normalizeId(id);
    if (norm === 'FONT_16' || norm === 'FONT_26') return 'BITMAP';
    if (norm === 'FONT_7SEG_48') return '7SEG';
    if (norm.startsWith('SANS')) return 'SANS';
    if (norm.startsWith('SERIF')) return 'SERIF';
    if (norm.startsWith('MONO')) return 'MONO';
    return 'GLCD';
  }

  function fontSizeVariantOf(id, scale) {
    const fam = fontFamilyOf(id);
    if (fam === 'GLCD') return Math.max(1, Math.trunc(Number(scale) || 1));
    const norm = normalizeId(id);
    if (norm === 'FONT_16') return '16';
    if (norm === 'FONT_26') return '26';
    if (norm === 'FONT_7SEG_48') return '48';
    const m = norm.match(/_(\d+)$/);
    if (m) return m[1];
    return '9';
  }

  function isBold(id) {
    if (!id) return false;
    return String(id).includes('_BOLD_');
  }

  function fontIdFrom(family, sizeVariant, bold) {
    if (!family || family === 'GLCD') return 'GLCD';
    if (family === 'BITMAP') {
      const s = String(sizeVariant || '16');
      return s === '26' ? 'FONT_26' : 'FONT_16';
    }
    if (family === '7SEG') return 'FONT_7SEG_48';
    if (family === 'SANS' || family === 'SERIF' || family === 'MONO') {
      const b = bold ? 'BOLD_' : '';
      const pt = String(sizeVariant || '9');
      return `${family}_${b}${pt}`;
    }
    return 'GLCD';
  }

  function fontHeightPx(id, scale) {
    const fam = fontFamilyOf(id);
    if (fam === 'GLCD') {
      const s = Math.max(1, Math.trunc(Number(scale) || 1));
      return s * 8;
    }
    const font = get(id);
    return font ? (font.lineHeight || 8) : 8;
  }

  return Object.freeze({
    list: FONTS,
    get,
    normalizeId,
    fieldFont,
    isSupported,
    effectiveId,
    measureWidth,
    lineHeight,
    rasterize,
    availableFontsFor,
    fontFamilyOf,
    fontSizeVariantOf,
    isBold,
    fontIdFrom,
    fontHeightPx,
    cppSymbol: (id) => {
      const eff = normalizeId(effectiveId(id));
      if (!eff || eff === 'GLCD') return 'JWPLC_FONT_GLCD';
      if (eff === 'FONT2' || eff === 'FONT_16') return 'JWPLC_FONT_16';
      if (eff === 'FONT4' || eff === 'FONT_26') return 'JWPLC_FONT_26';
      if (eff === '7SEG_48' || eff === 'FONT_7SEG_48') return 'JWPLC_FONT_7SEG_48';
      return `JWPLC_FONT_${eff}`;
    }
  });
});

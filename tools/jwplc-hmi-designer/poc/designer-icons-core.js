// Núcleo puro de los iconos con estado (Alpha16): decodifica el catálogo
// (poc/designer-icon-catalog.js) y genera el C++ de los iconos usados y de cada
// campo ICON contra JWPLC_UI (JWPLC_UIIcon / JWPLC_UIIconField). Sin DOM: se
// carga en el navegador (window.JWPLCIconsCore) y en node (tests, CLI).
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCIconsCore = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  const MAX_SIZE = 64;
  const MAX_STATES = 8;

  function base64ToBytes(b64) {
    if (typeof Buffer !== 'undefined') return new Uint8Array(Buffer.from(b64, 'base64'));
    const bin = atob(b64);
    const out = new Uint8Array(bin.length);
    for (let i = 0; i < bin.length; i += 1) out[i] = bin.charCodeAt(i);
    return out;
  }

  function stride(width) {
    return (width + 1) >> 1;
  }

  // Igual que JWPLC_UI.cpp:drawIcon: estado fuera de rango = el último.
  function clampState(entry, state) {
    const count = entry?.states?.length || 0;
    const value = Math.max(0, Math.trunc(Number(state) || 0));
    return count ? Math.min(value, count - 1) : 0;
  }

  // Píxeles RGB565 del estado; -1 = transparente (toma el fondo del campo).
  function decode(entry, state) {
    const st = entry.states[clampState(entry, state)];
    const bytes = base64ToBytes(entry.images[st.image]);
    const w = entry.width;
    const h = entry.height;
    const s = stride(w);
    const pixels = new Int32Array(w * h);
    for (let y = 0; y < h; y += 1) {
      for (let x = 0; x < w; x += 1) {
        const b = bytes[y * s + (x >> 1)];
        const index = (x & 1) ? (b & 0x0F) : (b >> 4);
        pixels[y * w + x] = index === entry.transparent ? -1 : st.palette[index];
      }
    }
    return { width: w, height: h, pixels };
  }

  // ICON sin fondo (JWPLC_UI.cpp:iconAlwaysTransparent): 1 donde el píxel es
  // transparente en todos los estados; ahí no se pinta nada.
  function alwaysTransparentMask(entry) {
    const w = entry.width;
    const h = entry.height;
    const s = stride(w);
    const mask = new Uint8Array(w * h).fill(1);
    entry.states.forEach((st) => {
      const bytes = base64ToBytes(entry.images[st.image]);
      for (let i = 0; i < w * h; i += 1) {
        if (!mask[i]) continue;
        const x = i % w;
        const b = bytes[Math.floor(i / w) * s + (x >> 1)];
        if (((x & 1) ? (b & 0x0F) : (b >> 4)) !== entry.transparent) mask[i] = 0;
      }
    });
    return mask;
  }

  function findIcon(catalog, id) {
    return (catalog?.icons || []).find((icon) => icon.id === id) || null;
  }

  function sizesOf(icon) {
    return Object.keys(icon?.sizes || {}).map(Number).sort((a, b) => a - b);
  }

  // Tamaño disponible más cercano al pedido (el catálogo no tiene todos).
  function resolveSize(icon, size) {
    const sizes = sizesOf(icon);
    if (!sizes.length) return 0;
    const wanted = Number(size) || 32;
    return sizes.reduce((best, s) => (Math.abs(s - wanted) < Math.abs(best - wanted) ? s : best), sizes[0]);
  }

  function entryFor(catalog, id, size) {
    const icon = findIcon(catalog, id);
    if (!icon) return null;
    return icon.sizes[resolveSize(icon, size)] || null;
  }

  // Colores de paleta que hacen falta: hasta el índice más alto usado en las
  // imágenes (el firmware sólo lee palette[índice] de los píxeles).
  function paletteLength(entry) {
    let max = 0;
    entry.images.forEach((b64) => base64ToBytes(b64).forEach((b) => { max = Math.max(max, b >> 4, b & 0x0F); }));
    return max + 1;
  }

  // Bytes en flash de un icono: imágenes únicas + paleta por estado + descriptores.
  function flashBytes(entry) {
    if (!entry) return 0;
    return entry.images.length * stride(entry.width) * entry.height +
      entry.states.length * (2 * paletteLength(entry) + 8) + 12;
  }

  function symbolFor(id, size) {
    return `HMI_ICON_${String(id).replace(/[^A-Za-z0-9]+/g, '_').toUpperCase()}_${size}`;
  }

  function hex(value, digits) {
    return `0x${(Number(value) >>> 0).toString(16).toUpperCase().padStart(digits, '0')}`;
  }

  function wrap(items, perLine, indent) {
    const lines = [];
    for (let i = 0; i < items.length; i += perLine) lines.push(indent + items.slice(i, i + perLine).join(', '));
    return lines.join(',\n');
  }

  // C++ de un icono (un tamaño): píxeles por imagen, paletas por estado y descriptor.
  function assetBlock(asset) {
    const { id, name, stateNames, entry } = asset;
    const sym = symbolFor(id, entry.width);
    const n = entry.states.length;
    const out = [];
    out.push(`// ${name || id} (${id}) ${entry.width} px · ${n} estados: ${(stateNames || []).join(', ')} · ${flashBytes(entry)} B`);
    entry.images.forEach((b64, k) => {
      const bytes = Array.from(base64ToBytes(b64), (b) => hex(b, 2));
      out.push(`static const uint8_t ${sym}_PX${k}[${bytes.length}] =\n{\n${wrap(bytes, 16, '    ')}\n};`);
    });
    const colors = paletteLength(entry);
    const pals = entry.states.map((st) => `    {${st.palette.slice(0, colors).map((c) => hex(c, 4)).join(', ')}}`);
    out.push(`static const uint16_t ${sym}_PAL[${n}][${colors}] =\n{\n${pals.join(',\n')}\n};`);
    const states = entry.states.map((st, k) => `    {${sym}_PX${st.image}, ${sym}_PAL[${k}]}`);
    out.push(`static const JWPLC_UIIconState ${sym}_STATES[${n}] =\n{\n${states.join(',\n')}\n};`);
    out.push(`static const JWPLC_UIIcon ${sym}(${entry.width}, ${entry.height}, ${n}, ${sym}_STATES, ${entry.transparent});`);
    return out.join('\n');
  }

  // Sección de iconos del header: uno por (icono, tamaño) usado, en orden de aparición.
  function assetsSection(assets) {
    if (!assets.length) return '';
    const total = assets.reduce((sum, a) => sum + flashBytes(a.entry), 0);
    return `// Iconos con estado (biblioteca del Designer): sólo los que usa este HMI, ${total} B en flash.\n` +
      `#ifndef JWPLC_UI_HAS_ICON\n` +
      `#error "Este HMI usa iconos: actualiza el package JWPLC (JWPLC_Display con JWPLC_UI_HAS_ICON)."\n` +
      `#endif\n\n` +
      assets.map(assetBlock).join('\n\n') + '\n';
  }

  function hex565(value) {
    const num = Number(value);
    return hex(Number.isFinite(num) ? num & 0xFFFF : 0, 4);
  }

  // field: ICON del Designer; symbol: el JWPLC_UIIcon de su icono y tamaño.
  function iconFieldBlock(field, id, symbol, cppString) {
    const label = field.label ? `"${cppString(field.label)}"` : 'nullptr';
    const layout = field.layout === 'INLINE' ? 'INLINE' : 'STACKED';
    return `    JWPLC_UIIconField(\n        ${id},\n        JWPLC_UIRect(${field.x}, ${field.y}),\n        &${symbol},\n        JWPLC_UIText(${label}),\n        JWPLC_UIIconStyle(\n            ${Math.max(1, Number(field.labelSize) || 1)},\n            ${field.frame ? 'true' : 'false'},\n            JWPLC_UI_LAYOUT_${layout}),\n        ${field.page || 0},\n        JWPLC_UIColors(\n            ${hex565(field.labelColor)},\n            ${hex565(field.valueColor)},\n            ${hex565(field.backgroundColor)},\n            ${hex565(field.frameColor)})${field.transparentBackground ? ',\n        true' : ''})`;
  }

  // bool para 2 estados; uint8_t con más (0..N-1).
  function iconCppType(field) {
    return field.iconVarType === 'uint8' ? 'uint8_t' : 'bool';
  }

  // El comentario nombra los estados (la guía del sketch lo copia en su ejemplo).
  function iconDeclaration(field, variable, stateNames = []) {
    const names = (stateNames || []).map((n) => String(n).replace(/[\r\n]+/g, ' '));
    if (iconCppType(field) === 'bool') {
      return names.length >= 2 ? `bool ${variable} = false; // ICON: false=${names[0]} true=${names[1]}` : `bool ${variable} = false;`;
    }
    const legend = names.length ? names.map((n, i) => `${i}=${n}`).join(' ') : 'estado 0..N-1';
    return `uint8_t ${variable} = 0; // ICON: ${legend}`;
  }

  function stateNamesFor(catalog, id) {
    return findIcon(catalog, id)?.stateNames || [];
  }

  function iconSetter(id, variable) {
    return `JWPLC_Display.setIconState(${id}, ${variable});`;
  }

  return Object.freeze({
    MAX_SIZE,
    MAX_STATES,
    base64ToBytes,
    decode,
    alwaysTransparentMask,
    clampState,
    findIcon,
    sizesOf,
    resolveSize,
    entryFor,
    flashBytes,
    paletteLength,
    symbolFor,
    assetBlock,
    assetsSection,
    iconFieldBlock,
    iconCppType,
    iconDeclaration,
    stateNamesFor,
    iconSetter
  });
});

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
  const MAX_STATES = 16;

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
  function clampState(entry, state, maxCount = 0) {
    const count = maxCount > 0 ? maxCount : (entry?.states?.length || 0);
    const value = Math.max(0, Math.trunc(Number(state) || 0));
    return count ? Math.min(value, count - 1) : 0;
  }

  function to565(val) {
    if (val === null || val === undefined) return null;
    if (typeof val === 'number') return Number.isFinite(val) ? (val & 0xFFFF) : 0;
    const s = String(val).trim();
    if (!s) return null;
    if (s.startsWith('#')) {
      const hex = s.replace('#', '');
      const num = parseInt(hex.length === 3 ? hex.split('').map((c) => c + c).join('') : hex, 16);
      if (Number.isNaN(num)) return null;
      const r = (num >> 16) & 0xFF;
      const g = (num >> 8) & 0xFF;
      const b = num & 0xFF;
      return (((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)) & 0xFFFF;
    }
    if (s.startsWith('0x') || s.startsWith('0X')) {
      const n = parseInt(s, 16);
      return Number.isNaN(n) ? null : (n & 0xFFFF);
    }
    const n = Number(s);
    return Number.isNaN(n) ? null : (n & 0xFFFF);
  }

  function dim565(color, factor = 0.25) {
    const r = Math.round(((color >> 11) & 0x1F) * factor);
    const g = Math.round(((color >> 5) & 0x3F) * factor);
    const b = Math.round((color & 0x1F) * factor);
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
  }

  function bright565(color, mix = 0.45) {
    const r = Math.min(31, Math.round(((color >> 11) & 0x1F) * (1 - mix) + 31 * mix));
    const g = Math.min(63, Math.round(((color >> 5) & 0x3F) * (1 - mix) + 63 * mix));
    const b = Math.min(31, Math.round((color & 0x1F) * (1 - mix) + 31 * mix));
    return ((r & 0x1F) << 11) | ((g & 0x3F) << 5) | (b & 0x1F);
  }

  function resolvePalette(entry, state, frameObj, override) {
    const base = (frameObj?.palette || []).slice();
    while (base.length < 16) base.push(0);
    if (!override) return base;
    if (Array.isArray(override)) {
      for (let i = 0; i < Math.min(override.length, 16); i += 1) {
        if (override[i] !== null && override[i] !== undefined) {
          const c = to565(override[i]);
          if (c !== null) base[i] = c;
        }
      }
      return base;
    }

    const roles = Array.isArray(entry?.roles) ? entry.roles : [];

    // 1. Color del equipo / cuerpo (override.bodyColor)
    const bodyColor = to565(override.bodyColor);
    if (bodyColor !== null && roles.length) {
      let bodyRole = null;
      if (roles.includes('B') && !roles.includes('G')) bodyRole = 'B';
      else if (roles.includes('S')) bodyRole = 'S';
      else if (roles.includes('C')) bodyRole = 'C';
      if (bodyRole) {
        const idx = roles.indexOf(bodyRole) + 1;
        if (idx < 16) base[idx] = bodyColor;
      }
    }

    // 2. Diccionario de roles personalizados (override.customColors)
    if (override.customColors && typeof override.customColors === 'object') {
      for (const [key, val] of Object.entries(override.customColors)) {
        const c = to565(val);
        if (c === null) continue;
        if (Number.isInteger(Number(key))) {
          const idx = Number(key);
          if (idx >= 0 && idx < 16) base[idx] = c;
        } else if (roles.includes(key)) {
          const idx = roles.indexOf(key) + 1;
          if (idx < 16) base[idx] = c;
        }
      }
    }

    // 3. Piloto: colores configurados por estado (override.stateColors)
    if (Array.isArray(override.stateColors) && override.stateColors[state] !== undefined) {
      const sc = to565(override.stateColors[state]);
      if (sc !== null) {
        if (roles.includes('L')) { // pilotos/piloto
          const lIdx = roles.indexOf('L') + 1;
          base[lIdx] = sc;
          if (roles.includes('H')) {
            const hIdx = roles.indexOf('H') + 1;
            base[hIdx] = bright565(sc, 0.4);
          }
        } else if (roles.includes('S') && roles.includes('O')) { // pilotos/piloto_cuadrado
          const sIdx = roles.indexOf('S') + 1;
          base[sIdx] = sc;
          if (roles.includes('H')) {
            const hIdx = roles.indexOf('H') + 1;
            base[hIdx] = bright565(sc, 0.4);
          }
        }
      }
    }

    // 4. Baliza: colores configurados por piso y cantidad de pisos
    if (roles.includes('G') && roles.includes('Y') && roles.includes('R') && roles.includes('B')) {
      const tierRoles = ['G', 'Y', 'R', 'B']; // Piso 1 (inferior: Verde), 2 (Ámbar), 3 (Rojo), 4 (Azul)
      const tierCount = Math.max(1, Math.min(4, Number(override.tierCount || override.activeTiers || 4)));
      const customTierColors = Array.isArray(override.tierColors) ? override.tierColors : null;
      for (let t = 0; t < 4; t += 1) {
        const role = tierRoles[t];
        const rIdx = roles.indexOf(role) + 1;
        const bit = (state >> t) & 1;
        if (t >= tierCount) {
          base[rIdx] = 0x18A3; // Piso no configurado/inactivo: gris oscuro uniforme
          continue;
        }
        if (customTierColors && customTierColors[t] !== undefined) {
          const onColor = to565(customTierColors[t]);
          if (onColor !== null) {
            base[rIdx] = bit ? onColor : dim565(onColor, 0.22);
          }
        }
      }
    }

    return base;
  }

  // Píxeles RGB565 del estado; -1 = transparente (toma el fondo del campo).
  // frame: índice de fotograma para estados animados (por defecto 0).
  function decode(entry, state, frame = 0, paletteOverride = null) {
    const overrideStateCount = Array.isArray(paletteOverride?.stateColors) ? paletteOverride.stateColors.length
      : Number(paletteOverride?.stateCount) || 0;
    const maxStates = Math.max(entry?.states?.length || 0, overrideStateCount);
    const clamped = clampState(entry, state, maxStates);
    const stBase = entry.states[Math.min(clamped, entry.states.length - 1)];
    const frameObj = (stBase && Array.isArray(stBase.frames) && stBase.frames.length > 0)
      ? stBase.frames[Math.max(0, Math.min(Math.trunc(Number(frame) || 0), stBase.frames.length - 1))]
      : stBase;
    let imageIdx = frameObj.image;
    if (entry.tierImages && entry.tierImages.length > 0) {
      const tierCount = Math.max(1, Math.min(entry.tierImages.length, Number(paletteOverride?.tierCount || paletteOverride?.activeTiers || 4)));
      imageIdx = entry.tierImages[tierCount - 1];
    }
    const bytes = base64ToBytes(entry.images[imageIdx]);
    const w = entry.width;
    const h = entry.height;
    const s = stride(w);
    const pixels = new Int32Array(w * h);
    const pal = paletteOverride ? resolvePalette(entry, clamped, frameObj, paletteOverride) : frameObj.palette;
    for (let y = 0; y < h; y += 1) {
      for (let x = 0; x < w; x += 1) {
        const b = bytes[y * s + (x >> 1)];
        const index = (x & 1) ? (b & 0x0F) : (b >> 4);
        pixels[y * w + x] = index === entry.transparent ? -1 : pal[index];
      }
    }
    return { width: w, height: h, pixels };
  }

  // ICON sin fondo (JWPLC_UI.cpp:iconAlwaysTransparent): 1 donde el píxel es
  // transparente en todos los estados y cuadros; ahí no se pinta nada.
  function alwaysTransparentMask(entry) {
    const w = entry.width;
    const h = entry.height;
    const s = stride(w);
    const mask = new Uint8Array(w * h).fill(1);
    entry.states.forEach((st) => {
      const allFrames = (st && Array.isArray(st.frames) && st.frames.length > 0) ? st.frames : [st];
      allFrames.forEach((frameObj) => {
        const bytes = base64ToBytes(entry.images[frameObj.image]);
        for (let i = 0; i < w * h; i += 1) {
          if (!mask[i]) continue;
          const x = i % w;
          const b = bytes[Math.floor(i / w) * s + (x >> 1)];
          if (((x & 1) ? (b & 0x0F) : (b >> 4)) !== entry.transparent) mask[i] = 0;
        }
      });
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

  // Bytes en flash de un icono: imágenes únicas + paleta por estado + descriptores y sub-frames.
  function flashBytes(entry) {
    if (!entry) return 0;
    let extraFrames = 0;
    entry.states.forEach((st) => {
      if (st.frames && st.frames.length > 1) extraFrames += st.frames.length * 8;
    });
    return entry.images.length * stride(entry.width) * entry.height +
      entry.states.length * (2 * paletteLength(entry) + 12) + extraFrames + 12;
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
    const { id, name, stateNames, entry, override } = asset;
    const sym = asset.symbol || symbolFor(id, entry.width);
    const overrideStateCount = Array.isArray(override?.stateColors) ? override.stateColors.length
      : Number(override?.stateCount) || 0;
    const stateCount = Math.max(entry.states.length, overrideStateCount);
    const n = Math.min(stateCount, MAX_STATES);
    const out = [];
    const names = (Array.isArray(stateNames) && stateNames.length >= n)
      ? stateNames.slice(0, n)
      : Array.from({ length: n }, (_, i) => stateNames?.[i] || `Estado ${i}`);
    out.push(`// ${name || id} (${id}) ${entry.width} px · ${n} estados: ${names.join(', ')} · ${flashBytes(entry)} B`);
    entry.images.forEach((b64, k) => {
      const bytes = Array.from(base64ToBytes(b64), (b) => hex(b, 2));
      out.push(`static const uint8_t ${sym}_PX${k}[${bytes.length}] =\n{\n${wrap(bytes, 16, '    ')}\n};`);
    });
    const colors = paletteLength(entry);
    const pals = [];
    for (let sIdx = 0; sIdx < n; sIdx += 1) {
      const stBase = entry.states[Math.min(sIdx, entry.states.length - 1)];
      const pal = override ? resolvePalette(entry, sIdx, stBase, override) : stBase.palette;
      pals.push(`    {${pal.slice(0, colors).map((c) => hex(c, 4)).join(', ')}}`);
    }
    out.push(`static const uint16_t ${sym}_PAL[${n}][${colors}] =\n{\n${pals.join(',\n')}\n};`);

    // Tablas de fotogramas para estados animados (si hay)
    entry.states.forEach((st, k) => {
      if (k < n && st.frames && st.frames.length > 1) {
        const frameLines = st.frames.map((f) => `    {${sym}_PX${f.image}, ${sym}_PAL[${k}]}`);
        out.push(`static const JWPLC_UIIconFrame ${sym}_ST${k}_FRAMES[${st.frames.length}] =\n{\n${frameLines.join(',\n')}\n};`);
      }
    });

    const states = [];
    for (let k = 0; k < n; k += 1) {
      const st = entry.states[Math.min(k, entry.states.length - 1)];
      let imgIdx = st.image;
      if (entry.tierImages && entry.tierImages.length > 0) {
        const tierCount = Math.max(1, Math.min(entry.tierImages.length, Number(override?.tierCount || override?.activeTiers || 4)));
        imgIdx = entry.tierImages[tierCount - 1];
      }
      if (st.frames && st.frames.length > 1) {
        const interval = Math.max(20, Math.min(2000, Number(override?.animIntervalMs) || st.intervalMs || 120));
        states.push(`    {${sym}_PX${imgIdx}, ${sym}_PAL[${k}], ${st.frames.length}, ${interval}, ${sym}_ST${k}_FRAMES}`);
      } else {
        states.push(`    {${sym}_PX${imgIdx}, ${sym}_PAL[${k}]}`);
      }
    }
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
    if (field?.iconId === 'pilotos/baliza' && field.balizaMode === 'independent') {
      const tierCount = Math.max(1, Math.min(4, Number(field.tierCount || 4)));
      const tierLabels = ['Piso 1 (Verde)', 'Piso 2 (Ámbar)', 'Piso 3 (Rojo)', 'Piso 4 (Azul)'];
      const tierVars = field.tierVariables || [];
      const lines = [];
      for (let t = 0; t < tierCount; t += 1) {
        const v = tierVars[t] || `${variable}_p${t + 1}`;
        lines.push(`bool ${v} = false; // BALIZA: ${tierLabels[t]}`);
      }
      return lines.join('\n');
    }
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

  function iconSetter(id, variable, field = null) {
    if (field?.iconId === 'pilotos/baliza' && field.balizaMode === 'independent') {
      const tierCount = Math.max(1, Math.min(4, Number(field.tierCount || 4)));
      const tierVars = field.tierVariables || [];
      const parts = [];
      for (let t = 0; t < tierCount; t += 1) {
        const v = tierVars[t] || `${variable}_p${t + 1}`;
        parts.push(`(${v} ? (1 << ${t}) : 0)`);
      }
      return `JWPLC_Display.setIconState(${id}, (uint8_t)(${parts.join(' | ')}));`;
    }
    return `JWPLC_Display.setIconState(${id}, ${variable});`;
  }

  return Object.freeze({
    MAX_SIZE,
    MAX_STATES,
    base64ToBytes,
    to565,
    dim565,
    bright565,
    resolvePalette,
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

// Núcleo puro de códigos QR para JWPLC HMI Designer.
// Replica exactamente el codificador ISO/IEC 18004 (modo byte, v1..v10, L/M/Q/H)
// de JWPLC_UIQR (JWPLC_UI_Widgets.cpp) sin dependencias externas ni DOM.
// Funciona en Node.js (CLI/tests) y en navegador (window.JWPLCQRCore).
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCQRCore = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  const SCREEN_W = 320;
  const SCREEN_H = 170;
  const MAX_VERSION = 10;
  const MAX_SIDE = MAX_VERSION * 4 + 17; // 57
  const MAX_BYTES = 271; // Capacidad máxima v10 nivel L

  const ECC_LEVELS = Object.freeze({
    L: { id: 0, label: 'L (7%)', pct: '7%', cpp: 'QR_LEVEL_L' },
    M: { id: 1, label: 'M (15%)', pct: '15%', cpp: 'QR_LEVEL_M' },
    Q: { id: 2, label: 'Q (25%)', pct: '25%', cpp: 'QR_LEVEL_Q' },
    H: { id: 3, label: 'H (30%)', pct: '30%', cpp: 'QR_LEVEL_H' }
  });

  const PRESETS = Object.freeze({
    URL: {
      id: 'URL',
      label: 'Web / IP',
      defaultText: 'http://192.168.1.50',
      hint: 'Enlace web o IP local del equipo'
    },
    PAIR: {
      id: 'PAIR',
      label: 'Vinculación',
      defaultText: 'http://192.168.1.50/v?c=123456',
      hint: 'Enlace de vinculación estilo Runtime'
    },
    DOCS: {
      id: 'DOCS',
      label: 'Manual / Docs',
      defaultText: 'https://docs.jwcontrol.com',
      hint: 'Manual técnico o documentación de máquina'
    },
    WIFI: {
      id: 'WIFI',
      label: 'Wi-Fi',
      defaultText: 'WIFI:T:WPA;S:JWPLC_AP;P:12345678;;',
      hint: 'Credenciales de acceso a red Wi-Fi'
    },
    SUPPORT: {
      id: 'SUPPORT',
      label: 'Soporte',
      defaultText: 'https://wa.me/51999999999',
      hint: 'Contacto directo de soporte técnico'
    },
    CUSTOM: {
      id: 'CUSTOM',
      label: 'Libre',
      defaultText: 'JWPLC-BASIC-01',
      hint: 'Texto, serie o URL personalizada'
    }
  });

  // Tablas idénticas a JWPLC_UI_Widgets.cpp (L=0, M=1, Q=2, H=3; ver 1..10)
  const ECC_PER_BLOCK = [
    [-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18],   // L
    [-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26],  // M
    [-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24],  // Q
    [-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28]   // H
  ];

  const NUM_BLOCKS = [
    [-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4],
    [-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5],
    [-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8],
    [-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8]
  ];

  const FORMAT_BITS = [1, 0, 3, 2]; // L=01, M=00, Q=11, H=10

  const encodeCache = new Map();
  const MAX_CACHE_ENTRIES = 64;

  function toUtf8Bytes(text) {
    const str = String(text ?? '');
    if (typeof TextEncoder !== 'undefined') {
      return new TextEncoder().encode(str);
    }
    if (typeof Buffer !== 'undefined') {
      return new Uint8Array(Buffer.from(str, 'utf8'));
    }
    const out = [];
    for (let i = 0; i < str.length; i += 1) {
      let c = str.charCodeAt(i);
      if (c < 0x80) {
        out.push(c);
      } else if (c < 0x800) {
        out.push(0xC0 | (c >> 6), 0x80 | (c & 0x3F));
      } else if (c >= 0xD800 && c <= 0xDBFF && i + 1 < str.length) {
        const c2 = str.charCodeAt(i + 1);
        if (c2 >= 0xDC00 && c2 <= 0xDFFF) {
          i += 1;
          const cp = 0x10000 + (((c & 0x3FF) << 10) | (c2 & 0x3FF));
          out.push(0xF0 | (cp >> 18), 0x80 | ((cp >> 12) & 0x3F), 0x80 | ((cp >> 6) & 0x3F), 0x80 | (cp & 0x3F));
        }
      } else {
        out.push(0xE0 | (c >> 12), 0x80 | ((c >> 6) & 0x3F), 0x80 | (c & 0x3F));
      }
    }
    return new Uint8Array(out);
  }

  function normalizeEcc(ecc) {
    const k = String(ecc || 'M').trim().toUpperCase();
    return ECC_LEVELS[k] ? k : 'M';
  }

  function qrRawModules(ver) {
    let r = (16 * ver + 128) * ver + 64;
    if (ver >= 2) {
      const n = Math.floor(ver / 7) + 2;
      r -= (25 * n - 10) * n - 55;
      if (ver >= 7) r -= 36;
    }
    return r;
  }

  function qrDataCodewords(ver, ecl) {
    return Math.floor(qrRawModules(ver) / 8) - ECC_PER_BLOCK[ecl][ver] * NUM_BLOCKS[ecl][ver];
  }

  function maxCapacityBytes(ecc) {
    const ecl = ECC_LEVELS[normalizeEcc(ecc)].id;
    const capBits = qrDataCodewords(MAX_VERSION, ecl) * 8;
    // modo (4 bits) + longitud v10 (16 bits) = 20 bits de cabecera
    return Math.floor((capBits - 20) / 8);
  }

  function gfMul(x, y) {
    let z = 0;
    for (let i = 7; i >= 0; i -= 1) {
      z = (z << 1) ^ ((z >> 7) * 0x11D);
      z ^= ((y >> i) & 1) * x;
    }
    return z & 0xFF;
  }

  function rsDivisor(degree) {
    const out = new Uint8Array(degree);
    out[degree - 1] = 1;
    let root = 1;
    for (let i = 0; i < degree; i += 1) {
      for (let j = 0; j < degree; j += 1) {
        out[j] = gfMul(out[j], root);
        if (j + 1 < degree) out[j] ^= out[j + 1];
      }
      root = gfMul(root, 0x02);
    }
    return out;
  }

  function rsRemainder(data, offset, len, div, degree) {
    const out = new Uint8Array(degree);
    for (let i = 0; i < len; i += 1) {
      const f = data[offset + i] ^ out[0];
      for (let k = 0; k < degree - 1; k += 1) out[k] = out[k + 1];
      out[degree - 1] = 0;
      for (let j = 0; j < degree; j += 1) {
        out[j] ^= gfMul(div[j], f);
      }
    }
    return out;
  }

  function encode(text, ecc = 'M', forcedMask = -1) {
    const eccKey = normalizeEcc(ecc);
    const ecl = ECC_LEVELS[eccKey].id;
    const rawStr = String(text ?? '');
    const cacheKey = `${eccKey}|${forcedMask}|${rawStr}`;
    if (encodeCache.has(cacheKey)) {
      return encodeCache.get(cacheKey);
    }

    const bytes = toUtf8Bytes(rawStr);
    const len = bytes.length;
    const maxCap = maxCapacityBytes(eccKey);

    if (len === 0) {
      return {
        ok: false,
        empty: true,
        error: 'El contenido del QR está vacío.',
        version: 1,
        size: 21,
        ecc: eccKey,
        byteLength: 0,
        maxCapacity: maxCap,
        modules: new Uint8Array(21 * 21)
      };
    }

    let version = 0;
    for (let v = 1; v <= MAX_VERSION; v += 1) {
      const cap = qrDataCodewords(v, ecl) * 8;
      const need = 4 + (v < 10 ? 8 : 16) + len * 8;
      if (need <= cap) {
        version = v;
        break;
      }
    }

    if (version === 0) {
      return {
        ok: false,
        overflow: true,
        error: `Excede el máximo de versión 10 (${len}/${maxCap} bytes en nivel ${eccKey}).`,
        version: MAX_VERSION,
        size: MAX_SIDE,
        ecc: eccKey,
        byteLength: len,
        maxCapacity: maxCap,
        modules: new Uint8Array(MAX_SIDE * MAX_SIDE)
      };
    }

    const size = version * 4 + 17;
    const dataLen = qrDataCodewords(version, ecl);
    const buf = new Uint8Array(dataLen);
    let bit = 0;
    const put = (val, n) => {
      for (let i = n - 1; i >= 0; i -= 1, bit += 1) {
        if ((val >> i) & 1) {
          buf[bit >> 3] |= (0x80 >> (bit & 7));
        }
      }
    };
    put(4, 4);
    put(len, version < 10 ? 8 : 16);
    for (let i = 0; i < len; i += 1) put(bytes[i], 8);
    const capBits = dataLen * 8;
    put(0, Math.min(4, capBits - bit));
    put(0, (8 - (bit % 8)) % 8);
    for (let pad = 0xEC; bit < capBits; pad ^= (0xEC ^ 0x11)) {
      put(pad, 8);
    }

    const numBlocks = NUM_BLOCKS[ecl][version];
    const eccLen = ECC_PER_BLOCK[ecl][version];
    const raw = Math.floor(qrRawModules(version) / 8);
    const numShort = numBlocks - (raw % numBlocks);
    const shortLen = Math.floor(raw / numBlocks);
    const div = rsDivisor(eccLen);
    const out = new Uint8Array(raw);
    const eccBlocks = [];
    const offs = new Int32Array(numBlocks);

    for (let b = 0, k = 0; b < numBlocks; b += 1) {
      const dl = shortLen - eccLen + (b < numShort ? 0 : 1);
      offs[b] = k;
      eccBlocks.push(rsRemainder(buf, k, dl, div, eccLen));
      k += dl;
    }

    let o = 0;
    for (let i = 0; i <= shortLen - eccLen; i += 1) {
      for (let b = 0; b < numBlocks; b += 1) {
        const dl = shortLen - eccLen + (b < numShort ? 0 : 1);
        if (i < dl) out[o++] = buf[offs[b] + i];
      }
    }
    for (let i = 0; i < eccLen; i += 1) {
      for (let b = 0; b < numBlocks; b += 1) {
        out[o++] = eccBlocks[b][i];
      }
    }

    const modules = new Uint8Array(size * size);
    const func = new Uint8Array(size * size);
    const setFunction = (x, y, dark) => {
      const idx = y * size + x;
      modules[idx] = dark ? 1 : 0;
      func[idx] = 1;
    };

    const drawFinder = (cx, cy) => {
      for (let dy = -4; dy <= 4; dy += 1) {
        for (let dx = -4; dx <= 4; dx += 1) {
          const x = cx + dx;
          const y = cy + dy;
          if (x < 0 || y < 0 || x >= size || y >= size) continue;
          const d = Math.max(Math.abs(dx), Math.abs(dy));
          setFunction(x, y, d !== 2 && d !== 4);
        }
      }
    };

    const drawAlignment = (cx, cy) => {
      for (let dy = -2; dy <= 2; dy += 1) {
        for (let dx = -2; dx <= 2; dx += 1) {
          const d = Math.max(Math.abs(dx), Math.abs(dy));
          setFunction(cx + dx, cy + dy, d !== 1);
        }
      }
    };

    const drawFormat = (mask) => {
      const data = (FORMAT_BITS[ecl] << 3) | mask;
      let rem = data;
      for (let i = 0; i < 10; i += 1) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
      const bits = ((data << 10) | rem) ^ 0x5412;

      for (let i = 0; i <= 5; i += 1) setFunction(8, i, (bits >> i) & 1);
      setFunction(8, 7, (bits >> 6) & 1);
      setFunction(8, 8, (bits >> 7) & 1);
      setFunction(7, 8, (bits >> 8) & 1);
      for (let i = 9; i < 15; i += 1) setFunction(14 - i, 8, (bits >> i) & 1);
      for (let i = 0; i < 8; i += 1) setFunction(size - 1 - i, 8, (bits >> i) & 1);
      for (let i = 8; i < 15; i += 1) setFunction(8, size - 15 + i, (bits >> i) & 1);
      setFunction(8, size - 8, 1);
    };

    for (let i = 0; i < size; i += 1) {
      setFunction(6, i, i % 2 === 0);
      setFunction(i, 6, i % 2 === 0);
    }
    drawFinder(3, 3);
    drawFinder(size - 4, 3);
    drawFinder(3, size - 4);

    if (version >= 2) {
      const n = Math.floor(version / 7) + 2;
      const step = Math.floor((version * 8 + n * 3 + 5) / (n * 4 - 4)) * 2;
      const pos = new Int32Array(n);
      pos[0] = 6;
      for (let i = n - 1, p = size - 7; i >= 1; i -= 1, p -= step) pos[i] = p;
      for (let i = 0; i < n; i += 1) {
        for (let j = 0; j < n; j += 1) {
          if ((i === 0 && j === 0) || (i === 0 && j === n - 1) || (i === n - 1 && j === 0)) continue;
          drawAlignment(pos[i], pos[j]);
        }
      }
    }

    drawFormat(0);

    if (version >= 7) {
      let rem = version;
      for (let i = 0; i < 12; i += 1) rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
      const bits = (version << 12) | rem;
      for (let i = 0; i < 18; i += 1) {
        const b = (bits >> i) & 1;
        const a = size - 11 + (i % 3);
        const c = Math.floor(i / 3);
        setFunction(a, c, b);
        setFunction(c, a, b);
      }
    }

    let bitIdx = 0;
    for (let right = size - 1; right >= 1; right -= 2) {
      if (right === 6) right = 5;
      for (let vert = 0; vert < size; vert += 1) {
        for (let j = 0; j < 2; j += 1) {
          const x = right - j;
          const upward = ((right + 1) & 2) === 0;
          const y = upward ? size - 1 - vert : vert;
          const cell = y * size + x;
          if (!func[cell] && bitIdx < o * 8) {
            modules[cell] = (out[bitIdx >> 3] >> (7 - (bitIdx & 7))) & 1;
            bitIdx += 1;
          }
        }
      }
    }

    const applyMask = (mask) => {
      for (let y = 0; y < size; y += 1) {
        for (let x = 0; x < size; x += 1) {
          const cell = y * size + x;
          if (func[cell]) continue;
          let inv = false;
          switch (mask) {
            case 0: inv = ((x + y) % 2) === 0; break;
            case 1: inv = (y % 2) === 0; break;
            case 2: inv = (x % 3) === 0; break;
            case 3: inv = ((x + y) % 3) === 0; break;
            case 4: inv = ((Math.floor(x / 3) + Math.floor(y / 2)) % 2) === 0; break;
            case 5: inv = ((x * y) % 2) + ((x * y) % 3) === 0; break;
            case 6: inv = (((x * y) % 2) + ((x * y) % 3)) % 2 === 0; break;
            default: inv = (((x + y) % 2) + ((x * y) % 3)) % 2 === 0; break;
          }
          if (inv) modules[cell] ^= 1;
        }
      }
    };

    const penalty = () => {
      let result = 0;
      for (let pass = 0; pass < 2; pass += 1) {
        for (let a = 0; a < size; a += 1) {
          let run = 0;
          let prev = 0;
          for (let b = 0; b < size; b += 1) {
            const v = pass === 0 ? modules[a * size + b] : modules[b * size + a];
            if (b > 0 && v === prev) {
              run += 1;
            } else {
              if (run >= 5) result += 3 + (run - 5);
              run = 1;
              prev = v;
            }
          }
          if (run >= 5) result += 3 + (run - 5);
          for (let b = -4; b < size; b += 1) {
            let p0 = 0, p1 = 0, p2 = 0, p3 = 0, p4 = 0, p5 = 0, p6 = 0, p7 = 0, p8 = 0, p9 = 0, p10 = 0;
            const at = (k) => {
              const c = b + k;
              if (c < 0 || c >= size) return 0;
              return pass === 0 ? modules[a * size + c] : modules[c * size + a];
            };
            p0 = at(0); p1 = at(1); p2 = at(2); p3 = at(3); p4 = at(4);
            p5 = at(5); p6 = at(6); p7 = at(7); p8 = at(8); p9 = at(9); p10 = at(10);
            const core1 = p4 && !p5 && p6 && p7 && p8 && !p9 && p10;
            const core2 = p0 && !p1 && p2 && p3 && p4 && !p5 && p6;
            if (core1 && !p0 && !p1 && !p2 && !p3) result += 40;
            if (core2 && !p7 && !p8 && !p9 && !p10) result += 40;
          }
        }
      }
      let dark = 0;
      for (let y = 0; y < size; y += 1) {
        for (let x = 0; x < size; x += 1) {
          const v = modules[y * size + x];
          dark += v;
          if (x + 1 < size && y + 1 < size &&
              v === modules[y * size + x + 1] &&
              v === modules[(y + 1) * size + x] &&
              v === modules[(y + 1) * size + x + 1]) {
            result += 3;
          }
        }
      }
      const total = size * size;
      const k = Math.floor((Math.abs(dark * 20 - total * 10) + total - 1) / total) - 1;
      result += (k > 0 ? k : 0) * 10;
      return result;
    };

    let best = forcedMask;
    if (best < 0 || best > 7) {
      let bestPenalty = 0x7FFFFFFF;
      for (let m = 0; m < 8; m += 1) {
        applyMask(m);
        drawFormat(m);
        const p = penalty();
        if (p < bestPenalty) {
          bestPenalty = p;
          best = m;
        }
        applyMask(m);
      }
    }
    applyMask(best);
    drawFormat(best);

    const encoded = {
      ok: true,
      version,
      size,
      ecc: eccKey,
      mask: best,
      byteLength: len,
      maxCapacity: maxCap,
      modules
    };

    if (encodeCache.size >= MAX_CACHE_ENTRIES) {
      const firstKey = encodeCache.keys().next().value;
      if (firstKey !== undefined) encodeCache.delete(firstKey);
    }
    encodeCache.set(cacheKey, encoded);
    return encoded;
  }

  function clampScale(scale) {
    const n = Math.trunc(Number(scale) || 3);
    return Math.max(1, Math.min(4, n));
  }

  function clampQuietZone(quietZone) {
    if (quietZone === undefined || quietZone === null) return 4;
    const n = Math.trunc(Number(quietZone));
    return Number.isFinite(n) ? Math.max(0, Math.min(4, n)) : 4;
  }

  function clampMaxLength(maxLength) {
    const n = Math.trunc(Number(maxLength) || 80);
    return Math.max(16, Math.min(213, n));
  }

  function computeMetrics(field) {
    const ecc = normalizeEcc(field?.ecc);
    const scale = clampScale(field?.scale);
    const quietZone = clampQuietZone(field?.quietZone);
    const frame = Boolean(field?.frame);
    const sourceMode = field?.sourceMode === 'VARIABLE' ? 'VARIABLE' : 'STATIC';
    const text = String(field?.text ?? '');
    const encoded = encode(text, ecc);
    const moduleCount = encoded.size || 21;
    const totalModules = moduleCount + 2 * quietZone;
    const qrSide = totalModules * scale;
    const framePad = (frame && quietZone === 0) ? 1 : 0;
    const width = qrSide + framePad * 2;
    const height = qrSide + framePad * 2;
    const x = Math.trunc(Number(field?.x) || 0);
    const y = Math.trunc(Number(field?.y) || 0);
    const fitsScreen = width <= SCREEN_W && height <= SCREEN_H;
    const inBounds = x >= 0 && y >= 0 && (x + width) <= SCREEN_W && (y + height) <= SCREEN_H;

    return {
      encoded,
      ok: encoded.ok,
      version: encoded.version,
      moduleCount,
      quietZone,
      scale,
      totalModules,
      qrSide,
      frame,
      framePad,
      sourceMode,
      width,
      height,
      fitsScreen,
      inBounds,
      byteLength: encoded.byteLength,
      maxCapacity: encoded.maxCapacity,
      packedBytes: Math.ceil((moduleCount * moduleCount) / 8)
    };
  }

  // Empaqueta módulos row-major (MSB primero en cada byte) para static const uint8_t[] en Flash.
  function packModulesToBytes(encoded) {
    const size = encoded?.size || 0;
    const modules = encoded?.modules;
    if (!size || !modules) return [];
    const totalBits = size * size;
    const out = new Array(Math.ceil(totalBits / 8)).fill(0);
    for (let i = 0; i < totalBits; i += 1) {
      if (modules[i]) {
        out[i >> 3] |= (0x80 >> (i & 7));
      }
    }
    return out;
  }

  function rgb565Luminance(color565) {
    const c = Number(color565) & 0xFFFF;
    const r = ((c >> 11) & 0x1F) * 255 / 31;
    const g = ((c >> 5) & 0x3F) * 255 / 63;
    const b = (c & 0x1F) * 255 / 31;
    return 0.2126 * r + 0.7152 * g + 0.0722 * b;
  }

  function contrastCheck(fgColor, bgColor) {
    const lumFg = rgb565Luminance(fgColor);
    const lumBg = rgb565Luminance(bgColor);
    const diff = Math.abs(lumBg - lumFg);
    const inverted = lumFg > lumBg;
    const lowContrast = diff < 95;
    return {
      ok: !inverted && !lowContrast,
      inverted,
      lowContrast,
      diff: Math.round(diff)
    };
  }

  function escapeWifiField(val) {
    return String(val || '').replace(/([\\;,:"])/g, '\\$1');
  }

  function unescapeWifiField(val) {
    return String(val || '').replace(/\\([\\;,:"])/g, '$1');
  }

  function formatWifiText({ auth = 'WPA', ssid = '', password = '', hidden = false }) {
    const t = auth === 'nopass' ? 'nopass' : (auth === 'WEP' ? 'WEP' : 'WPA');
    const parts = [`T:${t}`, `S:${escapeWifiField(ssid)}`];
    if (t !== 'nopass') parts.push(`P:${escapeWifiField(password)}`);
    if (hidden) parts.push('H:true');
    return `WIFI:${parts.join(';')};;`;
  }

  function parseWifiText(text) {
    const str = String(text || '');
    if (!str.toUpperCase().startsWith('WIFI:')) {
      return { auth: 'WPA', ssid: 'JWPLC_AP', password: '', hidden: false };
    }
    const body = str.slice(5).replace(/;;$/, '');
    const tokens = [];
    let cur = '';
    for (let i = 0; i < body.length; i += 1) {
      if (body[i] === '\\' && i + 1 < body.length) {
        cur += body[i] + body[i + 1];
        i += 1;
      } else if (body[i] === ';') {
        tokens.push(cur);
        cur = '';
      } else {
        cur += body[i];
      }
    }
    if (cur) tokens.push(cur);

    const out = { auth: 'WPA', ssid: '', password: '', hidden: false };
    tokens.forEach((tok) => {
      const idx = tok.indexOf(':');
      if (idx < 0) return;
      const k = tok.slice(0, idx).toUpperCase();
      const v = tok.slice(idx + 1);
      if (k === 'T') out.auth = v.toUpperCase() === 'NOPASS' ? 'nopass' : (v.toUpperCase() === 'WEP' ? 'WEP' : 'WPA');
      else if (k === 'S') out.ssid = unescapeWifiField(v);
      else if (k === 'P') out.password = unescapeWifiField(v);
      else if (k === 'H') out.hidden = v.toLowerCase() === 'true';
    });
    return out;
  }

  return {
    SCREEN_W,
    SCREEN_H,
    MAX_VERSION,
    MAX_SIDE,
    MAX_BYTES,
    ECC_LEVELS,
    PRESETS,
    normalizeEcc,
    clampScale,
    clampQuietZone,
    clampMaxLength,
    maxCapacityBytes,
    encode,
    computeMetrics,
    packModulesToBytes,
    contrastCheck,
    formatWifiText,
    parseWifiText
  };
});

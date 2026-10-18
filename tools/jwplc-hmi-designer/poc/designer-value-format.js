// Formato del valor de VALUE (Alpha15 I1): número, separador de miles y
// duración. Gemelo de JWPLC_Display/src/JWPLC_UI_Format.cpp: misma muestra
// (reserva de ancho), mismo texto de desborde y mismo resultado para cada
// valor. tests/value-format-parity.cjs compila el .cpp en host y compara.
// Se carga en el navegador (window.JWPLCValueFormat) y en node (require).
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCValueFormat = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  const MAX_INTEGER_DIGITS = 9;
  const MAX_DECIMAL_DIGITS = 6;
  const PATTERNS = ['SS', 'MM_SS', 'HH_MM_SS', 'COMPACT'];
  const UNITS = ['S', 'MS'];
  const SEPARATORS = ['', ',', '.', ' '];
  const DATETIME_PATTERNS = ['HH_MM', 'HH_MM_SS', 'DD_MM', 'DD_MM_YYYY', 'DD_MM_YYYY_HH_MM'];
  const DATETIME_SHAPE = {
    HH_MM: '88:88',
    HH_MM_SS: '88:88:88',
    DD_MM: '88/88',
    DD_MM_YYYY: '88/88/8888',
    DD_MM_YYYY_HH_MM: '88/88/8888 88:88'
  };

  // Modelo normalizado (lo que entiende el firmware).
  function normalize(field) {
    field = field || {};
    const kind = ['DURATION', 'DATETIME'].includes(field.valueKind) ? field.valueKind : 'NUMBER';
    const integerDigits = Math.min(MAX_INTEGER_DIGITS, Math.max(1, Math.trunc(Number(field.integerDigits) || 1)));
    return {
      kind,
      integerDigits,
      decimalDigits: kind !== 'NUMBER' ? 0 : Math.min(MAX_DECIMAL_DIGITS, Math.max(0, Math.trunc(Number(field.decimalDigits) || 0))),
      signedValue: !!field.signedValue,
      leadingZeros: kind !== 'NUMBER' ? false : !!field.leadingZeros,
      thousandsSep: kind !== 'NUMBER' ? '' : (SEPARATORS.includes(field.thousandsSep) ? field.thousandsSep : ''),
      pattern: PATTERNS.includes(field.durationPattern) ? field.durationPattern : 'MM_SS',
      unit: UNITS.includes(field.durationUnit) ? field.durationUnit : 'S',
      dateTimePattern: DATETIME_PATTERNS.includes(field.dateTimePattern) ? field.dateTimePattern : 'HH_MM'
    };
  }

  function groupThousands(f, text) {
    const point = f.thousandsSep === '.' ? ',' : '.';
    const match = /^([+-]?)(\d*)(.*)$/.exec(text);
    const digits = match[2];
    let grouped = '';
    for (let i = 0; i < digits.length; i++) {
      if (f.thousandsSep && i > 0 && (digits.length - i) % 3 === 0) grouped += f.thousandsSep;
      grouped += digits[i];
    }
    return match[1] + grouped + match[3].replace(/\./g, point);
  }

  function durationShape(f, digit) {
    const lead = digit.repeat(f.integerDigits);
    switch (f.pattern) {
      case 'MM_SS': return lead + (digit === '8' ? ':88' : ':--');
      case 'HH_MM_SS': return lead + (digit === '8' ? ':88:88' : ':--:--');
      case 'COMPACT':
        if (f.integerDigits + 4 < 6 && digit === '8') return '88m88s';
        return lead + (digit === '8' ? 'h88m' : 'h--m');
      default: return lead;
    }
  }

  // toFixed con el redondeo de printf: en un empate exacto (2.5, 0.125,
  // 987654321.25) printf redondea al par y toFixed hacia arriba. Un empate
  // termina a los pocos decimales, así que toFixed(d + 30) lo muestra exacto.
  function toFixedLikePrintf(x, d) {
    const plain = x.toFixed(d);
    const wide = Math.abs(x).toFixed(Math.min(100, d + 30));
    const dot = wide.indexOf('.');
    if (!/^50*$/.test(wide.slice(dot + 1 + d))) return plain;
    const truncated = d ? wide.slice(0, dot + 1 + d) : wide.slice(0, dot);
    if (Number(truncated[truncated.length - 1]) % 2 !== 0) return plain;
    return (x < 0 || Object.is(x, -0) ? '-' : '') + truncated;
  }

  function sample(field) {
    const f = normalize(field);
    if (f.kind === 'DATETIME') return DATETIME_SHAPE[f.dateTimePattern];
    if (f.kind === 'DURATION') return (f.signedValue ? '-' : '') + durationShape(f, '8');
    const body = (f.signedValue ? '-' : '') + '8'.repeat(f.integerDigits) + (f.decimalDigits ? '.' + '8'.repeat(f.decimalDigits) : '');
    return groupThousands(f, body);
  }

  function overflow(field) {
    const f = normalize(field);
    if (f.kind === 'DATETIME') return DATETIME_SHAPE[f.dateTimePattern].replace(/8/g, '-');
    if (f.kind === 'DURATION') return durationShape(f, '-');
    return '#'.repeat(sample(field).length);
  }

  const two = (n) => String(n).padStart(2, '0');

  function formatDuration(f, value) {
    if (!Number.isFinite(value) || (value < 0 && !f.signedValue)) return { text: durationShape(f, '-'), ok: false };
    let magnitude = Math.abs(value);
    if (f.unit === 'MS') magnitude /= 1000;
    if (magnitude >= 1e15) return { text: durationShape(f, '-'), ok: false };

    const total = Math.floor(magnitude);
    const limit = 10 ** f.integerDigits;
    const pad = Math.min(2, f.integerDigits);
    const sign = value < 0 && total > 0 ? '-' : '';
    const hours = Math.floor(total / 3600);
    const minutes = Math.floor(total / 60) % 60;
    const seconds = total % 60;
    const lead = (n) => String(n).padStart(pad, '0');

    switch (f.pattern) {
      case 'SS':
        if (total >= limit) break;
        return { text: sign + lead(total), ok: true };
      case 'MM_SS': {
        const totalMinutes = Math.floor(total / 60);
        if (totalMinutes >= limit) break;
        return { text: `${sign}${lead(totalMinutes)}:${two(seconds)}`, ok: true };
      }
      case 'HH_MM_SS':
        if (hours >= limit) break;
        return { text: `${sign}${lead(hours)}:${two(minutes)}:${two(seconds)}`, ok: true };
      case 'COMPACT':
        if (hours >= limit) break;
        if (hours > 0) return { text: `${sign}${hours}h${two(minutes)}m`, ok: true };
        if (minutes > 0) return { text: `${sign}${minutes}m${two(seconds)}s`, ok: true };
        return { text: `${sign}${seconds}s`, ok: true };
      default: break;
    }
    return { text: durationShape(f, '-'), ok: false };
  }

  // Fecha/hora (I2): clock = { valid, year, month, day, hour, minute, second }.
  function dateTime(field, clock) {
    const f = normalize(field);
    const c = clock || {};
    const ok = Boolean(c.valid) && c.month >= 1 && c.month <= 12 && c.day >= 1 && c.day <= 31 &&
      c.hour >= 0 && c.hour < 24 && c.minute >= 0 && c.minute < 60 && c.second >= 0 && c.second < 60 &&
      c.year >= 0 && c.year <= 9999;
    if (!ok) return { text: overflow(field), ok: false };
    const date = `${two(c.day)}/${two(c.month)}`;
    const year = String(c.year).padStart(4, '0');
    const hm = `${two(c.hour)}:${two(c.minute)}`;
    const text = {
      HH_MM: hm,
      HH_MM_SS: `${hm}:${two(c.second)}`,
      DD_MM: date,
      DD_MM_YYYY: `${date}/${year}`,
      DD_MM_YYYY_HH_MM: `${date}/${year} ${hm}`
    }[f.dateTimePattern];
    return { text, ok: true };
  }

  function clockFromDate(d) {
    return { valid: true, year: d.getFullYear(), month: d.getMonth() + 1, day: d.getDate(), hour: d.getHours(), minute: d.getMinutes(), second: d.getSeconds() };
  }

  // { text, ok } para un valor numérico.
  function format(field, value) {
    const f = normalize(field);
    if (f.kind === 'DATETIME') return { text: overflow(field), ok: false };
    if (f.kind === 'DURATION') return formatDuration(f, value);
    if (!Number.isFinite(value)) return { text: overflow(field), ok: false };
    const negative = value < 0;
    if (negative && !f.signedValue) return { text: overflow(field), ok: false };
    if (Math.abs(value) >= 1e15) return { text: overflow(field), ok: false };

    let out;
    if (f.leadingZeros) {
      const magnitude = toFixedLikePrintf(Math.abs(value), f.decimalDigits);
      const width = f.integerDigits + (f.decimalDigits ? 1 + f.decimalDigits : 0);
      out = (negative ? '-' : '') + magnitude.padStart(width, '0');
    } else {
      out = toFixedLikePrintf(value, f.decimalDigits);
    }
    const integerPart = out.replace(/^[+-]/, '').split('.')[0] || '';
    if (integerPart.length > f.integerDigits) return { text: overflow(field), ok: false };
    return { text: groupThousands(f, out), ok: true };
  }

  // Texto de la vista previa del Designer (field.preview es texto libre).
  // Fecha/hora: la hora del PC (o «sin RTC» si field.rtcPreview === 'INVALID').
  function preview(field, now) {
    if (normalize(field).kind === 'DATETIME') {
      return dateTime(field, field?.rtcPreview === 'INVALID' ? { valid: false } : clockFromDate(now || new Date())).text;
    }
    const raw = String(field?.preview ?? '').trim();
    if (!raw) return '';
    return format(field, Number(raw)).text;
  }

  return { normalize, sample, overflow, format, dateTime, clockFromDate, preview, PATTERNS, UNITS, SEPARATORS, DATETIME_PATTERNS };
});

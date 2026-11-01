// Núcleo puro del proyecto «Runtime» (Alpha16): el diseño se muestra como
// página HMI dentro de JWPLC_Runtime (programación en el panel, estilo LOGO!)
// y cada objeto se enlaza a una variable del Runtime en vez de a una variable
// C++ propia del sketch.
//
// El enlace vive en el nombre de la variable del objeto (rt_q1, rt_b02_t...),
// así el resto del Designer (unicidad, contrato, guardado) no cambia:
//   rt_i<n>      entrada In          JWPLC_Runtime.input(n)
//   rt_q<n>      salida Qn           JWPLC_Runtime.output(n)
//   rt_m<n>      marca Mn            JWPLC_Runtime.marker(n)
//   rt_b<nn>     salida del bloque   JWPLC_Runtime.block(nn)
//   rt_b<nn>_t   tiempo (s)          JWPLC_Runtime.blockSeconds(nn)
//   rt_b<nn>_c   cuenta              JWPLC_Runtime.counter(nn)
// Un sufijo _2, _3... permite mostrar la misma variable en dos objetos.
//
// toRuntimeHeader() convierte el header Arduino del codegen en el del Runtime:
// hooks jwplcRuntimeHMISetup / PageEnter / Update (ver JWPLC_Runtime.h).
// Se carga en el navegador (window.JWPLCHMIRuntimeCore) y en node (tests).
(function (root, factory) {
  const api = factory();
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCHMIRuntimeCore = api;
})(typeof self !== 'undefined' ? self : this, () => {
  'use strict';

  // Límites del Runtime (rt/RtProgram.h): NUM_I, NUM_Q, NUM_M y MAX_BLOCKS.
  const LIMITS = Object.freeze({ I: 8, Q: 8, M: 8, B: 64 });

  const KINDS = Object.freeze({
    I: { label: 'Entrada', method: 'input', type: 'bool' },
    Q: { label: 'Salida', method: 'output', type: 'bool' },
    M: { label: 'Marca', method: 'marker', type: 'bool' },
    B: { label: 'Bloque', method: 'block', type: 'bool' },
    T: { label: 'Tiempo', method: 'blockSeconds', type: 'number', unit: 's' },
    C: { label: 'Cuenta', method: 'counter', type: 'number' }
  });

  const pad2 = (n) => String(n).padStart(2, '0');

  /** «Q1», «B05», «B05.t», «B05.c» a partir del nombre de variable; null si no es del Runtime. */
  function parseSymbol(symbol) {
    const s = String(symbol || '').trim().toLowerCase();
    let m = /^rt_([iqm])(\d{1,2})(?:_\d+)?$/.exec(s);
    if (m) {
      const kind = m[1].toUpperCase();
      const n = Number(m[2]);
      if (n < 1 || n > LIMITS[kind]) return null;
      return { kind, n, key: `${kind}${n}` };
    }
    m = /^rt_b(\d{1,2})(?:_([tc]))?(?:_\d+)?$/.exec(s);
    if (m) {
      const n = Number(m[1]);
      if (n < 1 || n > LIMITS.B) return null;
      const kind = m[2] ? m[2].toUpperCase() : 'B';
      return { kind, n, key: `B${pad2(n)}${m[2] ? `.${m[2]}` : ''}` };
    }
    return null;
  }

  /** Expresión C++ que lee la variable del Runtime (o null). */
  function expressionFor(symbol) {
    const v = parseSymbol(symbol);
    return v ? `JWPLC_Runtime.${KINDS[v.kind].method}(${v.n})` : null;
  }

  /** Nombre de variable base para una clave «Q1», «B05.t»... */
  function symbolFor(key) {
    const m = /^([IQMB])(\d{1,2})(?:\.([tc]))?$/i.exec(String(key || '').trim());
    if (!m) return null;
    const kind = m[1].toLowerCase();
    const n = Number(m[2]);
    return kind === 'b' ? `rt_b${pad2(n)}${m[3] ? `_${m[3].toLowerCase()}` : ''}` : `rt_${kind}${n}`;
  }

  /**
   * Opciones del selector «Variable del Runtime» según el objeto.
   * wants: 'bool' (BOOL, ICON de dos estados) o 'number' (VALUE, BAR).
   * Las variables bool también sirven para VALUE (0/1); tiempo y cuenta sólo
   * para números.
   */
  function catalog(wants, blockCount = 16) {
    const groups = [];
    const bits = (kind) => Array.from({ length: LIMITS[kind] }, (_, i) => ({
      key: `${kind}${i + 1}`, symbol: `rt_${kind.toLowerCase()}${i + 1}`, label: `${kind}${i + 1}`
    }));
    groups.push({ label: 'Entradas I', options: bits('I') });
    groups.push({ label: 'Salidas Q', options: bits('Q') });
    groups.push({ label: 'Marcas M', options: bits('M') });
    const nb = Math.max(1, Math.min(LIMITS.B, Number(blockCount) || 16));
    const blocks = (suffix, label) => Array.from({ length: nb }, (_, i) => ({
      key: `B${pad2(i + 1)}${suffix ? `.${suffix}` : ''}`,
      symbol: `rt_b${pad2(i + 1)}${suffix ? `_${suffix}` : ''}`,
      label: `B${pad2(i + 1)} · ${label}`
    }));
    if (wants === 'number') {
      groups.push({ label: 'Temporizadores (tiempo transcurrido, s)', options: blocks('t', 'tiempo (s)') });
      groups.push({ label: 'Contadores (cuenta)', options: blocks('c', 'cuenta') });
    }
    groups.push({ label: 'Salida de un bloque', options: blocks('', 'salida') });
    return groups;
  }

  const SETTER_RE = /^(\s*)JWPLC_Display\.(setBool|setValue|setBar|setText|setIconState)\((\w+), ([A-Za-z_]\w*)\);$/gm;

  function replaceBlock(text, startRe, replacement) {
    const m = startRe.exec(text);
    if (!m) return text;
    const open = text.indexOf('{', m.index + m[0].length - 1);
    if (open < 0) return text;
    let depth = 0;
    for (let i = open; i < text.length; i += 1) {
      if (text[i] === '{') depth += 1;
      else if (text[i] === '}') {
        depth -= 1;
        if (depth === 0) return text.slice(0, m.index) + replacement + text.slice(i + 1);
      }
    }
    return text;
  }

  /**
   * Header Arduino del codegen -> header del Runtime.
   * opts.pageCount: páginas del diseño.
   * Devuelve { text, unbound: [ids sin variable del Runtime], unsupported: [...] }.
   */
  function toRuntimeHeader(source, opts = {}) {
    // Idempotente: el Designer puede volver a pasar el header ya convertido.
    let text = String(source || '').replace(/^(?:\/\/ (?:ERROR RUNTIME|AVISO): .*\n)+\n/, '');
    const pageCount = Math.max(1, Number(opts.pageCount) || 1);
    const unbound = [];
    const unsupported = [];

    if (/JWPLC_Display\.setMenus\(|JWPLC_UIMenu/.test(text)) unsupported.push('MENU');
    if (/JWPLC_UIEventLog|JWPLC_Display\.setEventLog/.test(text)) unsupported.push('EVENT_LOG');

    if (!text.includes('#include <JWPLC_Runtime.h>')) {
      text = text.replace('#include <JWPLC_Display.h>', '#include <JWPLC_Display.h>\n#include <JWPLC_Runtime.h>');
    }
    const hasFields = /static const JWPLC_UIField HMI_FIELDS\[\]/.test(text);
    if (!text.includes('// Proyecto «Runtime»')) {
      text = text.replace(
        '// API pública JWPLC_UI · Alpha11 A11-4',
        '// API pública JWPLC_UI · Alpha11 A11-4\n// Proyecto «Runtime»: página HMI dentro de JWPLC_Runtime (variables I, Q, M y bloques).');
    }

    // Sin variables propias: los valores salen del programa del Runtime.
    text = text.replace(
      /\/\/ Variables HMI\n[\s\S]*?\n\n\/\/ Definición declarativa/,
      '// Variables HMI: las del programa del Runtime (JWPLC_Runtime.input/output/...).\n\n// Definición declarativa');

    text = text.replace(/^\/\/ El Runtime la llama al arrancar \(JWPLC_Runtime\.begin\(\)\)\.\n(?=extern "C" void jwplcRuntimeHMISetup)/m, '');
    text = replaceBlock(text, /^(?:void jwplcHMISetup\(\)|extern "C" void jwplcRuntimeHMISetup\(void\))\n/m,
      `// El Runtime la llama al arrancar (JWPLC_Runtime.begin()).\n` +
      `extern "C" void jwplcRuntimeHMISetup(void)\n` +
      `{\n` +
      (hasFields
        ? `    JWPLC_Runtime.setHMI(\n` +
          `        HMI_FIELDS,\n` +
          `        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]),\n` +
          `        ${pageCount});\n`
        : `    JWPLC_Runtime.setHMI(nullptr, 0, ${pageCount}); // solo gráfica estática\n`) +
      `}`);

    text = text.replace(
      /^void jwplcUIPageEnter\(uint8_t page\)$/m,
      'extern "C" void jwplcRuntimeHMIPageEnter(uint8_t page)');
    text = text.replace(/jwplcUIPageEnter\(\)/g, 'jwplcRuntimeHMIPageEnter()');
    // El callback del sketch no aplica: la página la abre el Runtime.
    text = text.replace(/^\/\/ Para lógica propia al cambiar de página, define en el sketch:\n\/\/   extern "C" void jwplcHMIUserPageEnter\(uint8_t page\) \{ \.\.\. \}\nextern "C" void jwplcHMIUserPageEnter\(uint8_t page\) __attribute__\(\(weak\)\);\n\n/m, '');
    text = text.replace(/\n\n    if \(jwplcHMIUserPageEnter\)\n    \{\n        jwplcHMIUserPageEnter\(page\);\n    \}\n\}/, '\n}');

    text = text.replace(
      /^\/\/ La lógica de aplicación debe permanecer en loop\(\)\.\nvoid jwplcUIUpdate\(\)\n\{\n    switch \(JWPLC_Display\.userPage\(\)\)/m,
      '// Los valores salen del programa en marcha del Runtime.\nextern "C" void jwplcRuntimeHMIUpdate(uint8_t page)\n{\n    switch (page)');
    text = text.replace(
      /^\/\/ La lógica de aplicación debe permanecer en loop\(\)\.\nvoid jwplcUIUpdate\(\)\n\{/m,
      '// Los valores salen del programa en marcha del Runtime.\nextern "C" void jwplcRuntimeHMIUpdate(uint8_t page)\n{\n    (void)page;');

    text = text.replace(SETTER_RE, (line, indent, setter, id, variable) => {
      // TEXT: el Runtime no tiene variables de texto.
      const expr = setter === 'setText' ? null : expressionFor(variable);
      if (expr) return `${indent}JWPLC_Display.${setter}(${id}, ${expr});`;
      return `${indent}// ${id}: sin variable del Runtime (Datos > Variable del Runtime).`;
    });
    for (const m of text.matchAll(/^\s*\/\/ (\w+): sin variable del Runtime/gm)) unbound.push(m[1]);

    const notes = [];
    if (unsupported.length) {
      notes.push(`// ERROR RUNTIME: ${unsupported.join(' y ')} no se admite en una página del Runtime (el Runtime tiene su propio menú). Quítalo del diseño.`);
    }
    if (unbound.length) {
      notes.push(`// AVISO: ${unbound.length} objeto(s) sin variable del Runtime: ${unbound.join(', ')}.`);
    }
    if (notes.length) text = `${notes.join('\n')}\n\n${text}`;
    return { text, unbound, unsupported };
  }

  return { LIMITS, KINDS, parseSymbol, expressionFor, symbolFor, catalog, toRuntimeHeader };
});

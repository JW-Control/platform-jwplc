(function (root) {
  'use strict';

  // Alpha15 · Guía del sketch (JWPLC_HMI_LEEME.md).
  //
  // «Escribir en sketch» la deja junto a JWPLC_HMI_Generated.h: un .ino base
  // con todas las variables del HMI ya usadas y los pasos para un sketch que ya
  // existe. El Designer nunca modifica el .ino del usuario; esta guía se
  // reescribe cada vez (lleva GUIDE_MARK en la primera línea y el escritorio
  // sólo reemplaza un archivo que la tenga).
  //
  // Se construye leyendo el header ya generado (no el modelo): lo que se
  // describe es exactamente lo que compila. Puro: también corre en node
  // (tests/sketch-guide.test.cjs compila el .ino base contra los golden).

  const GUIDE_NAME = 'JWPLC_HMI_LEEME.md';
  const HEADER_NAME = 'JWPLC_HMI_Generated.h';
  const GUIDE_MARK = '<!-- Generado por JWPLC HMI Designer';

  const DECL = /^(char|bool|float|double|int8_t|uint8_t|int16_t|uint16_t|int32_t|uint32_t|int|long)\s+([A-Za-z_]\w*)(\[(\d+)\])?\s*=\s*([^;]*);\s*(?:\/\/\s*(.*))?$/;

  // Bloque «// Variables HMI»: grupos por página o por MENU.
  function parseVariables(header) {
    const lines = String(header || '').split(/\r?\n/);
    const start = lines.findIndex((l) => l.trim() === '// Variables HMI');
    if (start < 0) return [];
    const vars = [];
    let group = '';
    for (let i = start + 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      if (/^\/\/\s*Definición declarativa/.test(line) || /^(static|enum|#|void|extern)/.test(line)) break;
      if (line.startsWith('//')) { group = line.replace(/^\/\/\s*/, ''); continue; }
      const m = DECL.exec(line);
      if (!m) continue;
      vars.push({
        type: m[1], name: m[2], array: m[4] ? Number(m[4]) : 0, initial: m[5].trim(),
        comment: m[6] || '', group, menu: /^MENU\b/.test(group)
      });
    }
    return vars;
  }

  // MENU: nodos en el orden del enum «Nodos de MENU_X»; editables = los que
  // informa JWPLC_Display.menuChanged().
  function parseMenus(header) {
    const text = String(header || '');
    const menus = [];
    const re = /\/\/ Nodos de (MENU_\w+)[^\n]*\nenum : uint8_t\s*\{([^}]*)\}/g;
    let m;
    while ((m = re.exec(text))) {
      const id = m[1];
      const names = [...m[2].matchAll(/(\w+)\s*=\s*\d+/g)].map((x) => x[1]);
      const arrayRe = new RegExp(`HMI_${id}_NODES\\[\\]\\s*=\\s*\\{([\\s\\S]*?)\\n\\};`);
      const body = arrayRe.exec(text)?.[1] || '';
      const nodes = body.split('\n').map((l) => l.trim()).filter((l) => l.startsWith('JWPLC_UIMenu'));
      menus.push({
        id,
        nodes: names.map((name, i) => {
          const line = nodes[i] || '';
          return {
            name,
            label: /"((?:[^"\\]|\\.)*)"/.exec(line)?.[1] || '',
            variable: /&(\w+)/.exec(line)?.[1] || '',
            editable: /^JWPLC_UIMenuEdit/.test(line)
          };
        })
      });
    }
    return menus;
  }

  // Variables que cambia el operador (nodos editables del MENU). Las de nodos
  // «Ver» las escribe la aplicación, como las de los campos.
  function operatorVariables(menus) {
    return new Set(menus.flatMap((m) => m.nodes.filter((n) => n.editable && n.variable).map((n) => n.variable)));
  }

  const hasClockFields = (header) => /JWPLC_UIDateTimeFormat\(/.test(String(header || ''));

  // Una línea de ejemplo por variable de pantalla (las de MENU las cambia el operador).
  function exampleLine(v) {
    if (v.type === 'char' && v.array) return `strncpy(${v.name}, "Listo", sizeof(${v.name}) - 1); // TEXT: ${v.comment || `${v.array - 1} bytes`}`;
    if (/^ICON:/.test(v.comment)) return `${v.name} = ${v.type === 'bool' ? 'false' : '0'}; // ${v.comment}`;
    if (v.type === 'bool') return `${v.name} = false; // p. ej. digitalRead(I0_0) o el estado de tu lógica`;
    if (v.type === 'float' || v.type === 'double') return `${v.name} = 0.0f; // p. ej. la lectura de tu sensor`;
    return `${v.name} = 0; // p. ej. un contador de tu lógica`;
  }

  function baseSketch(header) {
    const vars = parseVariables(header);
    const menus = parseMenus(header);
    const out = ['#include "JWPLC_HMI_Generated.h"', '', 'void setup()', '{', '    jwplcHMISetup(); // páginas, campos, MENU e IDLE del Designer', '}', '', 'void loop()', '{'];
    const byOperator = operatorVariables(menus);
    const screen = vars.filter((v) => !byOperator.has(v.name));
    if (screen.length) {
      out.push('    // Escribe las variables: la pantalla redibuja sólo lo que cambió.');
      let group = null;
      screen.forEach((v) => {
        if (v.group !== group) { group = v.group; if (group) out.push(`    // ${group}`); }
        out.push(`    ${exampleLine(v)}`);
      });
    }
    let nodoDeclared = false; // una sola declaración aunque haya varios MENU
    menus.forEach((menu) => {
      const editable = menu.nodes.filter((n) => n.editable);
      if (!editable.length) return;
      if (out[out.length - 1] !== '{') out.push('');
      out.push(`    // ${menu.id}: el operador cambió un valor (OK en la botonera).`);
      if (!nodoDeclared) { out.push('    uint8_t nodo;'); nodoDeclared = true; }
      out.push(
        `    if (JWPLC_Display.menuChanged(${menu.id}, &nodo))`,
        '    {',
        '        switch (nodo)',
        '        {');
      editable.forEach((n) => {
        out.push(`        case ${n.name}:`, `            // «${n.label}» -> ${n.variable || 'variable'} ya tiene el valor nuevo`, '            break;');
      });
      out.push('        default:', '            break;', '        }', '    }');
    });
    if (out[out.length - 1] === '{') out.push('    // Tu lógica aquí (sin delay(): la botonera y la pantalla siguen solas).');
    out.push('}');
    return out.join('\n');
  }

  const mdCell = (s) => String(s ?? '').replace(/\|/g, '\\|');

  function build(options = {}) {
    const header = String(options.header || '');
    const sketch = options.sketchName || 'sketch';
    const projectFile = options.projectFile || `${sketch}.jwhmi`;
    const vars = parseVariables(header);
    const menus = parseMenus(header);
    const byOperator = operatorVariables(menus);
    const lines = [
      `${GUIDE_MARK}: se reescribe en cada «Escribir en sketch». No la edites: tus notas, en otro archivo. -->`,
      `# HMI del sketch «${sketch}»`,
      '',
      'Archivos que escribe el JWPLC HMI Designer en esta carpeta:',
      '',
      '| Archivo | Qué es | ¿Se edita a mano? |',
      '|---|---|---|',
      `| \`${HEADER_NAME}\` | Pantallas, campos, MENU e IDLE en C++ | No: se reemplaza al escribir |`,
      `| \`${projectFile}\` | El diseño, para volver a abrirlo en el Designer | No |`,
      `| \`${GUIDE_NAME}\` | Esta guía | No |`,
      '',
      `**Tu \`${sketch}.ino\` es tuyo: el Designer nunca lo modifica.** Para empezar uno nuevo, copia el`,
      'sketch base de abajo; si ya tienes uno, sigue los tres pasos.',
      '',
      '## Sketch base',
      '',
      '```cpp',
      baseSketch(header),
      '```',
      '',
      '## Si ya tienes un sketch',
      '',
      `1. Arriba de todo: \`#include "${HEADER_NAME}"\`.`,
      '2. Dentro de `setup()`, una vez: `jwplcHMISetup();`.',
      '3. En `loop()` asigna las variables de la tabla. No hace falta dibujar nada: el runtime redibuja',
      '   sólo los campos que cambiaron. No uses `delay()`: la botonera y la pantalla siguen solas.',
      '',
      `No vuelvas a declarar estas variables en tu \`.ino\`: ya están en \`${HEADER_NAME}\` (declararlas`,
      'otra vez da error de redefinición). Si cambias un nombre en el Designer, cámbialo también en tu código.',
      ''
    ];
    if (vars.length) {
      lines.push('## Variables del HMI', '', '| Variable | Tipo C++ | Dónde | Nota |', '|---|---|---|---|');
      vars.forEach((v) => {
        const type = v.array ? `${v.type}[${v.array}]` : v.type;
        const note = byOperator.has(v.name) ? 'La cambia el operador desde el MENU; léela en tu lógica'
          : v.menu ? 'El MENU la muestra («Ver»); escríbela en tu lógica' : (v.comment || `Valor inicial ${v.initial}`);
        lines.push(`| \`${v.name}\` | \`${type}\` | ${mdCell(v.group)} | ${mdCell(note)} |`);
      });
      lines.push('');
    }
    if (menus.length) {
      lines.push('## MENU', '');
      menus.forEach((menu) => {
        const editable = menu.nodes.filter((n) => n.editable);
        lines.push(`\`${menu.id}\`: ${editable.length
          ? `\`JWPLC_Display.menuChanged(${menu.id}, &nodo)\` devuelve \`true\` una vez cuando el operador confirma un valor; \`nodo\` es uno de: ${editable.map((n) => `\`${n.name}\``).join(', ')}.`
          : 'sólo muestra valores (no tiene ítems editables).'}`, '');
      });
    }
    if (hasClockFields(header)) {
      lines.push('## Fecha y hora', '', 'Los VALUE de fecha/hora no tienen variable: el firmware los llena desde el RTC de la placa.', '');
    }
    lines.push(`Generado el ${options.date || new Date().toISOString().slice(0, 10)} por el JWPLC HMI Designer.`, '');
    return lines.join('\n');
  }

  const api = { GUIDE_NAME, GUIDE_MARK, parseVariables, parseMenus, baseSketch, build };
  if (typeof module === 'object' && module.exports) module.exports = api;
  else root.JWPLCSketchGuide = api;
})(typeof window !== 'undefined' ? window : globalThis);

// Gate 8C: programa de prueba con una pantalla del HMI Designer (test/hmi/Demo.json,
// escrita con la CLI del Designer en %USERPROFILE%\JWPLC-HMI\Demo).
// - Al arrancar: usa la pantalla "Demo" y pone el estado "LISTO".
// - Cada 1 s: temperatura del RTC, piezas = cronómetro, nivel = cronómetro módulo 100,
//   bomba = I0.0. Si modoAuto (MENU) y temperatura > consigna (MENU): Q0.0 y estado "CALIENTE".
// Requiere jwplc_basic (cdp-add-device.js). Uso: node cdp.cjs cdp-program-hmi.js
(async () => {
    const A = __jwA;
    const b = A.blocklyCtx;
    const ws = b.workspace;
    const B = b.blockly;
    ws.clear();
    const J = 'jwplc_basic.';
    const num = (n) => `<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
    const txt = (s) => `<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
    const r = (op, inner = '') => `<block type="${J}${op}">${inner}</block>`;
    const nx = (a, rest) => a.replace(/<\/block>$/, `<next>${rest}</next></block>`);
    // Valor: texto fijo dentro del hueco o un bloque encima de una sombra vacía.
    const slot = (v) => (typeof v === 'string' && !v.startsWith('<') ? txt(v) : txt('') + v);
    const set = (name, value) => r('jwplc_hmi_set', `<value name="VAR">${txt(name)}</value><value name="VALUE">${slot(value)}</value>`);
    const get = (name) => r('jwplc_hmi_get', `<value name="VAR">${txt(name)}</value>`);
    const getBool = (name) => r('jwplc_hmi_get_bool', `<value name="VAR">${txt(name)}</value>`);
    const mod = (a, n) => `<block type="operator_mod"><value name="NUM1">${num(0)}${a}</value><value name="NUM2">${num(n)}</value></block>`;
    const gt = (a, c) => `<block type="operator_gt"><value name="OPERAND1">${txt('')}${a}</value><value name="OPERAND2">${txt('')}${c}</value></block>`;
    const and = (a, c) => `<block type="operator_and"><value name="OPERAND1">${a}</value><value name="OPERAND2">${c}</value></block>`;
    const out = (state) => r('jwplc_write_output', `<field name="Q">Q0_0</field><field name="STATE">${state}</field>`);

    const xml = `<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="${J}jwplc_when_start" x="20" y="20"><next>
  ${nx(r('jwplc_hmi_use', `<value name="NAME">${txt('Demo')}</value>`), set('estado', 'LISTO'))}
 </next></block>
 <block type="${J}jwplc_every_seconds" x="20" y="200"><value name="SECS">${num(1)}</value><next>
  ${nx(set('temperatura', r('jwplc_clock_temperature')),
        nx(set('piezas', r('jwplc_timer_seconds')),
            nx(set('nivel', mod(r('jwplc_timer_seconds'), 100)),
                nx(set('bomba', r('jwplc_read_input', '<field name="I">I0_0</field>')),
                    `<block type="control_if_else">
                       <value name="CONDITION">${and(getBool('modoAuto'), gt(r('jwplc_clock_temperature'), get('consigna')))}</value>
                       <statement name="SUBSTACK">${nx(out('HIGH'), set('estado', 'CALIENTE'))}</statement>
                       <statement name="SUBSTACK2">${nx(out('LOW'), set('estado', 'LISTO'))}</statement>
                     </block>`))))}
 </next></block>
 </xml>`;
    B.Xml.domToWorkspace(B.Xml.textToDom(xml), ws);
    await new Promise((res) => setTimeout(res, 2500));
    if (!window.monaco) {
        const t = document.querySelector('button.tigger');
        if (t) t.click();
    }
    for (let i = 0; i < 30; i++) {
        const v = window.monaco && window.monaco.editor.getModels().map((x) => x.getValue()).find((x) => x.includes('JWPLC Basic'));
        if (v) return 'BLOCKS=' + ws.getAllBlocks().length + '\n' + v;
        await new Promise((res) => setTimeout(res, 500));
    }
    return 'sin código';
})()

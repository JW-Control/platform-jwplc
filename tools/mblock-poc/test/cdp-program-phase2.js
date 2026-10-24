// Gate 5: programa de prueba con los bloques de la fase 2 (tiempo, memoria FRAM, microSD).
// - Al arrancar: pone en hora el reloj, suma 1 a la memoria 1 (contador de arranques) y lo muestra.
// - Cada 1 s: hora, fecha y temperatura en pantalla; si hay microSD agrega "hora;temperatura" a registro.csv.
// - OK: reinicia el cronómetro. ▲: muestra el cronómetro. ▼: muestra el minuto actual.
// - ▶: muestra si existe registro.csv. ESC: borra registro.csv y hace un pitido.
// Requiere el Device jwplc_basic añadido (cdp-add-device.js). Uso: node cdp.cjs cdp-program-phase2.js
(async () => {
    const A = __jwA;
    const b = A.blocklyCtx;
    const ws = b.workspace;
    const B = b.blockly;
    ws.clear();
    const J = 'jwplc_basic.';
    const num = (n) => `<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
    const txt = (s) => `<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
    const join = (a, c) => `<block type="operator_join"><value name="STRING1">${txt('')}${a}</value><value name="STRING2">${txt('')}${c}</value></block>`;
    const lit = (s) => `<block type="text"><field name="TEXT">${s}</field></block>`;
    const write = (line, value, next = '') =>
        `<block type="${J}jwplc_screen_write"><field name="LINE">${line}</field><value name="TEXT">${txt('')}${value}</value>${next}</block>`;
    const r = (op, inner = '') => `<block type="${J}${op}">${inner}</block>`;
    const onButton = (btn, body, x, y) =>
        `<block type="${J}jwplc_when_button" x="${x}" y="${y}"><field name="B">${btn}</field><next>${body}</next></block>`;

    const xml = `<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="${J}jwplc_when_start" x="20" y="20"><next>
  <block type="${J}jwplc_clock_set_from_pc"><next>
  <block type="${J}jwplc_memory_save">
    <value name="VALUE">${num(0)}<block type="operator_add"><value name="NUM1">${num(0)}${r('jwplc_memory_load', `<value name="SLOT">${num(1)}</value>`)}</value><value name="NUM2">${num(1)}</value></block></value>
    <value name="SLOT">${num(1)}</value>
  <next>${write(1, join(lit('Arranques: '), r('jwplc_memory_load', `<value name="SLOT">${num(1)}</value>`)))}</next>
  </block></next></block></next></block>

 <block type="${J}jwplc_every_seconds" x="20" y="300"><value name="SECS">${num(1)}</value><next>
  ${write(2, r('jwplc_clock_time_text'), `<next>${write(3, r('jwplc_clock_date_text'), `<next>${write(4, join(lit('Temp C: '), r('jwplc_clock_temperature')), `<next>
   <block type="control_if"><value name="CONDITION">${r('jwplc_sd_ready')}</value><statement name="SUBSTACK">
    <block type="${J}jwplc_sd_append">
      <value name="TEXT">${txt('')}${join(r('jwplc_clock_time_text'), join(lit(';'), r('jwplc_clock_temperature')))}</value>
      <value name="FILE">${txt('registro.csv')}</value>
    </block></statement></block></next>`)}</next>`)}</next>`)}
 </next></block>

 ${onButton('BTN_OK', r('jwplc_timer_reset'), 520, 20)}
 ${onButton('BTN_UP', write(4, join(lit('Crono s: '), r('jwplc_timer_seconds'))), 520, 140)}
 ${onButton('BTN_DOWN', write(4, join(lit('Minuto: '), r('jwplc_clock_unit', '<field name="UNIT">minute</field>'))), 520, 260)}
 ${onButton('BTN_RIGHT', write(4, r('jwplc_sd_exists', `<value name="FILE">${txt('registro.csv')}</value>`)), 520, 380)}
 ${onButton('BTN_ESC', r('jwplc_sd_remove', `<value name="FILE">${txt('registro.csv')}</value><next>${r('jwplc_beep')}</next>`), 520, 500)}
 </xml>`;
    B.Xml.domToWorkspace(B.Xml.textToDom(xml), ws);
    await new Promise((res) => setTimeout(res, 2500));
    if (!document.querySelector('.monaco-editor')) {
        const t = document.querySelector('button.tigger');
        if (t) t.click();
        await new Promise((res) => setTimeout(res, 2500));
    }
    return 'BLOCKS=' + ws.getAllBlocks().length + '\n' +
        window.monaco.editor.getModels().map((x) => x.getValue()).filter((v) => v.includes('JWPLC Basic')).join('\n=====\n');
})()

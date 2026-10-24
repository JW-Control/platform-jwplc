// Gate 6: programa de prueba con los bloques WiFi de la fase 3 (mensajes entre placas).
// - Al arrancar: canal 1, conecta a la red WiFi y muestra la IP.
// - Cada 2 s: si hay WiFi, envía "contador" con un valor que sube.
// - Cuando recibe "contador": muestra el valor en la línea 3.
// - OK envía "boton"; cuando recibe "boton" suena un pitido.
// Cambiar SSID/clave antes de subir. Requiere jwplc_basic (cdp-add-device.js).
// Uso: node cdp.cjs cdp-program-phase3.js
(async () => {
    const A = __jwA;
    const b = A.blocklyCtx;
    const ws = b.workspace;
    const B = b.blockly;
    ws.clear();
    ws.createVariable('cuenta', '', 'jwVarCuenta');
    const J = 'jwplc_basic.';
    const num = (n) => `<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
    const txt = (s) => `<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
    const lit = (s) => `<block type="text"><field name="TEXT">${s}</field></block>`;
    const join = (a, c) => `<block type="operator_join"><value name="STRING1">${txt('')}${a}</value><value name="STRING2">${txt('')}${c}</value></block>`;
    const v = '<block type="data_variable"><field name="VARIABLE" id="jwVarCuenta" variabletype="">cuenta</field></block>';
    const write = (line, value, next = '') =>
        `<block type="${J}jwplc_screen_write"><field name="LINE">${line}</field><value name="TEXT">${txt('')}${value}</value>${next}</block>`;
    const r = (op, inner = '') => `<block type="${J}${op}">${inner}</block>`;

    const xml = `<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="${J}jwplc_when_start" x="20" y="20"><next>
  <block type="${J}jwplc_net_channel"><value name="CH">${num(1)}</value><next>
  ${write(1, lit('Conectando...'), `<next>
  <block type="${J}jwplc_wifi_connect"><value name="SSID">${txt('MiRed')}</value><value name="PASS">${txt('clave')}</value><next>
  ${write(1, join(lit('IP '), r('jwplc_wifi_ip')))}
  </next></block></next>`)}
  </next></block></next></block>

 <block type="${J}jwplc_every_seconds" x="20" y="300"><value name="SECS">${num(2)}</value><next>
  <block type="control_if"><value name="CONDITION">${r('jwplc_wifi_connected')}</value><statement name="SUBSTACK">
   <block type="data_changevariableby"><field name="VARIABLE" id="jwVarCuenta" variabletype="">cuenta</field><value name="VALUE">${num(1)}</value><next>
   <block type="${J}jwplc_net_send_value"><value name="MSG">${txt('contador')}</value><value name="VALUE">${num(0)}${v}</value><next>
   ${write(2, join(lit('Enviado '), v))}
   </next></block></next></block>
  </statement></block>
 </next></block>

 <block type="${J}jwplc_net_when_received" x="520" y="20"><value name="MSG">${txt('contador')}</value><next>
  ${write(3, join(lit('Recibido '), r('jwplc_net_value', `<value name="MSG">${txt('contador')}</value>`)))}
 </next></block>

 <block type="${J}jwplc_when_button" x="520" y="200"><field name="B">BTN_OK</field><next>
  <block type="${J}jwplc_net_send"><value name="MSG">${txt('boton')}</value></block>
 </next></block>

 <block type="${J}jwplc_net_when_received" x="520" y="320"><value name="MSG">${txt('boton')}</value><next>
  <block type="${J}jwplc_beep"><next>${write(4, lit('Llego boton'))}</next></block>
 </next></block>
 </xml>`;
    B.Xml.domToWorkspace(B.Xml.textToDom(xml), ws);
    await new Promise((res) => setTimeout(res, 2500));
    if (!window.monaco) {
        const t = document.querySelector('button.tigger');
        if (t) t.click();
    }
    // El panel de vista previa (Monaco) puede tardar en cargarse la primera vez.
    for (let i = 0; i < 30 && !window.monaco; i++) await new Promise((res) => setTimeout(res, 500));
    return 'BLOCKS=' + ws.getAllBlocks().length + '\n' +
        window.monaco.editor.getModels().map((x) => x.getValue()).filter((x) => x.includes('JWPLC Basic')).join('\n=====\n');
})()

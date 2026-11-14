// Gate 7: programas de prueba de la fase 4 (RS485, Ethernet, Modbus).
// Uso: window.__jwPhase4 = 'A' | 'B' | 'C' y luego node cdp.cjs cdp-program-phase4.js
//   (o: node cdp.cjs -e "window.__jwPhase4='B'" antes de ejecutarlo; por defecto 'A').
// A: Ethernet DHCP + servidor Modbus TCP + texto por RS485.
//    Cada 1 s: registro 1 = segundos del cronómetro, IP en la línea 1, "t=..." por RS485.
//    Al llegar una línea por RS485: se muestra en la línea 2. Línea 3: registro 2 (lo escribe un SCADA).
// B: Ethernet con IP fija + Modbus RTU maestro: cada 1 s lee el registro 1 del esclavo 2
//    (línea 2) y escribe el contador en su registro 2.
// C: Modbus RTU esclavo ID 2: registro 1 = segundos; si el registro 2 es impar, Q0.0 encendida.
// Requiere jwplc_basic (cdp-add-device.js).
(async () => {
    const variant = window.__jwPhase4 || 'A';
    const A = __jwA;
    const b = A.blocklyCtx;
    const ws = b.workspace;
    const B = b.blockly;
    ws.clear();
    ws.createVariable('n', '', 'jwVarN');
    const J = 'jwplc_basic.';
    const num = (n) => `<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
    const txt = (s) => `<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
    // Texto fijo: va dentro del hueco (sombra); un bloque "texto" suelto se ve en blanco.
    const lit = (s) => ({ text: s });
    const slot = (v) => (typeof v === 'object' ? txt(v.text) : txt('') + v);
    const nslot = (v) => (typeof v === 'number' ? num(v) : num(0) + v);
    const join = (a, c) => `<block type="operator_join"><value name="STRING1">${slot(a)}</value><value name="STRING2">${slot(c)}</value></block>`;
    const write = (line, value, next = '') =>
        `<block type="${J}jwplc_screen_write"><field name="LINE">${line}</field><value name="TEXT">${slot(value)}</value>${next}</block>`;
    const r = (op, inner = '') => `<block type="${J}${op}">${inner}</block>`;
    const nx = (a, rest) => a.replace(/<\/block>$/, `<next>${rest}</next></block>`);
    const varN = '<block type="data_variable"><field name="VARIABLE" id="jwVarN" variabletype="">n</field></block>';
    const regGet = (n) => r('jwplc_reg_get', `<value name="N">${num(n)}</value>`);
    const regSet = (n, v) => r('jwplc_reg_set', `<value name="N">${num(n)}</value><value name="VALUE">${nslot(v)}</value>`);
    const start = (body) => `<block type="${J}jwplc_when_start" x="20" y="20"><next>${body}</next></block>`;
    const every = (secs, body, y) => `<block type="${J}jwplc_every_seconds" x="20" y="${y}"><value name="SECS">${num(secs)}</value><next>${body}</next></block>`;

    let xml;
    if (variant === 'A') {
        xml = start(nx(r('jwplc_eth_dhcp'), nx(r('jwplc_mbtcp_server'),
            r('jwplc_rs485_begin', '<field name="BAUD">115200</field>')))) +
            every(1, nx(regSet(1, r('jwplc_timer_seconds')),
                nx(write(1, join(lit('IP '), r('jwplc_eth_ip'))),
                    nx(write(3, join(lit('R2 = '), regGet(2))),
                        r('jwplc_rs485_send_line', `<value name="TEXT">${slot(join(lit('t='), regGet(1)))}</value>`)))), 250) +
            `<block type="${J}jwplc_rs485_when_line" x="500" y="20"><next>${write(2, r('jwplc_rs485_last_line'))}</next></block>`;
    } else if (variant === 'B') {
        xml = start(nx(r('jwplc_eth_static', `<value name="IP">${txt('192.168.0.50')}</value><value name="GW">${txt('192.168.0.1')}</value>`),
            r('jwplc_mbrtu_master', '<field name="BAUD">115200</field>'))) +
            every(1, nx(`<block type="data_changevariableby"><field name="VARIABLE" id="jwVarN" variabletype="">n</field><value name="VALUE">${num(1)}</value></block>`,
                nx(write(1, join(lit('IP '), r('jwplc_eth_ip'))),
                    nx(write(2, join(lit('R1 esclavo: '), r('jwplc_mbrtu_read', `<value name="N">${num(1)}</value><value name="ID">${num(2)}</value>`))),
                        r('jwplc_mbrtu_write', `<value name="VALUE">${nslot(varN)}</value><value name="N">${num(2)}</value><value name="ID">${num(2)}</value>`)))), 250);
    } else {
        xml = start(r('jwplc_mbrtu_slave', `<value name="ID">${num(2)}</value><field name="BAUD">115200</field>`)) +
            every(1, nx(regSet(1, r('jwplc_timer_seconds')),
                nx(write(1, join(lit('R2 = '), regGet(2))),
                    `<block type="control_if_else">
                       <value name="CONDITION"><block type="operator_equals"><value name="OPERAND1">${txt('')}<block type="operator_mod"><value name="NUM1">${nslot(regGet(2))}</value><value name="NUM2">${num(2)}</value></block></value><value name="OPERAND2">${txt('1')}</value></block></value>
                       <statement name="SUBSTACK"><block type="${J}jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">HIGH</field></block></statement>
                       <statement name="SUBSTACK2"><block type="${J}jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">LOW</field></block></statement>
                     </block>`)), 250);
    }
    B.Xml.domToWorkspace(B.Xml.textToDom(`<xml xmlns="http://www.w3.org/1999/xhtml">${xml}</xml>`), ws);
    await new Promise((res) => setTimeout(res, 2500));
    if (!window.monaco) {
        const t = document.querySelector('button.tigger');
        if (t) t.click();
    }
    for (let i = 0; i < 30 && !window.monaco; i++) await new Promise((res) => setTimeout(res, 500));
    return 'VARIANT=' + variant + ' BLOCKS=' + ws.getAllBlocks().length + '\n' +
        window.monaco.editor.getModels().map((x) => x.getValue()).filter((x) => x.includes('JWPLC Basic')).join('\n=====\n');
})()

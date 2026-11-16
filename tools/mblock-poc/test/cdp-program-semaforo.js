// Ejemplo para presentaciones: semáforo con botón para peatones.
// - Verde (Q0.2) hasta que alguien pulse OK; amarillo (Q0.1) 2 s; rojo (Q0.0)
//   5 s con cuenta regresiva en pantalla y un pitido por segundo.
// Requiere el Device jwplc_basic (cdp-add-device.js). Uso: node cdp.cjs cdp-program-semaforo.js
(async () => {
    const A = __jwA;
    const b = A.blocklyCtx;
    const ws = b.workspace;
    const B = b.blockly;
    ws.clear();
    ws.createVariable('segundos', '', 'jwVarSeg');
    const J = 'jwplc_basic.';
    const num = (n) => `<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
    const txt = (s) => `<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
    const seg = '<block type="data_variable"><field name="VARIABLE" id="jwVarSeg" variabletype="">segundos</field></block>';
    // Encadena bloques: cada uno es un <block>…</block> sin <next>.
    const chain = (...blocks) => blocks.reduceRight((rest, blk) =>
        rest ? blk.replace(/<\/block>$/, `<next>${rest}</next></block>`) : blk, '');
    const out = (q, on) => `<block type="${J}jwplc_write_output"><field name="Q">${q}</field><field name="STATE">${on ? 'HIGH' : 'LOW'}</field></block>`;
    const write = (line, value) => `<block type="${J}jwplc_screen_write"><field name="LINE">${line}</field><value name="TEXT">${typeof value === 'string' && !value.startsWith('<') ? txt(value) : txt('') + value}</value></block>`;
    const note = (hz, s) => `<block type="${J}jwplc_play_note"><field name="NOTE">${hz}</field><value name="SECS">${num(s)}</value></block>`;
    const wait = (s) => `<block type="control_wait"><value name="DURATION">${num(s)}</value></block>`;
    const join = (a, v) => `<block type="operator_join"><value name="STRING1">${txt(a)}</value><value name="STRING2">${txt('')}${v}</value></block>`;

    const ciclo = chain(
        out('Q0_0', false),
        out('Q0_2', true),
        write(2, 'VERDE: pasan autos'),
        write(3, 'Pulsa OK para cruzar'),
        `<block type="control_wait_until"><value name="CONDITION"><block type="${J}jwplc_button_down"><field name="B">BTN_OK</field></block></value></block>`,
        note(523, 0.2),
        out('Q0_2', false),
        out('Q0_1', true),
        write(2, 'AMARILLO: cuidado'),
        write(3, ''),
        wait(2),
        out('Q0_1', false),
        out('Q0_0', true),
        write(2, 'ROJO: cruza peaton'),
        `<block type="data_setvariableto"><field name="VARIABLE" id="jwVarSeg" variabletype="">segundos</field><value name="VALUE">${txt('5')}</value></block>`,
        `<block type="control_repeat"><value name="TIMES">${num(5)}</value><statement name="SUBSTACK">${chain(
            write(3, join('Quedan ', seg)),
            note(880, 0.1),
            wait(0.9),
            `<block type="data_changevariableby"><field name="VARIABLE" id="jwVarSeg" variabletype="">segundos</field><value name="VALUE">${num(-1)}</value></block>`
        )}</statement></block>`
    );

    const xml = `<xml xmlns="http://www.w3.org/1999/xhtml">
 ${chain(
        `<block type="${J}jwplc_when_start" x="40" y="40"></block>`,
        `<block type="${J}jwplc_set_volume"><value name="PCT">${num(40)}</value></block>`,
        write(1, 'Semaforo JWPLC'),
        `<block type="control_forever"><statement name="SUBSTACK">${ciclo}</statement></block>`
    )}
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
    return 'BLOCKS=' + ws.getAllBlocks().length + ' sin código';
})()

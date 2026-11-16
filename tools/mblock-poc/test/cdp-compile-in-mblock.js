// Compila desde el renderer de mBlock por la misma ruta que el driver: token y
// librerías del sketch (arduino/ de la extensión instalada) vía MeApi + fetch al bridge.
// Uso: node cdp.cjs cdp-compile-in-mblock.js
(async () => {
    const code = window.monaco.editor.getModels().map((x) => x.getValue()).find((v) => v.includes('JWPLC Basic'));
    const fe = MeApi.fe;
    const join = fe.getPath().join;
    const home = await MeApi.sys.getPath('home');
    const token = String(await fe.read2Str(join(home, '.jwplc', 'mblock-bridge', 'token.txt'))).trim();
    const files = {};
    for (const name of ['JWPLC_mBlock.h', 'JWPLC_mBlock_WiFi.h', 'JWPLC_mBlock_RS485.h', 'JWPLC_mBlock_ModbusRTU.h',
        'JWPLC_mBlock_Ethernet.h', 'JWPLC_mBlock_ModbusTCP.h', 'JWPLC_mBlock_HMI.h']) {
        const p = join(fe.extDirPath, 'jwplc_basic', 'arduino', name);
        if (fe.exists(p)) files[name] = String(await fe.read2Str(p));
    }
    const t0 = Date.now();
    const res = await fetch('http://127.0.0.1:47820/compile', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-JWPLC-Token': token },
        body: JSON.stringify({ code, files }),
    });
    const txt = await res.text();
    let lines;
    try {
        lines = txt.trim().split('\n').map((l) => JSON.parse(l));
    } catch (e) {
        return 'HTTP=' + res.status + ' ' + txt;
    }
    return 'HTTP=' + res.status + ' files=' + Object.keys(files).join(',') + ' ms=' + (Date.now() - t0) + '\n' +
        lines.filter((l) => l.type !== 'log').map((l) => JSON.stringify(l)).join('\n') +
        '\nLAST_LOGS:\n' + lines.filter((l) => l.type === 'log').slice(-6).map((l) => l.line).join('\n');
})()

// JWPLC Basic para mBlock 5: runtime de upload.
//
// Este fragmento se inserta tal cual dentro del index.js de la extensión
// (ver build-extension.cjs). Corre en el renderer de mBlock, que NO tiene
// require/child_process (Electron 21 + contextIsolation, Gate 3A), así que
// la compilación y el upload se delegan al JWPLC mBlock Bridge por HTTP.

// La FQBN (package publicado o alpha en prueba) la decide el bridge.
// JWPLC_SKETCH_FILES (librerías de src/arduino) lo define build-extension.cjs.
const JWPLC_BRIDGE_URL = 'http://127.0.0.1:47820';

// mBlock pasa el puerto a los middlewares, pero no al driver. El middleware
// lo guarda aquí para el driver.
const jwplcUploadState = {
    port: null,
};

const jwplcCapturePortMiddleware = (app, device, code, params, log, unused, portAddress) => {
    // mBlock entrega la dirección como "serialport.COM14".
    const m = /COM\d+/i.exec(String(portAddress || ''));
    jwplcUploadState.port = m ? m[0].toUpperCase() : null;
    return code;
};

async function jwplcReadBridgeToken() {
    const me = window.MeApi;
    if (!me || !me.fe || !me.sys) throw new Error('MeApi no disponible');
    const home = await me.sys.getPath('home');
    const tokenFile = me.fe.getPath().join(home, '.jwplc', 'mblock-bridge', 'token.txt');
    if (!me.fe.exists(tokenFile)) throw new Error('no existe ' + tokenFile);
    return String(await me.fe.read2Str(tokenFile)).trim();
}

// Libera el puerto serie que mLink mantiene abierto mientras mBlock está
// "conectado"; si no, esptool no puede abrir el COM.
// - El puerto de app._mCtx.ConnectService se cierra con el evento "close"
//   ignorado (setIgnoreError), como hace el driver esptoolByExec de mBlock con
//   close(cb, true). Si se emite "disconnect", mBlock muestra "fallo al subir"
//   aunque la subida siga. _mCtx es interno de mBlock 5.4.3: revisar al
//   cambiar de versión.
// - enviar "close" por el canal mLink no sirve: mLink lo ignora si el socket
//   no es el que abrió el COM.
async function jwplcReleasePort(app, port) {
    const ctx = app && (app.mCtx || app._mCtx);
    const cs = ctx && ctx.ConnectService;
    if (!cs || !cs.ports) return 'sin ConnectService';
    const ids = [...cs.ports.keys()].filter((id) => String(id).toUpperCase().endsWith(port));
    for (const id of ids) {
        const p = cs.ports.get(id);
        try {
            if (p && typeof p.setIgnoreError === 'function') {
                p.setIgnoreError('close', true);
                await p.close();
                p.setIgnoreError('close', false);
                cs.ports.delete(id);
            } else {
                await cs.disconnect(id);
            }
        } catch (_) {}
    }
    await new Promise((r) => setTimeout(r, 1000));
    return ids.length ? 'cerrado ' + ids.join(',') : 'no estaba abierto';
}

const jwplcUploadDriver = (app, device, code, log, progress, onSuccess, onError) => {
    const say = (text) => { try { log('[JWPLC] ' + text); } catch (_) {} };
    let current = 0;
    let compiling = false;
    const pct = (n) => {
        current = Math.max(0, Math.min(100, Math.round(n)));
        try { progress(current); } catch (_) {}
    };
    // El cuadro de subida de mBlock da la subida por fallida si pasan 5 s sin
    // progreso (muestra "Fallo al subir" y "Intentar otra vez" aunque siga).
    // La compilación pasa decenas de segundos sin escribir nada: se reenvía el
    // progreso cada 2 s y avanza despacio mientras compila (hasta 48 %).
    const heartbeat = setInterval(() => {
        if (compiling && current < 48) current += 1;
        pct(current);
    }, 2000);

    (async () => {
        const port = jwplcUploadState.port;
        if (!port) throw new Error('mBlock no informó el puerto COM. Conecta la JWPLC Basic antes de subir.');

        let health;
        try {
            health = await (await fetch(JWPLC_BRIDGE_URL + '/health')).json();
        } catch (e) {
            throw new Error('JWPLC mBlock Bridge no responde en ' + JWPLC_BRIDGE_URL +
                '. Inícialo con tools/mblock-poc/bridge/start-bridge.cmd');
        }
        say('bridge ' + health.version + ' | ' + health.fqbn + ' | puerto ' + port);
        const token = await jwplcReadBridgeToken();

        say('puerto serie de mBlock: ' + await jwplcReleasePort(app, port));

        pct(2);
        const res = await fetch(JWPLC_BRIDGE_URL + '/upload', {
            method: 'POST',
            headers: { 'Content-Type': 'application/json', 'X-JWPLC-Token': token },
            body: JSON.stringify({ code, port, files: JWPLC_SKETCH_FILES }),
        });
        if (!res.ok || !res.body) {
            let detail = '';
            try { detail = (await res.json()).error; } catch (_) {}
            throw new Error('bridge HTTP ' + res.status + ' ' + detail);
        }

        // NDJSON: compilación ~0-50 %, upload 50-100 %.
        const reader = res.body.getReader();
        const decoder = new TextDecoder();
        let buffer = '';
        let result = null;
        for (;;) {
            const { value, done } = await reader.read();
            if (done) break;
            buffer += decoder.decode(value, { stream: true });
            const lines = buffer.split('\n');
            buffer = lines.pop();
            for (const line of lines) {
                if (!line.trim()) continue;
                const ev = JSON.parse(line);
                if (ev.type === 'log') {
                    say(ev.line);
                } else if (ev.type === 'stage') {
                    say('etapa: ' + ev.stage);
                    compiling = ev.stage === 'compile';
                    if (ev.stage === 'upload') pct(50);
                } else if (ev.type === 'progress') {
                    pct(50 + ev.percent / 2);
                } else if (ev.type === 'result') {
                    result = ev;
                }
            }
        }
        if (!result) throw new Error('el bridge cerró la conexión sin resultado');
        if (!result.ok) throw new Error('falló la etapa ' + result.stage + ' (exit ' + result.exit + ')');
        pct(100);
        say('upload completo en ' + Math.round(result.ms / 1000) + ' s');
    })().then(
        () => {
            clearInterval(heartbeat);
            onSuccess();
        },
        (err) => {
            clearInterval(heartbeat);
            say('ERROR: ' + (err && err.message ? err.message : err));
            onError(err && err.message ? err.message : String(err));
        }
    );
};

const ExtHandler = {
    onLoad(app, target) {},
    onUnload(app) {},
    onConnect(app, device) {},
    onDisconnect(app, device) {},
    onStopAll(app, device) {},
    beforeChangeUploadMode(app, device) { return true; },
    beforeChangeDebugMode(app, device) { return true; },
    afterChangeUploadMode(app, device) { return true; },
    afterChangeDebugMode(app, device) { return true; },
    onSelect(app, device) {},
    onUnselect(app, device) {},
    beforeCodeUpload(app, device) {},
    afterCodeUpload(app, device) {},
    onRead(app, device) {},
};

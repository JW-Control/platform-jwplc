#!/usr/bin/env node
/**
 * JWPLC mBlock Bridge
 *
 * Servicio local que compila y sube a la JWPLC Basic el código Arduino C
 * generado por mBlock 5.
 *
 * Motivo (Gate 3A): el renderer de mBlock 5.4.3 corre en Electron 21 con
 * contextIsolation activo, así que un upload middleware/driver de extensión
 * no tiene `require` ni `child_process`. Sí puede hacer `fetch` a
 * 127.0.0.1 (webSecurity=false), de modo que la extensión delega aquí la
 * compilación (arduino-cli + core JWPLC) y el upload (esptool del core).
 *
 * Seguridad:
 * - escucha sólo en 127.0.0.1;
 * - exige el token aleatorio que este proceso escribe en
 *   %USERPROFILE%\.jwplc\mblock-bridge\token.txt (la extensión lo lee con
 *   MeApi.fe; una página web no puede leerlo);
 * - rechaza peticiones con cabecera Origin (navegadores) salvo el propio
 *   servidor local de mBlock;
 * - el puerto de upload debe ser COMn.
 *
 * Sin dependencias: sólo Node >= 18.
 *
 * Uso:
 *   node jwplc-mblock-bridge.cjs            (puerto 47820)
 *   set JWPLC_BRIDGE_PORT=47821 & node ...
 *   set JWPLC_ARDUINO_CLI=C:\ruta\arduino-cli.exe & node ...
 *   set JWPLC_FQBN=jwplc:esp32:jwplcbasic & node ...
 */
'use strict';

const http = require('http');
const fs = require('fs');
const os = require('os');
const path = require('path');
const crypto = require('crypto');
const { spawn } = require('child_process');

const BRIDGE_VERSION = '0.1.0';
const PORT = Number(process.env.JWPLC_BRIDGE_PORT || 47820);
const HOST = '127.0.0.1';
const DEFAULT_FQBN = process.env.JWPLC_FQBN || 'jwplc:esp32:jwplcbasic';
const ALLOWED_FQBN = /^jwplc(_local)?:esp32:[a-z0-9_]+$/;
const ALLOWED_PORT = /^COM\d{1,3}$/i;
const MAX_CODE_BYTES = 512 * 1024;
const ALLOWED_ORIGINS = new Set(['http://127.0.0.1:3003', 'http://localhost:3003']);

// Fuera de %APPDATA%: si el bridge lo lanza una app empaquetada (MSIX), Windows
// virtualiza las escrituras nuevas en AppData y mBlock no vería el token.
const WORK_DIR = path.join(os.homedir(), '.jwplc', 'mblock-bridge');
const CONFIG_DIR = WORK_DIR;
const TOKEN_FILE = path.join(CONFIG_DIR, 'token.txt');
const SKETCH_NAME = 'jwplc_mblock';
const SKETCH_DIR = path.join(WORK_DIR, 'sketch', SKETCH_NAME);
const BUILD_DIR = path.join(WORK_DIR, 'build');
const OUTPUT_DIR = path.join(WORK_DIR, 'output');
const LOG_FILE = path.join(WORK_DIR, 'bridge.log');

function findArduinoCli() {
    const candidates = [
        process.env.JWPLC_ARDUINO_CLI,
        'C:\\Program Files\\Arduino CLI\\arduino-cli.exe',
        'C:\\Program Files\\Arduino IDE\\resources\\app\\lib\\backend\\resources\\arduino-cli.exe',
    ].filter(Boolean);
    for (const c of candidates) {
        if (fs.existsSync(c)) return c;
    }
    return 'arduino-cli';
}

const ARDUINO_CLI = findArduinoCli();

fs.mkdirSync(CONFIG_DIR, { recursive: true });
fs.mkdirSync(SKETCH_DIR, { recursive: true });
fs.mkdirSync(OUTPUT_DIR, { recursive: true });

const TOKEN = crypto.randomBytes(24).toString('hex');
fs.writeFileSync(TOKEN_FILE, TOKEN, 'utf8');

function logLine(text) {
    const line = `[${new Date().toISOString()}] ${text}`;
    console.log(line);
    try { fs.appendFileSync(LOG_FILE, line + os.EOL); } catch (_) {}
}

// Un solo trabajo a la vez: el puerto COM y el build path son únicos.
let busy = false;

function runCli(args, onLine) {
    return new Promise((resolve) => {
        logLine(`RUN ${ARDUINO_CLI} ${args.join(' ')}`);
        const child = spawn(ARDUINO_CLI, args, { windowsHide: true });
        let tail = '';
        const feed = (chunk) => {
            const text = tail + chunk.toString();
            const lines = text.split(/\r?\n/);
            tail = lines.pop();
            for (const l of lines) if (l.trim()) onLine(l);
        };
        child.stdout.on('data', feed);
        child.stderr.on('data', feed);
        child.on('error', (err) => {
            onLine('ERROR: ' + err.message);
            resolve(-1);
        });
        child.on('close', (code) => {
            if (tail.trim()) onLine(tail);
            resolve(code);
        });
    });
}

async function compile(code, fqbn, emit) {
    fs.writeFileSync(path.join(SKETCH_DIR, SKETCH_NAME + '.ino'), code, 'utf8');
    emit({ type: 'stage', stage: 'compile', fqbn });
    const exit = await runCli([
        'compile',
        '--fqbn', fqbn,
        '--build-path', BUILD_DIR,
        '--output-dir', OUTPUT_DIR,
        SKETCH_DIR,
    ], (line) => {
        if (/^(Library .* has been declared precompiled|Using precompiled library)/.test(line)) return;
        logLine('  ' + line);
        emit({ type: 'log', line });
    });
    return exit;
}

async function upload(port, fqbn, emit) {
    emit({ type: 'stage', stage: 'upload', port });
    let lastPct = -1;
    const exit = await runCli([
        'upload',
        '--fqbn', fqbn,
        '--port', port,
        '--input-dir', OUTPUT_DIR,
        SKETCH_DIR,
    ], (line) => {
        const m = /\((\d+)\s*%\)/.exec(line);
        if (m) {
            const pct = Number(m[1]);
            if (pct !== lastPct) {
                lastPct = pct;
                emit({ type: 'progress', percent: pct });
            }
            return;
        }
        logLine('  ' + line);
        emit({ type: 'log', line });
    });
    return exit;
}

function readBody(req) {
    return new Promise((resolve, reject) => {
        let size = 0;
        const chunks = [];
        req.on('data', (c) => {
            size += c.length;
            if (size > MAX_CODE_BYTES) {
                reject(new Error('payload too large'));
                req.destroy();
                return;
            }
            chunks.push(c);
        });
        req.on('end', () => resolve(Buffer.concat(chunks).toString('utf8')));
        req.on('error', reject);
    });
}

function sendJson(res, status, obj) {
    res.writeHead(status, { 'Content-Type': 'application/json; charset=utf-8' });
    res.end(JSON.stringify(obj));
}

function authorize(req) {
    const origin = req.headers.origin;
    if (origin && !ALLOWED_ORIGINS.has(origin)) return 'origin not allowed';
    if (req.headers['x-jwplc-token'] !== TOKEN) return 'invalid token';
    return null;
}

async function handleJob(req, res, withUpload) {
    const denied = authorize(req);
    if (denied) return sendJson(res, 403, { ok: false, error: denied });
    if (busy) return sendJson(res, 409, { ok: false, error: 'bridge busy' });

    let body;
    try {
        body = JSON.parse(await readBody(req));
    } catch (e) {
        return sendJson(res, 400, { ok: false, error: 'invalid JSON: ' + e.message });
    }
    const code = body && body.code;
    const fqbn = (body && body.fqbn) || DEFAULT_FQBN;
    const port = body && body.port;
    if (typeof code !== 'string' || !code.trim()) return sendJson(res, 400, { ok: false, error: 'missing code' });
    if (!ALLOWED_FQBN.test(fqbn)) return sendJson(res, 400, { ok: false, error: 'fqbn not allowed: ' + fqbn });
    if (withUpload && !(typeof port === 'string' && ALLOWED_PORT.test(port))) {
        return sendJson(res, 400, { ok: false, error: 'invalid port: ' + port });
    }

    busy = true;
    // Respuesta NDJSON en streaming: una línea JSON por evento.
    res.writeHead(200, { 'Content-Type': 'application/x-ndjson; charset=utf-8', 'Cache-Control': 'no-store' });
    const emit = (obj) => {
        try { res.write(JSON.stringify(obj) + '\n'); } catch (_) {}
        if (obj.type !== 'log' && obj.type !== 'progress') logLine(JSON.stringify(obj));
    };
    const started = Date.now();
    try {
        const compileExit = await compile(code, fqbn, emit);
        if (compileExit !== 0) {
            emit({ type: 'result', ok: false, stage: 'compile', exit: compileExit, ms: Date.now() - started });
            return;
        }
        if (!withUpload) {
            emit({ type: 'result', ok: true, stage: 'compile', exit: 0, ms: Date.now() - started });
            return;
        }
        const uploadExit = await upload(port.toUpperCase(), fqbn, emit);
        emit({ type: 'result', ok: uploadExit === 0, stage: 'upload', exit: uploadExit, ms: Date.now() - started });
    } catch (e) {
        emit({ type: 'result', ok: false, stage: 'internal', error: String(e && e.message || e) });
    } finally {
        busy = false;
        res.end();
    }
}

const server = http.createServer((req, res) => {
    const url = (req.url || '').split('?')[0];
    if (req.method === 'GET' && url === '/health') {
        // /health no ejecuta nada; sirve para que la extensión detecte el bridge.
        return sendJson(res, 200, {
            ok: true,
            bridge: 'jwplc-mblock-bridge',
            version: BRIDGE_VERSION,
            fqbn: DEFAULT_FQBN,
            cli: ARDUINO_CLI,
            tokenFile: TOKEN_FILE,
            busy,
        });
    }
    if (req.method === 'POST' && url === '/compile') return handleJob(req, res, false);
    if (req.method === 'POST' && url === '/upload') return handleJob(req, res, true);
    sendJson(res, 404, { ok: false, error: 'not found' });
});

server.on('error', (err) => {
    logLine('SERVER_ERROR ' + err.message);
    process.exit(1);
});

server.listen(PORT, HOST, () => {
    logLine(`JWPLC_MBLOCK_BRIDGE=LISTENING http://${HOST}:${PORT} version=${BRIDGE_VERSION}`);
    logLine(`ARDUINO_CLI=${ARDUINO_CLI}`);
    logLine(`FQBN=${DEFAULT_FQBN}`);
    logLine(`TOKEN_FILE=${TOKEN_FILE}`);
    logLine(`WORK_DIR=${WORK_DIR}`);
});

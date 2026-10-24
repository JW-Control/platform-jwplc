#!/usr/bin/env node
/**
 * Instala dist/jwplc_basic en mBlock 5 (Windows) sin pasar por la GUI:
 *
 * 1. copia la carpeta a %USERPROFILE%\mblock\exts\jwplc_basic;
 * 2. registra el Device en %USERPROFILE%\mblock\exts\device.json
 *    (lista local que mBlock lee al arrancar), con copia de seguridad.
 *
 * Cerrar mBlock antes (o recargar su ventana después) para que lo detecte.
 * Alternativa por GUI: arrastrar dist/jwplc_basic_v<ver>.mext a mBlock.
 *
 * Uso: node install-extension.cjs [--uninstall]
 */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');

const EXT_ID = 'jwplc_basic';
const SRC = path.join(__dirname, 'dist', EXT_ID);
const EXTS = path.join(os.homedir(), 'mblock', 'exts');
const DEST = path.join(EXTS, EXT_ID);
const DEVICE_JSON = path.join(EXTS, 'device.json');

function main() {
    const uninstall = process.argv.includes('--uninstall');
    if (!fs.existsSync(DEVICE_JSON)) throw new Error('no existe ' + DEVICE_JSON + ' (¿mBlock 5 instalado y abierto al menos una vez?)');

    const raw = fs.readFileSync(DEVICE_JSON, 'utf8');
    const backup = DEVICE_JSON + '.bak-' + new Date().toISOString().replace(/[:.]/g, '-');
    fs.writeFileSync(backup, raw);
    const devices = JSON.parse(raw).filter((d) => d.id !== EXT_ID);

    fs.rmSync(DEST, { recursive: true, force: true });
    if (!uninstall) {
        if (!fs.existsSync(path.join(SRC, 'manifest.json'))) throw new Error('falta ' + SRC + ': ejecuta antes build-extension.cjs');
        fs.cpSync(SRC, DEST, { recursive: true });
        const manifest = JSON.parse(fs.readFileSync(path.join(DEST, 'manifest.json'), 'utf8'));
        devices.push(Object.assign({}, manifest, { originVersion: manifest.version, updateStatus: 0 }));
    }
    fs.writeFileSync(DEVICE_JSON, JSON.stringify(devices));

    console.log('DEVICE_JSON_BACKUP=' + backup);
    console.log(uninstall ? 'UNINSTALLED=' + DEST : 'INSTALLED=' + DEST);
}

main();

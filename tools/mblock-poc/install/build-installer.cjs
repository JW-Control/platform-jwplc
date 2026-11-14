#!/usr/bin/env node
/**
 * Arma el paquete para otra máquina: install/dist/JWPLC-mBlock-<versión>.zip con
 * el instalador del bridge, el bridge, el .mext y los bloques sin empaquetar
 * (para Install-JWPLC-mBlock.cmd -Extension).
 *
 * Ejecutar antes extension/build-extension.cjs. Uso: node build-installer.cjs
 */
'use strict';

const fs = require('fs');
const path = require('path');
const { writeZip } = require('../extension/build-extension.cjs');

const ROOT = path.join(__dirname, '..');
const EXT_DIST = path.join(ROOT, 'extension', 'dist');
const manifest = JSON.parse(fs.readFileSync(path.join(EXT_DIST, 'jwplc_basic', 'manifest.json'), 'utf8'));
const NAME = 'JWPLC-mBlock-' + manifest.version;
const OUT = path.join(__dirname, 'dist', NAME);
const ZIP = OUT + '.zip';

fs.rmSync(OUT, { recursive: true, force: true });
fs.rmSync(ZIP, { force: true });
fs.mkdirSync(path.join(OUT, 'bridge'), { recursive: true });
fs.mkdirSync(path.join(OUT, 'extension', 'dist'), { recursive: true });

const copy = (from, to) => fs.cpSync(from, path.join(OUT, to), { recursive: true });
copy(path.join(__dirname, 'Install-JWPLC-mBlock.cmd'), 'Install-JWPLC-mBlock.cmd');
copy(path.join(__dirname, 'LEEME.txt'), 'LEEME.txt');
copy(path.join(ROOT, 'bridge', 'jwplc-mblock-bridge.cjs'), path.join('bridge', 'jwplc-mblock-bridge.cjs'));
copy(path.join(ROOT, 'extension', 'install-extension.cjs'), path.join('extension', 'install-extension.cjs'));
copy(path.join(EXT_DIST, 'jwplc_basic'), path.join('extension', 'dist', 'jwplc_basic'));
const mext = 'jwplc_basic_v' + manifest.version + '.mext';
copy(path.join(EXT_DIST, mext), mext);

// Windows PowerShell 5.1 lee un .ps1 sin BOM como ANSI: con BOM, las tildes salen bien.
const ps1 = fs.readFileSync(path.join(__dirname, 'Install-JWPLC-mBlock.ps1'), 'utf8').replace(/^﻿/, '');
fs.writeFileSync(path.join(OUT, 'Install-JWPLC-mBlock.ps1'), '﻿' + ps1.replace(/\r?\n/g, '\r\n'), 'utf8');
for (const f of ['Install-JWPLC-mBlock.cmd', 'LEEME.txt']) {
    const p = path.join(OUT, f);
    fs.writeFileSync(p, fs.readFileSync(p, 'utf8').replace(/\r?\n/g, '\r\n'));
}

// Rutas con '/' (Compress-Archive de PowerShell 5.1 escribe '\').
writeZip(OUT, ZIP);

console.log('INSTALLER_DIR=' + OUT);
console.log('INSTALLER_ZIP=' + ZIP);

#!/usr/bin/env node
/**
 * Instala para el JWPLC mBlock Bridge el package JWPLC de una rama/tag/commit
 * del repo, sin tocar el Arduino IDE del usuario:
 *
 *   %USERPROFILE%\.jwplc\mblock-bridge\sketchbook\hardware\jwplc_a14\esp32\   <- JWPLC/2.1.0 del ref
 *   %USERPROFILE%\.jwplc\mblock-bridge\bridge.json                           <- fqbn + sketchbook
 *
 * El bridge pasa ese sketchbook a arduino-cli (ARDUINO_DIRECTORIES_USER); las
 * toolchains se siguen resolviendo desde los packages ya instalados
 * (Arduino15), igual que el método hardware/jwplc_a14 del traspaso alpha.14.
 * Útil mientras una alpha no esté publicada en el índice.
 *
 * Uso (desde el repo):
 *   node install-package-from-ref.cjs [ref] [vendor]
 *   ref    por defecto origin/v2.1.0-alpha.14/HmiDesigner/main
 *   vendor por defecto jwplc_a14
 *   node install-package-from-ref.cjs --reset   (vuelve al core instalado jwplc:esp32,
 *                                                con un sketchbook vacío propio)
 */
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const { execFileSync } = require('child_process');

const WORK_DIR = path.join(os.homedir(), '.jwplc', 'mblock-bridge');
const CONFIG_FILE = path.join(WORK_DIR, 'bridge.json');
const SKETCHBOOK = path.join(WORK_DIR, 'sketchbook');

function main() {
    if (process.argv.includes('--reset')) {
        // Core publicado e instalado (jwplc:esp32), con un sketchbook vacío propio:
        // así ninguna librería del sketchbook del usuario tapa las del package.
        const empty = path.join(WORK_DIR, 'sketchbook-empty');
        fs.mkdirSync(empty, { recursive: true });
        const config = { fqbn: 'jwplc:esp32:jwplcbasic', sketchbook: empty, packageRef: 'core instalado', packageSha: '' };
        fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));
        console.log('BRIDGE_CONFIG=' + CONFIG_FILE);
        console.log('FQBN=' + config.fqbn + ' (core publicado), SKETCHBOOK=' + empty);
        console.log('Reinicia el bridge para aplicar el cambio.');
        return;
    }
    const ref = process.argv[2] || 'origin/v2.1.0-alpha.14/HmiDesigner/main';
    const vendor = process.argv[3] || 'jwplc_a14';
    if (!/^jwplc[a-z0-9_]*$/.test(vendor)) throw new Error('vendor no válido: ' + vendor);

    const repo = execFileSync('git', ['rev-parse', '--show-toplevel'], { cwd: __dirname }).toString().trim();
    const sha = execFileSync('git', ['rev-parse', ref], { cwd: repo }).toString().trim();
    const dest = path.join(SKETCHBOOK, 'hardware', vendor, 'esp32');
    const staging = path.join(WORK_DIR, 'staging-package');

    fs.rmSync(staging, { recursive: true, force: true });
    fs.mkdirSync(staging, { recursive: true });
    const tarFile = path.join(staging, 'package.tar');
    execFileSync('git', ['archive', '--format=tar', '-o', tarFile, sha, 'JWPLC/2.1.0'], { cwd: repo });
    // Rutas relativas: el tar de Git Bash toma "C:" como host remoto.
    execFileSync('tar', ['-xf', path.basename(tarFile)], { cwd: staging });

    fs.rmSync(dest, { recursive: true, force: true });
    fs.mkdirSync(path.dirname(dest), { recursive: true });
    fs.renameSync(path.join(staging, 'JWPLC', '2.1.0'), dest);
    fs.rmSync(staging, { recursive: true, force: true });
    fs.writeFileSync(path.join(dest, 'PACKAGE_SOURCE.txt'), `ref=${ref}\nsha=${sha}\ninstalled=${new Date().toISOString()}\n`);

    const config = { fqbn: `${vendor}:esp32:jwplcbasic`, sketchbook: SKETCHBOOK, packageRef: ref, packageSha: sha };
    fs.writeFileSync(CONFIG_FILE, JSON.stringify(config, null, 2));

    console.log('PACKAGE_REF=' + ref);
    console.log('PACKAGE_SHA=' + sha);
    console.log('PACKAGE_DIR=' + dest);
    console.log('BRIDGE_CONFIG=' + CONFIG_FILE);
    console.log('FQBN=' + config.fqbn);
    console.log('Reinicia el bridge para aplicar el cambio.');
}

main();

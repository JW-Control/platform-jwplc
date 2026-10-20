'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log('=== live-bridge-flasher.test.cjs — Verificación del Flasheador Live Bridge ===\n');

const ROOT = path.resolve(__dirname, '..');
const POC_DIR = path.join(ROOT, 'poc');
const FW_DIR = path.join(POC_DIR, 'firmware');

let passes = 0;
let fails = 0;

function test(name, fn) {
  try {
    fn();
    console.log(`  PASS ${name}`);
    passes++;
  } catch (err) {
    console.error(`  FAIL ${name}`);
    console.error(`       ${err.message}`);
    fails++;
  }
}

async function testAsync(name, fn) {
  try {
    await fn();
    console.log(`  PASS ${name}`);
    passes++;
  } catch (err) {
    console.error(`  FAIL ${name}`);
    console.error(`       ${err.message}`);
    fails++;
  }
}

async function run() {
  console.log('FASE 2: Empaquetado y Artefactos de Firmware');

  test('manifest.json existe y tiene la estructura correcta', () => {
    const manifestPath = path.join(FW_DIR, 'manifest.json');
    assert(fs.existsSync(manifestPath), 'manifest.json no existe');
    const manifest = JSON.parse(fs.readFileSync(manifestPath, 'utf8'));
    assert.strictEqual(manifest.target, 'esp32');
    assert.strictEqual(manifest.baudRate, 921600);
    assert.strictEqual(manifest.romBaudRate, 115200);
    assert.strictEqual(manifest.parts.length, 3);

    const names = manifest.parts.map(p => p.name);
    assert.deepStrictEqual(names, ['bootloader', 'partitions', 'app']);

    const addresses = manifest.parts.map(p => p.address);
    assert.deepStrictEqual(addresses, [0x1000, 0x8000, 0x10000]);
  });

  test('Binarios de firmware existen y tienen encabezados ESP32 válidos', () => {
    const bootloader = fs.readFileSync(path.join(FW_DIR, 'bootloader.bin'));
    const partitions = fs.readFileSync(path.join(FW_DIR, 'partitions.bin'));
    const firmware = fs.readFileSync(path.join(FW_DIR, 'firmware.bin'));
    const merged = fs.readFileSync(path.join(FW_DIR, 'firmware.merged.bin'));

    // Bootloader ESP32 empieza con magic byte 0xE9
    assert.strictEqual(bootloader[0], 0xE9, 'Bootloader magic byte debe ser 0xE9');
    assert(bootloader.length > 10000, 'Bootloader tamaño insuficiente');

    // Partition table ESP32 empieza con magic 0xAA 0x50
    assert.strictEqual(partitions[0], 0xAA, 'Partitions byte 0 debe ser 0xAA');
    assert.strictEqual(partitions[1], 0x50, 'Partitions byte 1 debe ser 0x50');

    // App firmware empieza con magic byte 0xE9
    assert.strictEqual(firmware[0], 0xE9, 'App firmware magic byte debe ser 0xE9');
    assert(firmware.length > 300000, 'App firmware tamaño insuficiente');

    // Merged flash image 4MB (4,194,304 bytes)
    assert.strictEqual(merged.length, 4194304, 'Merged image debe tener exactamente 4MB');
  });

  await testAsync('esptool-bundle.js se carga e implementa ESPLoader y Transport', async () => {
    const bundlePath = path.join(POC_DIR, 'esptool-bundle.js');
    assert(fs.existsSync(bundlePath), 'esptool-bundle.js no existe en poc/');

    const esptool = await import('file:///' + bundlePath.replace(/\\/g, '/'));
    assert(typeof esptool.ESPLoader === 'function', 'ESPLoader debe ser una clase/función');
    assert(typeof esptool.Transport === 'function', 'Transport debe ser una clase/función');

    // Verificar métodos clave de ESPLoader
    const proto = esptool.ESPLoader.prototype;
    assert(typeof proto.main === 'function', 'ESPLoader.main debe existir');
    assert(typeof proto.writeFlash === 'function', 'ESPLoader.writeFlash debe existir');
    assert(typeof proto.after === 'function', 'ESPLoader.after debe existir');
  });

  console.log('\nFASE 3: Motor de Flasheo y Módulo UI');

  test('designer-flasher.js expone API pública JWPLCHMIFlasher', () => {
    const flasherSource = fs.readFileSync(path.join(POC_DIR, 'designer-flasher.js'), 'utf8');
    assert(flasherSource.includes('window.JWPLCHMIFlasher'), 'Debe registrar window.JWPLCHMIFlasher');
    assert(flasherSource.includes('showPromptModal'), 'Debe incluir showPromptModal');
    assert(flasherSource.includes('runFlashWithModal'), 'Debe incluir runFlashWithModal');
    assert(flasherSource.includes('executeFlash'), 'Debe incluir executeFlash');
  });

  test('designer-live.js integra detección de timeout de 2.5s y flasheo en 1 clic', () => {
    const liveSource = fs.readFileSync(path.join(POC_DIR, 'designer-live.js'), 'utf8');
    assert(liveSource.includes('armNoBridgeTimer'), 'Debe incluir armNoBridgeTimer');
    assert(liveSource.includes('stopNoBridgeTimer'), 'Debe incluir stopNoBridgeTimer');
    assert(liveSource.includes('triggerFlashSequence'), 'Debe incluir triggerFlashSequence');
    assert(liveSource.includes('reconnectLive'), 'Debe incluir reconnectLive');
    assert(liveSource.includes('diagFlashButton'), 'Debe incluir botón manual de flasheo en panel de diagnóstico');
    assert(liveSource.includes('2500'), 'Timeout de 2.5s para no-bridge');
  });

  console.log('\nFASE 4: Autoconexión y Cadena de Carga');

  test('ux-foundation.js y service-worker.js integran designer-flasher y esptool-bundle', () => {
    const uxSource = fs.readFileSync(path.join(POC_DIR, 'ux-foundation.js'), 'utf8');
    assert(uxSource.includes('designer-flasher.js'), 'ux-foundation.js debe cargar designer-flasher.js antes de designer-live.js');

    const swSource = fs.readFileSync(path.join(POC_DIR, 'service-worker.js'), 'utf8');
    assert(swSource.includes('designer-flasher.js'), 'service-worker debe cachear designer-flasher.js');
    assert(swSource.includes('esptool-bundle.js'), 'service-worker debe cachear esptool-bundle.js');
    assert(swSource.includes('firmware/manifest.json'), 'service-worker debe cachear manifest.json');
  });

  test('Mapeo de direcciones flash (0x1000 bootloader, 0x8000 partitions, 0x10000 app)', () => {
    const flasherSource = fs.readFileSync(path.join(POC_DIR, 'designer-flasher.js'), 'utf8');
    assert(flasherSource.includes('0x1000'), 'Dirección bootloader 0x1000 requerida');
    assert(flasherSource.includes('0x8000'), 'Dirección particiones 0x8000 requerida');
    assert(flasherSource.includes('0x10000'), 'Dirección firmware app 0x10000 requerida');
  });

  console.log(`\nResultado: ${passes} pruebas pasadas, ${fails} falladas.`);
  if (fails > 0) process.exit(1);
}

run();

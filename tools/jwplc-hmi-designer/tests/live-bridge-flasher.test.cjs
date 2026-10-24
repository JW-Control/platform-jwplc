'use strict';

const fs = require('fs');
const path = require('path');
const assert = require('assert');

console.log('=== live-bridge-flasher.test.cjs — Verificación del Flasheador Live Bridge ===\n');

const ROOT = path.resolve(__dirname, '..');
const POC_DIR = path.join(ROOT, 'poc');
// Formato de exportación del package JWPLC («Exportar binario compilado»).
const FW_DIR = path.join(POC_DIR, 'firmware', 'jwplc.esp32.jwplcbasic');
const PKG = path.resolve(ROOT, '..', '..', 'JWPLC', '2.1.0');
const APP = 'JWPLC_HMI_Live_Bridge.ino';

// designer-flasher.js real en un contexto mínimo (sin DOM) para usar su parser.
function loadFlasher(globals = {}) {
  const vm = require('vm');
  const el = () => ({ style: {}, appendChild() {}, setAttribute() {}, addEventListener() {}, querySelector: () => null });
  const document = { baseURI: 'http://127.0.0.1/', createElement: el, head: el(), body: el(), getElementById: () => null, querySelector: () => null, addEventListener() {} };
  const ctx = { window: {}, document, navigator: {}, console, URL, setTimeout, clearTimeout };
  ctx.window.document = document;
  Object.assign(ctx, globals);
  Object.assign(ctx.window, globals);
  vm.createContext(ctx);
  vm.runInContext(fs.readFileSync(path.join(POC_DIR, 'designer-flasher.js'), 'utf8'), ctx);
  return ctx.window.JWPLCHMIFlasher;
}

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

  test('flash_args del package: mismas partes y direcciones que la subida de Arduino', () => {
    const flasher = loadFlasher();
    assert.strictEqual(flasher.FIRMWARE_DIR, './firmware/jwplc.esp32.jwplcbasic/');
    const args = flasher.parseFlashArgs(fs.readFileSync(path.join(FW_DIR, 'flash_args'), 'utf8'));
    assert.deepStrictEqual(JSON.parse(JSON.stringify(args.parts.map((p) => [p.name, p.address, p.file]))), [
      ['bootloader', 0x1000, `${APP}.bootloader.bin`],
      ['partitions', 0x8000, `${APP}.partitions.bin`],
      ['boot_app0', 0xe000, 'boot_app0.bin'],
      ['app', 0x10000, `${APP}.bin`]
    ]);
    assert.deepStrictEqual([args.flashMode, args.flashFreq, args.flashSize], ['dio', '40m', '4MB']);
    args.parts.forEach((p) => assert(fs.existsSync(path.join(FW_DIR, p.file)), `falta ${p.file}`));
    // El mismo orden y direcciones que la receta del package (platform.txt).
    const platform = fs.readFileSync(path.join(PKG, 'platform.txt'), 'utf8');
    assert(/0x8000 \{build\.project_name\}\.partitions\.bin.*0xe000 boot_app0\.bin.*0x10000 \{build\.project_name\}\.bin/.test(platform),
      'platform.txt ya no genera flash_args con 0x8000/0xe000/0x10000');
  });

  test('la tabla de particiones grabada es jwplc_max_app_4mb del package', () => {
    const csv = fs.readFileSync(path.join(PKG, 'tools', 'partitions', 'jwplc_max_app_4mb.csv'), 'utf8')
      .split(/\r?\n/).filter((l) => l.trim() && !l.startsWith('#'))
      .map((l) => l.split(',').map((c) => c.trim()).slice(0, 5).map((c) => (/^0x/i.test(c) ? parseInt(c, 16) : c)));
    const bin = fs.readFileSync(path.join(FW_DIR, `${APP}.partitions.bin`));
    const TYPE = { 0: 'app', 1: 'data' };
    const SUB = { '0:16': 'ota_0', '1:0': 'ota', '1:2': 'nvs', '1:3': 'coredump' };
    const rows = [];
    for (let o = 0; o < bin.length && bin.readUInt16LE(o) === 0x50AA; o += 32) {
      rows.push([bin.toString('latin1', o + 12, o + 28).replace(/\0+$/, ''), TYPE[bin[o + 2]], SUB[`${bin[o + 2]}:${bin[o + 3]}`], bin.readUInt32LE(o + 4), bin.readUInt32LE(o + 8)]);
    }
    assert.deepStrictEqual(rows, csv);
    assert.strictEqual(fs.readFileSync(path.join(PKG, 'boards.txt'), 'utf8').match(/^jwplcbasic\.build\.partitions=(.*)$/m)[1].trim(), 'jwplc_max_app_4mb');
  });

  test('el flasher graba con flash-mode/freq/size keep, como la subida del package', () => {
    const src = fs.readFileSync(path.join(POC_DIR, 'designer-flasher.js'), 'utf8');
    assert(/flashMode: 'keep',\s*flashFreq: 'keep',\s*flashSize: 'keep'/.test(src));
    assert(/--flash-mode keep --flash-freq keep --flash-size keep/.test(fs.readFileSync(path.join(PKG, 'platform.txt'), 'utf8')));
  });

  test('boot_app0.bin es otadata válida que arranca app0 (0xE000)', () => {
    const otadata = fs.readFileSync(path.join(FW_DIR, 'boot_app0.bin'));
    assert.strictEqual(otadata.length, 8192, 'otadata de 8 KB (dos sectores)');
    assert.strictEqual(otadata.readUInt32LE(0), 1, 'ota_seq = 1 -> app0');
    assert(otadata.subarray(4, 28).every((b) => b === 0xFF), 'label vacío');
  });

  test('Binarios de firmware existen y tienen encabezados ESP32 válidos', () => {
    const bootloader = fs.readFileSync(path.join(FW_DIR, `${APP}.bootloader.bin`));
    const partitions = fs.readFileSync(path.join(FW_DIR, `${APP}.partitions.bin`));
    const firmware = fs.readFileSync(path.join(FW_DIR, `${APP}.bin`));

    // Bootloader ESP32 empieza con magic byte 0xE9
    assert.strictEqual(bootloader[0], 0xE9, 'Bootloader magic byte debe ser 0xE9');
    assert(bootloader.length > 10000, 'Bootloader tamaño insuficiente');

    // Partition table ESP32 empieza con magic 0xAA 0x50
    assert.strictEqual(partitions[0], 0xAA, 'Partitions byte 0 debe ser 0xAA');
    assert.strictEqual(partitions[1], 0x50, 'Partitions byte 1 debe ser 0x50');

    // App firmware empieza con magic byte 0xE9
    assert.strictEqual(firmware[0], 0xE9, 'App firmware magic byte debe ser 0xE9');
    assert(firmware.length > 300000, 'App firmware tamaño insuficiente');

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

  await testAsync('si el flasheo falla, el puerto se cierra y el reintento puede volver a abrirlo', async () => {
    // Puerto Web Serial simulado: open() falla si ya está abierto, como el real.
    const port = { opened: false };
    let disconnects = 0;
    class Transport {
      constructor(p) { this.device = p; }
      async connect() {
        if (this.device.opened) throw new Error('The port is already open.');
        this.device.opened = true;
      }
      async disconnect() { disconnects++; this.device.opened = false; }
    }
    class ESPLoader {
      constructor(opts) { this.transport = opts.transport; }
      async main() {
        await this.transport.connect();
        throw new Error('Failed to connect with the device');
      }
    }
    const fetch = async (url) => {
      const file = path.join(FW_DIR, decodeURIComponent(new URL(url).pathname.split('/').pop()));
      const buf = fs.readFileSync(file);
      return { ok: true, status: 200, text: async () => buf.toString('utf8'), arrayBuffer: async () => buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length) };
    };
    const flasher = loadFlasher({ ESPLoader, ESPTransport: Transport, fetch });
    for (const intento of [1, 2]) {
      let message = '';
      try { await flasher.flash(port, {}); } catch (e) { message = e.message; }
      assert.strictEqual(message, 'Failed to connect with the device', `intento ${intento}: debe fallar por la sincronización, no por el puerto`);
      assert.strictEqual(port.opened, false, `intento ${intento}: el puerto debe quedar cerrado`);
    }
    assert.strictEqual(disconnects, 2);
  });

  await testAsync('tras grabar, reinicia en modo aplicación: IO0 libre (DTR=0) y pulso en EN (RTS 1 -> 0)', async () => {
    const signals = [];
    class Transport {
      constructor(p) { this.device = p; }
      async setDTR(v) { signals.push(`DTR=${v ? 1 : 0}`); }
      async setRTS(v) { signals.push(`RTS=${v ? 1 : 0}`); }
      async disconnect() { signals.push('close'); }
    }
    let written = null;
    class ESPLoader {
      async main() { return 'ESP32-D0WD-V3'; }
      async writeFlash(opts) { written = opts; }
      // esptool-js 0.7 sólo pone RTS=0 sin pulso previo: no debe usarse.
      async after() { signals.push('loader.after'); }
    }
    const fetch = async (url) => {
      const buf = fs.readFileSync(path.join(FW_DIR, decodeURIComponent(new URL(url).pathname.split('/').pop())));
      return { ok: true, status: 200, text: async () => buf.toString('utf8'), arrayBuffer: async () => buf.buffer.slice(buf.byteOffset, buf.byteOffset + buf.length) };
    };
    const flasher = loadFlasher({ ESPLoader, ESPTransport: Transport, fetch, performance });
    const result = await flasher.flash({}, {});
    assert.strictEqual(result.ok, true);
    assert.strictEqual(written.fileArray.length, 4);
    assert.deepStrictEqual(signals, ['DTR=0', 'RTS=1', 'RTS=0', 'close']);
  });

  test('el botón LIVE se desactiva mientras se graba', () => {
    const flasherSource = fs.readFileSync(path.join(POC_DIR, 'designer-flasher.js'), 'utf8');
    const liveSource = fs.readFileSync(path.join(POC_DIR, 'designer-live.js'), 'utf8');
    assert(/callbacks\.onFlashStart\?\.\(\)/.test(flasherSource) && /finally\s*\{\s*callbacks\.onFlashEnd\?\.\(\)/.test(flasherSource), 'runFlashWithModal debe avisar inicio y fin');
    assert(/onFlashStart:\s*\(\)\s*=>\s*\{\s*liveButton\.disabled = true/.test(liveSource), 'designer-live.js debe desactivar el botón');
    assert(/onFlashEnd:\s*\(\)\s*=>\s*\{\s*liveButton\.disabled = false/.test(liveSource), 'designer-live.js debe reactivar el botón');
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
    assert(swSource.includes('firmware/jwplc.esp32.jwplcbasic/flash_args'), 'service-worker debe cachear flash_args');
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

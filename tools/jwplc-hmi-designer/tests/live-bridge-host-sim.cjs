#!/usr/bin/env node
// Live Bridge en host: compila el JWPLC_HMI_Live_Bridge.ino real con g++ contra
// stubs (Serial en memoria, TFT en framebuffer) y le pasa los paquetes que
// genera el codificador real de poc/designer-live.js (Chromium). Verifica que
// la TFT simulada queda idéntica al lienzo del Designer en los protocolos
// v1 (JWH1/JWH2) y v2 (JWH3/JWH4), que cada imagen se dibuja con pushImage y
// compara el tamaño de los paquetes.
//
// Uso: node tools/jwplc-hmi-designer/tests/live-bridge-host-sim.cjs
// Requiere g++ (C++17) y Playwright.
'use strict';

const fs = require('fs');
const os = require('os');
const path = require('path');
const http = require('http');
const { execFileSync } = require('child_process');

const ROOT = path.resolve(__dirname, '..');
const POC = path.join(ROOT, 'poc');
const INO = path.join(ROOT, 'firmware', 'JWPLC_HMI_Live_Bridge', 'JWPLC_HMI_Live_Bridge.ino');
const WORK = fs.mkdtempSync(path.join(os.tmpdir(), 'jwplc-live-sim-'));
const W = 320;
const H = 170;

function loadPlaywright() {
  try { return require('playwright'); } catch (_) { /* global */ }
  const globalRoot = execFileSync(process.platform === 'win32' ? 'npm.cmd' : 'npm', ['root', '-g'], { shell: process.platform === 'win32' }).toString().trim();
  return require(path.join(globalRoot, 'playwright'));
}

function chromiumPath() {
  const base = process.env.PLAYWRIGHT_BROWSERS_PATH || '/opt/pw-browsers';
  const dir = fs.existsSync(base) ? fs.readdirSync(base).find((d) => /^chromium-\d+$/.test(d)) : null;
  const exe = dir ? path.join(base, dir, 'chrome-linux', 'chrome') : null;
  return exe && fs.existsSync(exe) ? exe : undefined;
}

const MIME = { '.html': 'text/html', '.js': 'text/javascript', '.css': 'text/css', '.svg': 'image/svg+xml', '.png': 'image/png', '.json': 'application/json', '.webmanifest': 'application/manifest+json' };

function serve() {
  const server = http.createServer((req, res) => {
    const rel = decodeURIComponent(new URL(req.url, 'http://x').pathname).replace(/^\/+/, '') || 'index.html';
    const file = path.join(POC, rel);
    if (!file.startsWith(POC) || !fs.existsSync(file) || fs.statSync(file).isDirectory()) {
      res.writeHead(404); res.end(); return;
    }
    res.writeHead(200, { 'Content-Type': MIME[path.extname(file)] || 'application/octet-stream' });
    fs.createReadStream(file).pipe(res);
  });
  return new Promise((resolve) => server.listen(0, '127.0.0.1', () => resolve(server)));
}

const failures = [];
function check(condition, message) {
  if (condition) console.log(`  PASS ${message}`);
  else { console.log(`  FAIL ${message}`); failures.push(message); }
}

// Stubs mínimos de lo que usa el bridge. pushImage copia al framebuffer;
// drawPixel/drawFastHLine también, pero se cuentan aparte (deben quedar en 0).
const STUB_ARDUINO = `#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <vector>
#include <string>
inline uint32_t g_simUs = 0;
inline uint32_t millis() { return g_simUs / 1000; }
inline uint32_t micros() { return g_simUs; }
inline void delay(uint32_t ms) { g_simUs += ms * 1000 + 1; }
struct SimSerial {
  std::vector<uint8_t> in; size_t pos = 0; std::string out;
  void setRxBufferSize(size_t) {}
  void begin(uint32_t) {}
  int available() { g_simUs += 1; return (int)(in.size() - pos); }
  int read() { return pos < in.size() ? in[pos++] : -1; }
  size_t read(uint8_t *dst, size_t n) { size_t k = 0; while (k < n && pos < in.size()) dst[k++] = in[pos++]; return k; }
  template <typename T> void print(T v) { out += std::to_string(v); }
  void print(const char *v) { out += v; }
  template <typename T> void println(T v) { print(v); out += "\\n"; }
  void println(const char *v) { out += v; out += "\\n"; }
};
inline SimSerial Serial;
struct SimEsp { uint32_t getFreeHeap() { return 200000; } uint32_t getMinFreeHeap() { return 190000; } uint32_t getMaxAllocHeap() { return 110000; } };
inline SimEsp ESP;
`;

const STUB_DISPLAY = `#pragma once
#include "Arduino.h"
struct JWPLC_IOState; struct JWPLC_RTCState;
inline uint16_t g_tft[320 * 170];
inline uint32_t g_pushImageCalls = 0, g_slowCalls = 0;
struct JWPLC_TFTClass {
  bool pushImage(int16_t x, int16_t y, int16_t w, int16_t h, const uint16_t *p, uint32_t = 50) {
    ++g_pushImageCalls;
    for (int r = 0; r < h; ++r) memcpy(&g_tft[(y + r) * 320 + x], p + r * w, (size_t)w * 2);
    return true;
  }
  bool drawPixel(int16_t x, int16_t y, uint16_t c, uint32_t = 50) { ++g_slowCalls; g_tft[y * 320 + x] = c; return true; }
  bool drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t c, uint32_t = 50) { ++g_slowCalls; for (int i = 0; i < w; ++i) g_tft[y * 320 + x + i] = c; return true; }
  bool beginBatch(uint32_t = 50) { return true; }
  void endBatch() {}
};
enum IdleWakeMode : uint8_t { IDLE_WAKE_DISABLED = 2 };
enum IdleReturnMode : uint8_t { IDLE_RETURN_DISABLED = 2 };
struct JWPLC_DisplayClass {
  JWPLC_TFTClass t;
  JWPLC_TFTClass &tft() { return t; }
  void setIdleWakeMode(IdleWakeMode) {}
  void setIdleReturnMode(IdleReturnMode) {}
  void enterUserUI() {}
};
inline JWPLC_DisplayClass JWPLC_Display;
`;

// Lee paquetes de stdin, corre setup() + loop() y vuelca la TFT y la salida.
const STUB_MAIN = `#include "Arduino.h"
#include "JWPLC_Display.h"
void setup(); void loop();
int main(int argc, char **argv) {
  FILE *f = fopen(argv[1], "rb");
  uint8_t buf[65536]; size_t n;
  while ((n = fread(buf, 1, sizeof buf, f)) > 0) Serial.in.insert(Serial.in.end(), buf, buf + n);
  fclose(f);
  setup();
  while (Serial.pos < Serial.in.size()) loop();
  FILE *o = fopen(argv[2], "wb"); fwrite(g_tft, 2, 320 * 170, o); fclose(o);
  printf("%s", Serial.out.c_str());
  printf("PUSH_IMAGE=%u SLOW=%u\\n", g_pushImageCalls, g_slowCalls);
  return 0;
}
`;

function buildSim() {
  const stub = path.join(WORK, 'stub');
  fs.mkdirSync(stub, { recursive: true });
  fs.writeFileSync(path.join(stub, 'Arduino.h'), STUB_ARDUINO);
  fs.writeFileSync(path.join(stub, 'JWPLC_Display.h'), STUB_DISPLAY);
  fs.writeFileSync(path.join(stub, 'jwplc_spi_bus.h'), '#pragma once\n');
  fs.writeFileSync(path.join(WORK, 'main.cpp'), STUB_MAIN);
  fs.copyFileSync(INO, path.join(WORK, 'bridge.cpp'));
  const exe = path.join(WORK, 'bridge-sim');
  execFileSync('g++', ['-std=c++17', '-O1', '-I', stub, '-include', 'Arduino.h', path.join(WORK, 'bridge.cpp'), path.join(WORK, 'main.cpp'), '-o', exe], { stdio: 'inherit' });
  return exe;
}

function runSim(exe, packets) {
  const input = path.join(WORK, 'in.bin');
  const output = path.join(WORK, 'tft.bin');
  fs.writeFileSync(input, Buffer.concat(packets.map((p) => Buffer.from(p))));
  const log = execFileSync(exe, [input, output]).toString();
  const raw = fs.readFileSync(output);
  return { log, tft: new Uint16Array(raw.buffer, raw.byteOffset, W * H) };
}

function sameFrame(a, b) {
  for (let i = 0; i < W * H; i += 1) if (a[i] !== b[i]) return `difiere en x=${i % W} y=${Math.floor(i / W)}`;
  return '';
}

async function main() {
  try { execFileSync('g++', ['--version'], { stdio: 'ignore' }); } catch (_) { console.log('OMITIDO: falta g++'); return; }
  const exe = buildSim();
  const server = await serve();
  const { chromium } = loadPlaywright();
  const browser = await chromium.launch({ executablePath: chromiumPath() });
  const page = await browser.newPage({ viewport: { width: 1600, height: 1000 } });
  const errors = [];
  page.on('pageerror', (e) => errors.push(e.message));

  try {
    await page.goto(`http://127.0.0.1:${server.address().port}/desktop.html`);
    await page.waitForFunction(() => window.JWPLCHMILive?.encodePacket && window.JWPLCHMIEditor, null, { timeout: 20000 });
    await page.waitForTimeout(1500);

    // Diseño real del lienzo (texto, bordes, valores) y una edición posterior.
    const data = await page.evaluate(() => {
      const live = window.JWPLCHMILive;
      const ed = window.JWPLCHMIEditor;
      ed.addTextField(); ed.addValueField(); ed.addTextField();
      window.dispatchEvent(new Event('resize'));
      const a = live.captureFrame();
      // Segundo estado: ruido de 1 píxel en una franja (peor caso para RLE).
      const b = new Uint16Array(a);
      for (let y = 60; y < 100; y += 1) for (let x = 40; x < 200; x += 1) b[y * 320 + x] = ((x * 7) ^ (y * 13)) & 0xFFFF;
      const full = { x: 0, y: 0, width: 320, height: 170 };
      const region = { x: 40, y: 60, width: 160, height: 40 };
      const enc = (frame, rect, mixed) => Array.from(live.encodePacket(frame, rect, mixed));
      return {
        a: Array.from(a), b: Array.from(b),
        v1: [enc(a, full, false), enc(b, region, false)],
        v2: [enc(a, full, true), enc(b, region, true)]
      };
    });
    const a = Uint16Array.from(data.a);
    const b = Uint16Array.from(data.b);

    for (const [name, packets] of [['v1 (JWH1/JWH2)', data.v1], ['v2 (JWH3/JWH4)', data.v2]]) {
      console.log(`PROTOCOLO ${name}`);
      const onlyFull = runSim(exe, [packets[0]]);
      check(!sameFrame(onlyFull.tft, a), `FULL: la TFT simulada == lienzo del Designer ${sameFrame(onlyFull.tft, a)}`);
      const both = runSim(exe, packets);
      check(!sameFrame(both.tft, b), `FULL + REGION: la TFT simulada == lienzo editado ${sameFrame(both.tft, b)}`);
      const acks = (both.log.match(/^JWHMI_LIVE_FRAME \d+$/gm) || []).length;
      check(acks === 2 && !/JWHMI_LIVE_ERROR/.test(both.log), `2 ACK y sin errores (${both.log.trim().split('\n').join(' | ')})`);
      check(/PUSH_IMAGE=[1-9]\d* SLOW=0/.test(both.log), 'dibuja sólo con pushImage (sin drawPixel/drawFastHLine)');
      check(/^JWHMI_LIVE_READY 2$/m.test(both.log), 'READY anuncia la versión 2 del protocolo');
    }

    const size = (p) => p.length;
    const fullSaving = 1 - size(data.v2[0]) / size(data.v1[0]);
    const regionSaving = 1 - size(data.v2[1]) / size(data.v1[1]);
    console.log(`TAMAÑO FULL v1=${size(data.v1[0])} B v2=${size(data.v2[0])} B (${(fullSaving * 100).toFixed(0)}% menos)`);
    console.log(`TAMAÑO REGION ruidosa v1=${size(data.v1[1])} B v2=${size(data.v2[1])} B (${(regionSaving * 100).toFixed(0)}% menos)`);
    // v2 limita una repetición a 0x7FFF píxeles (v1 a 0xFFFF): a lo sumo un
    // ítem de 4 bytes más por cada 32767 píxeles iguales.
    check(size(data.v2[0]) <= size(data.v1[0]) + 8, 'v2 no pesa más que v1 en el FULL del lienzo');
    check(regionSaving > 0.4, 'v2 reduce más del 40% una región de píxeles sueltos');
    check(errors.length === 0, `sin errores JS (${errors.join(' | ') || 'ninguno'})`);
  } finally {
    await browser.close();
    server.close();
  }

  if (failures.length) { console.log(`${failures.length} verificaciones fallaron.`); process.exit(1); }
  console.log('Todas las verificaciones pasaron.');
}

main().catch((e) => { console.error(e); process.exit(1); });

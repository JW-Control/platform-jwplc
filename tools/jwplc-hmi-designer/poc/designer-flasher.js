(() => {
  'use strict';

  // =========================================================================
  // JWPLC HMI Designer — Motor y UX de Flasheo Web / Electron (Live Bridge)
  // Utiliza esptool-js sobre Web Serial API para instalar en 1 clic el firmware
  // JWPLC_HMI_Live_Bridge sin necesidad de Arduino IDE ni Python.
  // =========================================================================

  let esptoolModule = null;
  let cachedFirmware = null;
  let activeModal = null;

  function resolveAssetUrl(relativePath) {
    const base = document.baseURI || window.location.href;
    return new URL(relativePath, base).href;
  }

  async function getEsptool() {
    if (esptoolModule) return esptoolModule;
    if (window.ESPLoader && window.ESPTransport) {
      esptoolModule = {
        ESPLoader: window.ESPLoader,
        Transport: window.ESPTransport,
        ClassicReset: window.ESPClassicReset,
        HardReset: window.ESPHardReset
      };
      return esptoolModule;
    }
    const bundleUrl = resolveAssetUrl('./esptool-bundle.js');
    try {
      const mod = await import(bundleUrl);
      esptoolModule = mod;
      return mod;
    } catch (error) {
      console.error('[JWPLC FLASHER] Error importando esptool-bundle.js', error);
      throw new Error(`No se pudo cargar el motor esptool-js: ${error?.message || error}`);
    }
  }

  // Firmware en el formato de exportación del package JWPLC («Exportar binario
  // compilado»): firmware/jwplc.esp32.jwplcbasic/ con los nombres que deja el
  // package y su flash_args (platform.txt, recipe.hooks.objcopy.postobjcopy.4).
  // flash_args es el manifiesto: las mismas partes y direcciones que usa la
  // subida de Arduino. No hay manifiesto escrito a mano.
  const FIRMWARE_DIR = './firmware/jwplc.esp32.jwplcbasic/';
  // Igual que jwplcbasic.upload.speed y el ROM del ESP32.
  const UPLOAD_BAUD = 921600;
  const ROM_BAUD = 115200;
  // Respaldo si flash_args no se puede leer (mismo contenido que exporta el package).
  const FLASH_ARGS_FALLBACK = [
    '--flash-mode dio --flash-freq 40m --flash-size 4MB',
    '0x1000 JWPLC_HMI_Live_Bridge.ino.bootloader.bin',
    '0x8000 JWPLC_HMI_Live_Bridge.ino.partitions.bin',
    '0xe000 boot_app0.bin',
    '0x10000 JWPLC_HMI_Live_Bridge.ino.bin'
  ].join('\n');

  function partName(file) {
    if (/\.bootloader\.bin$/i.test(file)) return 'bootloader';
    if (/\.partitions\.bin$/i.test(file)) return 'partitions';
    if (/^boot_app0\.bin$/i.test(file)) return 'boot_app0';
    return 'app';
  }

  // "--flash-mode dio --flash-freq 40m --flash-size 4MB" + líneas "0xADDR archivo".
  function parseFlashArgs(text) {
    const options = {};
    const parts = [];
    String(text || '').split(/\r?\n/).map((line) => line.trim()).filter(Boolean).forEach((line) => {
      if (line.startsWith('--')) {
        const re = /--(flash-mode|flash-freq|flash-size)\s+(\S+)/g;
        let m;
        while ((m = re.exec(line))) options[m[1]] = m[2];
        return;
      }
      const m = /^(0x[0-9a-f]+)\s+(\S+)$/i.exec(line);
      if (m) parts.push({ address: parseInt(m[1], 16), file: m[2], name: partName(m[2]) });
    });
    if (!parts.length) throw new Error('flash_args no contiene partes para grabar.');
    return {
      flashMode: options['flash-mode'] || null,
      flashFreq: options['flash-freq'] || null,
      flashSize: options['flash-size'] || null,
      parts
    };
  }

  async function loadFirmwareBinaries(onLog) {
    if (cachedFirmware) return cachedFirmware;

    onLog?.('Leyendo flash_args del Live Bridge (formato de exportación del package)...');
    let args;
    try {
      const resp = await fetch(resolveAssetUrl(`${FIRMWARE_DIR}flash_args`), { cache: 'no-store' });
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      args = parseFlashArgs(await resp.text());
    } catch (e) {
      onLog?.(`Aviso: flash_args no cargó (${e.message}); se usan las partes por defecto del package.`);
      args = parseFlashArgs(FLASH_ARGS_FALLBACK);
    }

    const loadedParts = [];
    for (const part of args.parts) {
      const fileUrl = resolveAssetUrl(FIRMWARE_DIR + part.file);
      onLog?.(`Cargando ${part.name} @0x${part.address.toString(16)} (${part.file})...`);
      const fileResp = await fetch(fileUrl, { cache: 'no-store' });
      if (!fileResp.ok) throw new Error(`No se pudo descargar ${part.file}: HTTP ${fileResp.status}`);
      const arrayBuffer = await fileResp.arrayBuffer();
      loadedParts.push({
        name: part.name,
        address: part.address,
        data: new Uint8Array(arrayBuffer),
        desc: `${part.file} @0x${part.address.toString(16)}`
      });
    }

    cachedFirmware = {
      manifest: {
        target: 'esp32',
        baudRate: UPLOAD_BAUD,
        romBaudRate: ROM_BAUD,
        flashMode: args.flashMode,
        flashFreq: args.flashFreq,
        flashSize: args.flashSize
      },
      parts: loadedParts,
      totalBytes: loadedParts.reduce((acc, p) => acc + p.data.length, 0)
    };

    onLog?.(`Binarios listos: ${(cachedFirmware.totalBytes / 1024).toFixed(1)} KB en total (${args.flashMode}/${args.flashFreq}/${args.flashSize}).`);
    return cachedFirmware;
  }

  // Inyectar estilos para el modal de flasheo
  const styleEl = document.createElement('style');
  styleEl.id = 'jwplc-flasher-styles';
  styleEl.textContent = `
    .jwplc-flash-backdrop {
      position: fixed;
      inset: 0;
      background: rgba(4, 9, 14, 0.85);
      backdrop-filter: blur(5px);
      z-index: 99999;
      display: flex;
      align-items: center;
      justify-content: center;
      opacity: 0;
      transition: opacity 0.22s ease-out;
      font-family: Inter, ui-sans-serif, system-ui, -apple-system, sans-serif;
      color: #e5edf5;
    }
    .jwplc-flash-backdrop.visible {
      opacity: 1;
    }
    .jwplc-flash-modal {
      width: 500px;
      max-width: calc(100vw - 32px);
      background: #0d1720;
      border: 1px solid #20394c;
      border-radius: 12px;
      box-shadow: 0 24px 60px rgba(0, 0, 0, 0.75), inset 0 1px 0 rgba(255, 255, 255, 0.08);
      padding: 22px 26px;
      display: flex;
      flex-direction: column;
      gap: 16px;
      transform: scale(0.96);
      transition: transform 0.22s cubic-bezier(0.16, 1, 0.3, 1);
    }
    .jwplc-flash-backdrop.visible .jwplc-flash-modal {
      transform: scale(1);
    }
    .jwplc-flash-header {
      display: flex;
      align-items: flex-start;
      gap: 12px;
    }
    .jwplc-flash-icon {
      width: 40px;
      height: 40px;
      border-radius: 10px;
      background: linear-gradient(135deg, #ff9d3b 0%, #f0740c 100%);
      color: #081118;
      display: grid;
      place-items: center;
      font-size: 20px;
      font-weight: 900;
      flex-shrink: 0;
      box-shadow: 0 2px 10px rgba(255, 138, 42, 0.3);
    }
    .jwplc-flash-icon.success {
      background: linear-gradient(135deg, #43d17a 0%, #239952 100%);
      color: #ffffff;
      box-shadow: 0 2px 10px rgba(67, 209, 122, 0.3);
    }
    .jwplc-flash-icon.error {
      background: linear-gradient(135deg, #f45f65 0%, #b82d33 100%);
      color: #ffffff;
      box-shadow: 0 2px 10px rgba(244, 95, 101, 0.3);
    }
    .jwplc-flash-title-area {
      flex: 1;
      min-width: 0;
    }
    .jwplc-flash-badge {
      font: 700 9.5px/1 "Cascadia Code", Consolas, monospace;
      color: #ff9a43;
      background: rgba(255, 138, 42, 0.12);
      border: 1px solid rgba(255, 138, 42, 0.32);
      padding: 2.5px 6px;
      border-radius: 4px;
      letter-spacing: 0.4px;
      display: inline-block;
      margin-bottom: 5px;
    }
    .jwplc-flash-title {
      margin: 0;
      font-size: 16px;
      font-weight: 700;
      color: #eef5fb;
      line-height: 1.25;
    }
    .jwplc-flash-desc {
      margin: 0;
      font-size: 12.5px;
      line-height: 1.45;
      color: #9cb1c2;
    }
    .jwplc-flash-specs {
      background: #070e14;
      border: 1px solid #182a37;
      border-radius: 8px;
      padding: 10px 14px;
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      font-size: 11px;
    }
    .jwplc-flash-specs span {
      display: block;
      color: #6d8699;
      font-size: 10px;
    }
    .jwplc-flash-specs strong {
      display: block;
      color: #52c9ff;
      font-family: "Cascadia Code", Consolas, monospace;
      font-size: 11.5px;
      margin-top: 2px;
    }
    .jwplc-flash-progress-box {
      display: flex;
      flex-direction: column;
      gap: 6px;
    }
    .jwplc-flash-progress-track {
      height: 16px;
      background: #060e15;
      border: 1px solid #1f374a;
      border-radius: 8px;
      overflow: hidden;
      position: relative;
    }
    .jwplc-flash-progress-bar {
      height: 100%;
      width: 0%;
      background: linear-gradient(90deg, #ff8a2a 0%, #35d07f 100%);
      transition: width 0.14s ease-out;
      border-radius: 6px;
      box-shadow: 0 0 10px rgba(53, 208, 127, 0.4);
    }
    .jwplc-flash-progress-bar.success {
      background: #35d07f;
    }
    .jwplc-flash-progress-info {
      display: flex;
      justify-content: space-between;
      font-size: 11px;
      color: #8da2b3;
    }
    .jwplc-flash-progress-pct {
      font-weight: 700;
      color: #e5edf5;
      font-family: "Cascadia Code", Consolas, monospace;
    }
    .jwplc-flash-terminal {
      background: #04080d;
      border: 1px solid #14232e;
      border-radius: 6px;
      padding: 8px 10px;
      font: 11px/1.35 "Cascadia Code", Consolas, monospace;
      color: #7de3a7;
      max-height: 110px;
      overflow-y: auto;
      white-space: pre-wrap;
      margin: 0;
    }
    .jwplc-flash-actions {
      display: flex;
      align-items: center;
      justify-content: flex-end;
      gap: 10px;
      margin-top: 4px;
    }
    .jwplc-flash-btn {
      padding: 7px 15px;
      border-radius: 6px;
      font-size: 12.5px;
      font-weight: 600;
      cursor: pointer;
      display: inline-flex;
      align-items: center;
      gap: 6px;
      transition: all 0.15s ease;
    }
    .jwplc-flash-btn-primary {
      background: #ff8a2a;
      color: #081219;
      border: 1px solid #ffa354;
      box-shadow: 0 2px 10px rgba(255, 138, 42, 0.35);
    }
    .jwplc-flash-btn-primary:hover:not(:disabled) {
      background: #ffa149;
      transform: translateY(-1px);
    }
    .jwplc-flash-btn-secondary {
      background: #152430;
      color: #dfe8f0;
      border: 1px solid #2a4050;
    }
    .jwplc-flash-btn-secondary:hover:not(:disabled) {
      border-color: #486477;
      color: #ffffff;
    }
    .jwplc-flash-btn:disabled {
      opacity: 0.45;
      cursor: not-allowed;
      transform: none !important;
    }
  `;
  document.head.appendChild(styleEl);

  function createModalDOM() {
    if (activeModal?.backdrop) {
      activeModal.backdrop.remove();
      activeModal = null;
    }

    const backdrop = document.createElement('div');
    backdrop.className = 'jwplc-flash-backdrop';

    const modal = document.createElement('div');
    modal.className = 'jwplc-flash-modal';

    modal.innerHTML = `
      <div class="jwplc-flash-header">
        <div id="flashIcon" class="jwplc-flash-icon">⚡</div>
        <div class="jwplc-flash-title-area">
          <span class="jwplc-flash-badge">JWPLC LIVE BRIDGE · 1 CLIC</span>
          <h3 id="flashTitle" class="jwplc-flash-title">Live Preview en JWPLC</h3>
        </div>
      </div>
      <p id="flashDesc" class="jwplc-flash-desc"></p>
      <div id="flashSpecs" class="jwplc-flash-specs">
        <div><span>Microcontrolador</span><strong>ESP32 Basic</strong></div>
        <div><span>Velocidad Serie</span><strong>921,600 baud</strong></div>
        <div><span>Bus TFT SPI</span><strong>80 MHz Mutex</strong></div>
      </div>
      <div id="flashProgressBox" class="jwplc-flash-progress-box" hidden>
        <div class="jwplc-flash-progress-track">
          <div id="flashProgressBar" class="jwplc-flash-progress-bar"></div>
        </div>
        <div class="jwplc-flash-progress-info">
          <span id="flashProgressStatus">Iniciando...</span>
          <span id="flashProgressPct" class="jwplc-flash-progress-pct">0%</span>
        </div>
      </div>
      <pre id="flashTerminal" class="jwplc-flash-terminal" hidden></pre>
      <div id="flashActions" class="jwplc-flash-actions"></div>
    `;

    backdrop.appendChild(modal);
    document.body.appendChild(backdrop);

    // Fade-in
    requestAnimationFrame(() => backdrop.classList.add('visible'));

    const elements = {
      backdrop,
      modal,
      icon: modal.querySelector('#flashIcon'),
      title: modal.querySelector('#flashTitle'),
      desc: modal.querySelector('#flashDesc'),
      specs: modal.querySelector('#flashSpecs'),
      progressBox: modal.querySelector('#flashProgressBox'),
      progressBar: modal.querySelector('#flashProgressBar'),
      progressStatus: modal.querySelector('#flashProgressStatus'),
      progressPct: modal.querySelector('#flashProgressPct'),
      terminal: modal.querySelector('#flashTerminal'),
      actions: modal.querySelector('#flashActions')
    };

    activeModal = {
      elements,
      terminalLines: [],
      close() {
        backdrop.classList.remove('visible');
        setTimeout(() => {
          backdrop.remove();
          if (activeModal?.elements?.backdrop === backdrop) {
            activeModal = null;
          }
        }, 220);
      },
      log(text) {
        if (!elements.terminal) return;
        elements.terminal.hidden = false;
        this.terminalLines.push(text);
        if (this.terminalLines.length > 80) this.terminalLines.shift();
        elements.terminal.textContent = this.terminalLines.join('\n');
        elements.terminal.scrollTop = elements.terminal.scrollHeight;
      },
      setProgress(pct, statusText) {
        elements.progressBox.hidden = false;
        elements.progressBar.style.width = `${Math.max(0, Math.min(100, pct))}%`;
        elements.progressPct.textContent = `${Math.round(pct)}%`;
        if (statusText) elements.progressStatus.textContent = statusText;
      }
    };

    return activeModal;
  }

  // Reinicio del ESP32 en modo aplicación, como el hard_reset de esptool
  // (Python): IO0 libre (DTR=0) y un pulso en EN (RTS=1 -> 100 ms -> RTS=0).
  // No se usa loader.after('hard_reset') de esptool-js 0.7: sólo pone RTS=0
  // sin pulsarlo antes y, por un fallo de Transport.setSignals, vuelve a poner
  // DTR=1 (IO0 a tierra). El chip seguía en el stub del flasher: la app decía
  // «instalado» pero el PLC no arrancaba el Live Bridge.
  async function hardResetToApp(transport) {
    const wait = (ms) => new Promise((r) => setTimeout(r, ms));
    await transport.setDTR(false);
    await transport.setRTS(true);
    await wait(100);
    await transport.setRTS(false);
    await wait(50);
  }

  // =========================================================================
  // Flasher Core: ejecuta la secuencia esptool-js
  // =========================================================================
  async function executeFlash(port, options = {}) {
    const { onProgress, onLog } = options;
    const esptool = await getEsptool();
    const firmware = await loadFirmwareBinaries(onLog);

    onLog?.('Inicializando transporte WebSerial con ESP32...');
    const transport = new esptool.Transport(port, false);

    const terminal = {
      clean() {},
      writeLine(str) { onLog?.(str); },
      write(str) { onLog?.(str); }
    };

    const baudRate = firmware.manifest.baudRate || 921600;
    const romBaudRate = firmware.manifest.romBaudRate || 115200;

    const loader = new esptool.ESPLoader({
      transport,
      baudrate: baudRate,
      romBaudrate: romBaudRate,
      terminal,
      debugLogging: false
    });

    // Si algo falla antes del hard reset, se cierra el puerto: si no, el
    // reintento choca con «port already open» y el puerto queda tomado.
    try {
      return await writeWithLoader(loader, transport, firmware, onProgress, onLog);
    } catch (error) {
      try { await transport.disconnect(); } catch (_) {}
      throw error;
    }
  }

  async function writeWithLoader(loader, transport, firmware, onProgress, onLog) {
    const baudRate = firmware.manifest.baudRate || 921600;
    onProgress?.(5, 'Sincronizando con bootloader ROM...', 0);
    const chipDesc = await loader.main();
    onLog?.(`Microcontrolador detectado: ${chipDesc}`);
    onProgress?.(15, `ESP32 detectado. Flasher stub activo a ${baudRate.toLocaleString()} baud`, 0);

    // Preparar fileArray para writeFlash
    const fileArray = firmware.parts.map((part) => ({
      data: part.data,
      address: part.address
    }));

    const partSizes = firmware.parts.map((p) => p.data.length);
    const totalBytes = firmware.totalBytes;
    const partOffsets = [0];
    for (let i = 0; i < partSizes.length - 1; i += 1) {
      partOffsets.push(partOffsets[i] + partSizes[i]);
    }

    onProgress?.(20, 'Escribiendo firmware comprimido...', 0);

    const flashOptions = {
      fileArray,
      // Como la subida del package (--flash-mode keep --flash-freq keep
      // --flash-size keep): se graba el encabezado tal como lo generó el build.
      flashMode: 'keep',
      flashFreq: 'keep',
      flashSize: 'keep',
      eraseAll: false,
      compress: true,
      reportProgress: (fileIndex, written, total) => {
        const currentBytes = partOffsets[fileIndex] + written;
        const rawPct = (currentBytes / totalBytes) * 75; // 20% a 95%
        const overallPct = Math.min(95, Math.round(20 + rawPct));
        const currentPart = firmware.parts[fileIndex]?.name || `bloque ${fileIndex + 1}`;
        onProgress?.(
          overallPct,
          `Escribiendo ${currentPart}... (${(currentBytes / 1024).toFixed(0)} / ${(totalBytes / 1024).toFixed(0)} KB)`,
          fileIndex
        );
      }
    };

    const startTime = performance.now();
    await loader.writeFlash(flashOptions);
    const duration = ((performance.now() - startTime) / 1000).toFixed(1);

    onProgress?.(96, `Escritura completada en ${duration}s. Reiniciando microcontrolador...`, 3);
    onLog?.(`Flasheo completado con éxito (${duration}s). Ejecutando Hard Reset (RTS)...`);

    try {
      await hardResetToApp(transport);
    } catch (e) {
      onLog?.(`Aviso en hard_reset: ${e?.message || e}`);
    }

    try {
      await transport.disconnect();
    } catch (e) {
      onLog?.(`Aviso en disconnect: ${e?.message || e}`);
    }

    onProgress?.(100, 'Microcontrolador reiniciado con Live Bridge.', 3);
    return { ok: true, chip: chipDesc, durationSec: duration };
  }

  // =========================================================================
  // UX / UI: Modales interactivos (Fases 3 y 4)
  // =========================================================================

  function showPromptModal({ port, onInstall, onWait, onCancel }) {
    const modal = createModalDOM();
    const el = modal.elements;

    el.icon.textContent = '⚡';
    el.icon.className = 'jwplc-flash-icon';
    el.title.textContent = 'Dispositivo sin firmware Live Preview';
    el.desc.innerHTML = `
      El JWPLC conectado no respondió al handshake del Preview en Vivo.
      Es probable que tenga cargado un programa PLC estándar o esté en blanco.<br/><br/>
      <strong>Instalar el Live Bridge reemplaza el programa que tiene el PLC.</strong>
      Mientras tanto la máquina no ejecuta su lógica y las salidas quedan apagadas; para volver,
      sube de nuevo tu sketch desde Arduino IDE. Hazlo sólo con la máquina detenida y segura.<br/><br/>
      <strong>¿Instalar el firmware Live Bridge?</strong>
      El proceso toma unos segundos a 921,600 baudios y no requiere abrir Arduino IDE.
    `;
    el.specs.hidden = false;
    el.progressBox.hidden = true;
    el.terminal.hidden = true;

    el.actions.innerHTML = `
      <button id="btnCancel" type="button" class="jwplc-flash-btn jwplc-flash-btn-secondary">Cancelar</button>
      <button id="btnWait" type="button" class="jwplc-flash-btn jwplc-flash-btn-secondary">Seguir esperando</button>
      <button id="btnInstall" type="button" class="jwplc-flash-btn jwplc-flash-btn-primary">⚡ Instalar Firmware Live</button>
    `;

    el.actions.querySelector('#btnCancel')?.addEventListener('click', () => {
      modal.close();
      onCancel?.();
    });

    el.actions.querySelector('#btnWait')?.addEventListener('click', () => {
      modal.close();
      onWait?.();
    });

    el.actions.querySelector('#btnInstall')?.addEventListener('click', () => {
      onInstall?.();
    });

    return modal;
  }

  async function runFlashWithModal(port, callbacks = {}) {
    let modal = activeModal || createModalDOM();
    const el = modal.elements;

    el.icon.textContent = '⚡';
    el.icon.className = 'jwplc-flash-icon';
    el.title.textContent = 'Instalando Live Bridge en JWPLC...';
    el.desc.textContent = 'Por favor no desconectes el cable USB durante la transferencia.';
    el.specs.hidden = true;
    el.progressBox.hidden = false;
    el.terminal.hidden = false;
    el.actions.innerHTML = `
      <button id="btnAbort" type="button" class="jwplc-flash-btn jwplc-flash-btn-secondary" disabled>Flasheando…</button>
    `;

    modal.setProgress(2, 'Preparando transferencia...');
    modal.log('--- INICIO DE INSTALACIÓN LIVE BRIDGE ---');

    try {
      callbacks.onFlashStart?.();
      let result;
      try {
        result = await executeFlash(port, {
          onProgress: (pct, msg) => {
            modal.setProgress(pct, msg);
          },
          onLog: (text) => {
            modal.log(text);
          }
        });
      } finally {
        callbacks.onFlashEnd?.();
      }

      // Estado Éxito
      el.icon.textContent = '✓';
      el.icon.className = 'jwplc-flash-icon success';
      el.title.textContent = '¡Firmware instalado con éxito!';
      el.desc.textContent = `Instalación completada en ${result.durationSec}s. Conectando Live Preview en vivo...`;
      el.progressBar.classList.add('success');
      modal.setProgress(100, 'Listo. Reiniciando y reconectando...');

      // Pausa breve para permitir que el ESP32 complete su boot y render inicial
      await new Promise((r) => setTimeout(r, 800));
      modal.close();

      if (callbacks.onSuccess) {
        await callbacks.onSuccess(port);
      }
    } catch (error) {
      console.error('[JWPLC FLASHER] Error en flasheo:', error);
      modal.log(`ERROR: ${error?.message || error}`);

      el.icon.textContent = '!';
      el.icon.className = 'jwplc-flash-icon error';
      el.title.textContent = 'Error durante la instalación';
      el.desc.innerHTML = `
        No se pudo completar el flasheo (${error?.message || error}).<br/>
        <em>Sugerencia:</em> Si el microcontrolador no entra en modo bootloader automáticamente, mantén presionado el botón <strong>BOOT/IO0</strong> en el PLC mientras reconectas.
      `;
      el.actions.innerHTML = `
        <button id="btnErrorClose" type="button" class="jwplc-flash-btn jwplc-flash-btn-secondary">Cerrar</button>
        <button id="btnRetry" type="button" class="jwplc-flash-btn jwplc-flash-btn-primary">Reintentar</button>
      `;

      el.actions.querySelector('#btnErrorClose')?.addEventListener('click', () => {
        modal.close();
        callbacks.onCancel?.();
      });

      el.actions.querySelector('#btnRetry')?.addEventListener('click', () => {
        runFlashWithModal(port, callbacks);
      });
    }
  }

  // =========================================================================
  // API Pública
  // =========================================================================
  window.JWPLCHMIFlasher = {
    isSupported: () => typeof navigator !== 'undefined' && 'serial' in navigator,
    loadBinaries: loadFirmwareBinaries,
    parseFlashArgs,
    FIRMWARE_DIR,
    flash: executeFlash,
    showPromptModal,
    runFlashWithModal
  };
})();

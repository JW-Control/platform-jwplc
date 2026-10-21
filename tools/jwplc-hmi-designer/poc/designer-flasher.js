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

  async function loadFirmwareBinaries(onLog) {
    if (cachedFirmware) return cachedFirmware;

    onLog?.('Descargando binarios precompilados del Live Bridge...');
    const manifestUrl = resolveAssetUrl('./firmware/manifest.json');
    let manifest;
    try {
      const resp = await fetch(manifestUrl, { cache: 'no-store' });
      if (!resp.ok) throw new Error(`HTTP ${resp.status}`);
      manifest = await resp.json();
    } catch (e) {
      onLog?.(`Aviso: manifest.json no cargó (${e.message}), usando rutas por defecto`);
      manifest = {
        target: 'esp32',
        flashMode: 'dio',
        flashFreq: '40m',
        flashSize: '4MB',
        baudRate: 921600,
        romBaudRate: 115200,
        parts: [
          { name: 'bootloader', path: './firmware/bootloader.bin', address: 0x1000 },
          { name: 'partitions', path: './firmware/partitions.bin', address: 0x8000 },
          // Igual que la subida de Arduino (flash_args): otadata que arranca app0.
          { name: 'boot_app0', path: './firmware/boot_app0.bin', address: 0xe000 },
          { name: 'app', path: './firmware/firmware.bin', address: 0x10000 }
        ]
      };
    }

    const loadedParts = [];
    for (const part of manifest.parts) {
      const fileUrl = resolveAssetUrl(part.path);
      onLog?.(`Cargando ${part.name} (${part.path})...`);
      const fileResp = await fetch(fileUrl, { cache: 'no-store' });
      if (!fileResp.ok) throw new Error(`No se pudo descargar ${part.path}: HTTP ${fileResp.status}`);
      const arrayBuffer = await fileResp.arrayBuffer();
      loadedParts.push({
        name: part.name,
        address: part.address,
        data: new Uint8Array(arrayBuffer),
        desc: part.desc || part.name
      });
    }

    cachedFirmware = {
      manifest,
      parts: loadedParts,
      totalBytes: loadedParts.reduce((acc, p) => acc + p.data.length, 0)
    };

    onLog?.(`Binarios listos: ${(cachedFirmware.totalBytes / 1024).toFixed(1)} KB en total.`);
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
      flashMode: firmware.manifest.flashMode || 'dio',
      flashFreq: firmware.manifest.flashFreq || '40m',
      flashSize: firmware.manifest.flashSize || '4MB',
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
      await loader.after('hard_reset');
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
      El proceso toma ~3 segundos a 921,600 baudios y no requiere abrir Arduino IDE.
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
      const result = await executeFlash(port, {
        onProgress: (pct, msg) => {
          modal.setProgress(pct, msg);
        },
        onLog: (text) => {
          modal.log(text);
        }
      });

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
    flash: executeFlash,
    showPromptModal,
    runFlashWithModal
  };
})();

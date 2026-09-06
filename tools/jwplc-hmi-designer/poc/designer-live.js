(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const FRAME_PIXELS = WIDTH * HEIGHT;
  const BAUD_RATE = 921600;
  const PROBE_MS = 500;
  const FALLBACK_SYNC_MS = 1000;
  const ACK_TIMEOUT_MS = 4000;
  const TX_CHUNK_BYTES = 1024;
  const REGION_MAX_AREA_RATIO = 0.65;

  const FULL_MAGIC = [0x4A, 0x57, 0x48, 0x31]; // JWH1
  const REGION_MAGIC = [0x4A, 0x57, 0x48, 0x32]; // JWH2
  const PROBE = new Uint8Array([0x4A, 0x57, 0x48, 0x3F]); // JWH?

  const previewCanvas = document.getElementById('previewCanvas');
  const toolbar = document.querySelector('.toolbar');
  const newProjectButton = document.getElementById('newProjectButton');
  const componentTools = [...document.querySelectorAll('.component-tool[data-tool]')];
  const demoButton = document.getElementById('demoButton');
  const demoValueButton = document.getElementById('demoValueButton');
  const codePanel = document.querySelector('.code-panel');
  const codeOutput = document.getElementById('codeOutput');
  const statusTab = document.getElementById('statusTab');
  const contractTab = document.getElementById('contractTab');
  const bottomTabs = [...document.querySelectorAll('.bottom-tabs .tab')];
  const diagnosticTab = bottomTabs.find((button) => button.textContent.trim() === 'Diagnóstico');

  const liveButton = document.createElement('button');
  liveButton.id = 'liveConnectButton';
  liveButton.type = 'button';
  liveButton.textContent = 'Conectar JWPLC';
  liveButton.title = 'Preview físico en vivo por USB / Web Serial';

  const liveStatus = document.createElement('span');
  liveStatus.id = 'liveStatus';
  liveStatus.className = 'live-status';
  liveStatus.textContent = 'LIVE desconectado';

  const separator = document.createElement('span');
  separator.className = 'separator';

  const fitButton = document.getElementById('fitButton');
  if (toolbar && fitButton) {
    toolbar.insertBefore(separator, fitButton);
    toolbar.insertBefore(liveButton, fitButton);
    toolbar.insertBefore(liveStatus, fitButton);
  }

  const diagnosticPanel = document.createElement('section');
  diagnosticPanel.id = 'liveDiagnosticPanel';
  diagnosticPanel.hidden = true;
  diagnosticPanel.innerHTML = `
    <div class="live-diag-toolbar">
      <strong>Monitor LIVE</strong>
      <span id="diagConnection">Desconectado</span>
      <span class="grow"></span>
      <button id="diagPauseButton" type="button">Pausar log</button>
      <button id="diagClearButton" type="button">Limpiar</button>
      <button id="diagCopyButton" type="button">Copiar</button>
    </div>
    <div class="live-diag-grid">
      <div><span>Baud</span><strong id="diagBaud">921600</strong></div>
      <div><span>FPS efectivo</span><strong id="diagFps">—</strong></div>
      <div><span>Último ACK</span><strong id="diagAck">—</strong></div>
      <div><span>TX último</span><strong id="diagBytes">—</strong></div>
      <div><span>Modo</span><strong id="diagMode">—</strong></div>
      <div><span>Región</span><strong id="diagRegion">—</strong></div>
      <div><span>FULL confirmados</span><strong id="diagFullCount">0</strong></div>
      <div><span>REGION confirmados</span><strong id="diagRegionCount">0</strong></div>
      <div><span>Heap libre</span><strong id="diagHeapFree">—</strong></div>
      <div><span>Heap mínimo</span><strong id="diagHeapMin">—</strong></div>
      <div><span>Bloque mayor</span><strong id="diagHeapLargest">—</strong></div>
      <div><span>Errores</span><strong id="diagErrors">0</strong></div>
    </div>
    <pre id="diagLog" class="live-diag-log"></pre>
  `;
  codePanel?.appendChild(diagnosticPanel);

  const style = document.createElement('style');
  style.textContent = `
    .live-status{font-size:11px;color:#7893a5;white-space:nowrap;align-self:center}
    .live-status.ready{color:#56e39f}
    .live-status.waiting{color:#f0b35a}
    #liveConnectButton.live{border-color:#35d07f;color:#7af0ae;box-shadow:inset 0 -2px 0 rgba(53,208,127,.35)}
    #liveConnectButton.waiting{border-color:#d99a45;color:#efc078}
    #liveDiagnosticPanel{height:100%;min-height:0;padding:8px 12px;overflow:hidden;background:#08131b;color:#c9dae5}
    .live-diag-toolbar{display:flex;align-items:center;gap:10px;margin-bottom:8px;font-size:12px}
    .live-diag-toolbar .grow{flex:1}
    .live-diag-toolbar button{padding:4px 9px}
    .live-diag-grid{display:grid;grid-template-columns:repeat(6,minmax(100px,1fr));gap:6px;margin-bottom:8px}
    .live-diag-grid>div{display:flex;flex-direction:column;padding:5px 7px;border:1px solid #1f3543;border-radius:4px;background:#0b1a24}
    .live-diag-grid span{font-size:10px;color:#718c9f}
    .live-diag-grid strong{font-size:12px;color:#d9edf8;font-weight:600}
    .live-diag-log{height:108px;margin:0;padding:7px;overflow:auto;border:1px solid #1f3543;background:#061018;color:#8fd7aa;font:11px/1.35 Consolas,monospace;white-space:pre-wrap}
    @media(max-width:1200px){.live-diag-grid{grid-template-columns:repeat(4,minmax(100px,1fr))}}
  `;
  document.head.appendChild(style);

  if (diagnosticTab) {
    diagnosticTab.disabled = false;
    diagnosticTab.id = 'diagnosticTab';
  }

  const diag = {
    connection: document.getElementById('diagConnection'),
    fps: document.getElementById('diagFps'),
    ack: document.getElementById('diagAck'),
    bytes: document.getElementById('diagBytes'),
    mode: document.getElementById('diagMode'),
    region: document.getElementById('diagRegion'),
    fullCount: document.getElementById('diagFullCount'),
    regionCount: document.getElementById('diagRegionCount'),
    heapFree: document.getElementById('diagHeapFree'),
    heapMin: document.getElementById('diagHeapMin'),
    heapLargest: document.getElementById('diagHeapLargest'),
    errors: document.getElementById('diagErrors'),
    log: document.getElementById('diagLog'),
    pauseButton: document.getElementById('diagPauseButton'),
    clearButton: document.getElementById('diagClearButton'),
    copyButton: document.getElementById('diagCopyButton')
  };

  let port = null;
  let writer = null;
  let reader = null;
  let bridgeReady = false;
  let sequence = 0;
  let sendInFlight = false;
  let dirtyRequested = false;
  let forceFull = true;
  let probeTimer = null;
  let fallbackTimer = null;
  let ackTimer = null;
  let syncRaf = null;
  let readBuffer = '';
  let awaitingAckSequence = null;
  let pendingFrame = null;
  let pendingMeta = null;
  let lastAckFrame = null;
  let lastAckAt = 0;
  let fullCount = 0;
  let regionCount = 0;
  let hostErrors = 0;
  let diagPaused = false;
  const diagLines = [];

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

  function timestamp() {
    const now = new Date();
    return now.toLocaleTimeString('es-PE', { hour12: false }) + `.${String(now.getMilliseconds()).padStart(3, '0')}`;
  }

  function refreshLog() {
    if (!diag.log || diagPaused) return;
    diag.log.textContent = diagLines.join('\n');
    diag.log.scrollTop = diag.log.scrollHeight;
  }

  function logLine(text) {
    diagLines.push(`[${timestamp()}] ${text}`);
    if (diagLines.length > 250) diagLines.splice(0, diagLines.length - 250);
    refreshLog();
  }

  function showDiagnostic() {
    if (!diagnosticPanel || !codeOutput) return;
    codeOutput.hidden = true;
    diagnosticPanel.hidden = false;
    bottomTabs.forEach((tab) => tab.classList.toggle('active', tab === diagnosticTab));
  }

  function showCodeOutput() {
    if (!diagnosticPanel || !codeOutput) return;
    diagnosticPanel.hidden = true;
    codeOutput.hidden = false;
    diagnosticTab?.classList.remove('active');
  }

  diagnosticTab?.addEventListener('click', showDiagnostic);
  statusTab?.addEventListener('click', showCodeOutput);
  contractTab?.addEventListener('click', showCodeOutput);
  diag.pauseButton?.addEventListener('click', () => {
    diagPaused = !diagPaused;
    diag.pauseButton.textContent = diagPaused ? 'Reanudar log' : 'Pausar log';
    if (!diagPaused) refreshLog();
  });
  diag.clearButton?.addEventListener('click', () => {
    diagLines.length = 0;
    refreshLog();
  });
  diag.copyButton?.addEventListener('click', async () => {
    try {
      await navigator.clipboard.writeText(diagLines.join('\n'));
      diag.copyButton.textContent = 'Copiado';
      setTimeout(() => { diag.copyButton.textContent = 'Copiar'; }, 900);
    } catch (_) {
      diag.copyButton.textContent = 'Error';
      setTimeout(() => { diag.copyButton.textContent = 'Copiar'; }, 900);
    }
  });

  function dispatchInput(element, value) {
    if (!element) return;
    if (element.value === String(value)) return;
    element.value = String(value);
    element.dispatchEvent(new Event('input', { bubbles: true }));
    element.dispatchEvent(new Event('change', { bubbles: true }));
  }

  function fieldSerial(field) {
    const match = String(field?.key || '').match(/-(\d+)$/);
    return match ? Number(match[1]) : 1;
  }

  function normalizeSelectedObjectDefaults() {
    const api = editor();
    const field = api?.getSelectedField?.();
    if (!field) return;

    const serial = fieldSerial(field);
    const nameInput = document.getElementById('fieldName');
    const idInput = document.getElementById('fieldId');
    const variableInput = document.getElementById('fieldVariable');

    if (field.type === 'TEXT') {
      const legacyName = field.name === 'Estado' || /^Texto\s+\d+$/i.test(field.name || '');
      const legacyId = field.id === 'FIELD_STATUS' || /^FIELD_TEXT_\d+$/i.test(field.id || '');
      const legacyVariable = field.variable === 'estadoTexto' || /^texto\d+$/i.test(field.variable || '');
      if (legacyName) dispatchInput(nameInput, `TEXT ${serial}`);
      if (legacyId) dispatchInput(idInput, `FIELD_TEXT_${serial}`);
      if (legacyVariable) dispatchInput(variableInput, `texto${serial}`);
    } else if (field.type === 'VALUE') {
      const legacyName = field.name === 'Temperatura' || /^Valor\s+\d+$/i.test(field.name || '');
      const legacyId = field.id === 'FIELD_TEMP' || /^FIELD_VALUE_\d+$/i.test(field.id || '');
      const legacyVariable = field.variable === 'temperatura' || /^valor\d+$/i.test(field.variable || '');
      if (legacyName) dispatchInput(nameInput, `VALUE ${serial}`);
      if (legacyId) dispatchInput(idInput, `FIELD_VALUE_${serial}`);
      if (legacyVariable) dispatchInput(variableInput, `valor${serial}`);
    }
  }

  function scheduleNormalize() {
    setTimeout(normalizeSelectedObjectDefaults, 0);
  }

  componentTools.forEach((button) => button.addEventListener('click', scheduleNormalize));
  newProjectButton?.addEventListener('click', scheduleNormalize);
  demoButton?.addEventListener('click', scheduleNormalize);
  demoValueButton?.addEventListener('click', scheduleNormalize);
  setTimeout(normalizeSelectedObjectDefaults, 0);

  function rgb888To565(r, g, b) {
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
  }

  function captureFrame565() {
    const ctx = previewCanvas.getContext('2d', { alpha: false });
    const rgba = ctx.getImageData(0, 0, WIDTH, HEIGHT).data;
    const frame = new Uint16Array(FRAME_PIXELS);
    for (let i = 0, p = 0; i < FRAME_PIXELS; i += 1, p += 4) {
      frame[i] = rgb888To565(rgba[p], rgba[p + 1], rgba[p + 2]);
    }
    return frame;
  }

  function computeDirtyRect(current, baseline) {
    if (!baseline) return { x: 0, y: 0, width: WIDTH, height: HEIGHT };
    let minX = WIDTH;
    let minY = HEIGHT;
    let maxX = -1;
    let maxY = -1;

    for (let y = 0; y < HEIGHT; y += 1) {
      const row = y * WIDTH;
      for (let x = 0; x < WIDTH; x += 1) {
        const index = row + x;
        if (current[index] === baseline[index]) continue;
        if (x < minX) minX = x;
        if (x > maxX) maxX = x;
        if (y < minY) minY = y;
        if (y > maxY) maxY = y;
      }
    }

    if (maxX < minX || maxY < minY) return null;
    return {
      x: minX,
      y: minY,
      width: maxX - minX + 1,
      height: maxY - minY + 1
    };
  }

  function buildRuns(frame, rect) {
    const runs = [];
    let current = -1;
    let count = 0;
    for (let y = rect.y; y < rect.y + rect.height; y += 1) {
      const row = y * WIDTH;
      for (let x = rect.x; x < rect.x + rect.width; x += 1) {
        const color = frame[row + x];
        if (color === current && count < 0xFFFF) {
          count += 1;
        } else {
          if (count > 0) runs.push([count, current]);
          current = color;
          count = 1;
        }
      }
    }
    if (count > 0) runs.push([count, current]);
    return runs;
  }

  function nextSequence() {
    sequence = (sequence + 1) >>> 0;
    return sequence;
  }

  function appendRuns(packet, offset, runs) {
    const view = new DataView(packet.buffer);
    runs.forEach(([count, color]) => {
      view.setUint16(offset, count, true);
      view.setUint16(offset + 2, color, true);
      offset += 4;
    });
  }

  function encodeFullPacket(frame) {
    const rect = { x: 0, y: 0, width: WIDTH, height: HEIGHT };
    const runs = buildRuns(frame, rect);
    const seq = nextSequence();
    const packet = new Uint8Array(16 + runs.length * 4);
    const view = new DataView(packet.buffer);
    packet.set(FULL_MAGIC, 0);
    view.setUint32(4, seq, true);
    view.setUint32(8, runs.length, true);
    view.setUint16(12, WIDTH, true);
    view.setUint16(14, HEIGHT, true);
    appendRuns(packet, 16, runs);
    return { packet, seq, runs: runs.length, rect, mode: 'FULL' };
  }

  function encodeRegionPacket(frame, rect) {
    const runs = buildRuns(frame, rect);
    const seq = nextSequence();
    const packet = new Uint8Array(20 + runs.length * 4);
    const view = new DataView(packet.buffer);
    packet.set(REGION_MAGIC, 0);
    view.setUint32(4, seq, true);
    view.setUint32(8, runs.length, true);
    view.setUint16(12, rect.x, true);
    view.setUint16(14, rect.y, true);
    view.setUint16(16, rect.width, true);
    view.setUint16(18, rect.height, true);
    appendRuns(packet, 20, runs);
    return { packet, seq, runs: runs.length, rect, mode: 'REGION' };
  }

  async function writePacketChunked(packet) {
    for (let offset = 0; offset < packet.length; offset += TX_CHUNK_BYTES) {
      await writer.write(packet.subarray(offset, Math.min(offset + TX_CHUNK_BYTES, packet.length)));
    }
  }

  async function sendProbe() {
    if (!writer || bridgeReady) return;
    try {
      await writer.write(PROBE);
    } catch (error) {
      if (port) console.warn('[JWPLC LIVE] probe failed', error);
    }
  }

  function stopProbeTimer() {
    if (probeTimer) clearInterval(probeTimer);
    probeTimer = null;
  }

  function stopAckTimer() {
    if (ackTimer) clearTimeout(ackTimer);
    ackTimer = null;
  }

  function updateDiagForSend(meta) {
    if (diag.mode) diag.mode.textContent = meta.mode;
    if (diag.bytes) diag.bytes.textContent = `${meta.packetBytes.toLocaleString()} B`;
    if (diag.region) {
      const r = meta.rect;
      diag.region.textContent = meta.mode === 'FULL'
        ? '320×170'
        : `X${r.x} Y${r.y} ${r.width}×${r.height}`;
    }
  }

  function armAckTimeout(expectedSequence) {
    stopAckTimer();
    ackTimer = setTimeout(() => {
      if (awaitingAckSequence !== expectedSequence) return;
      hostErrors += 1;
      if (diag.errors) diag.errors.textContent = String(hostErrors);
      logLine(`TIMEOUT ACK seq=${expectedSequence}; próximo envío será FULL`);
      awaitingAckSequence = null;
      pendingFrame = null;
      pendingMeta = null;
      lastAckFrame = null;
      forceFull = true;
      dirtyRequested = true;
      liveStatus.textContent = 'LIVE · reintentando FULL';
      liveStatus.className = 'live-status waiting';
      requestLiveSync(true);
    }, ACK_TIMEOUT_MS);
  }

  function requestLiveSync(force = false) {
    if (force) forceFull = true;
    dirtyRequested = true;
    if (syncRaf !== null) return;
    syncRaf = requestAnimationFrame(() => {
      syncRaf = null;
      trySendLatest();
    });
  }

  async function trySendLatest() {
    if (!writer || !bridgeReady || !dirtyRequested || sendInFlight || awaitingAckSequence !== null) return;

    const frame = captureFrame565();
    let rect = computeDirtyRect(frame, lastAckFrame);
    if (!rect && !forceFull) {
      dirtyRequested = false;
      return;
    }

    if (!rect) rect = { x: 0, y: 0, width: WIDTH, height: HEIGHT };
    const areaRatio = (rect.width * rect.height) / FRAME_PIXELS;
    const encoded = (forceFull || !lastAckFrame || areaRatio >= REGION_MAX_AREA_RATIO)
      ? encodeFullPacket(frame)
      : encodeRegionPacket(frame, rect);

    dirtyRequested = false;
    forceFull = false;
    sendInFlight = true;
    awaitingAckSequence = encoded.seq;
    pendingFrame = frame;
    pendingMeta = {
      ...encoded,
      packetBytes: encoded.packet.length,
      sentAt: performance.now()
    };
    updateDiagForSend(pendingMeta);
    armAckTimeout(encoded.seq);
    logLine(`TX ${encoded.mode} seq=${encoded.seq} bytes=${encoded.packet.length} runs=${encoded.runs} region=${encoded.rect.x},${encoded.rect.y},${encoded.rect.width}x${encoded.rect.height}`);

    liveStatus.textContent = `LIVE · ${encoded.mode} ${encoded.seq}`;
    liveStatus.className = 'live-status ready';

    try {
      await writePacketChunked(encoded.packet);
    } catch (error) {
      stopAckTimer();
      awaitingAckSequence = null;
      pendingFrame = null;
      pendingMeta = null;
      lastAckFrame = null;
      forceFull = true;
      dirtyRequested = true;
      hostErrors += 1;
      if (diag.errors) diag.errors.textContent = String(hostErrors);
      liveStatus.textContent = 'LIVE · error de envío';
      liveStatus.className = 'live-status waiting';
      logLine(`ERROR TX ${error?.message || error}`);
      console.error('[JWPLC LIVE] write failed', error);
    } finally {
      sendInFlight = false;
      if (dirtyRequested && awaitingAckSequence === null) requestLiveSync(forceFull);
    }
  }

  function parseStats(line) {
    const values = {};
    const regex = /(\w+)=([^\s]+)/g;
    let match;
    while ((match = regex.exec(line)) !== null) values[match[1]] = match[2];
    if (diag.heapFree && values.free) diag.heapFree.textContent = `${Number(values.free).toLocaleString()} B`;
    if (diag.heapMin && values.min) diag.heapMin.textContent = `${Number(values.min).toLocaleString()} B`;
    if (diag.heapLargest && values.largest) diag.heapLargest.textContent = `${Number(values.largest).toLocaleString()} B`;
    if (values.errors) {
      const total = Math.max(hostErrors, Number(values.errors) || 0);
      if (diag.errors) diag.errors.textContent = String(total);
    }
  }

  function handleDeviceLine(line) {
    const clean = line.trim();
    if (!clean) return;

    if (clean.startsWith('JWHMI_LIVE_READY')) {
      logLine(`RX ${clean}`);
      if (bridgeReady) return;
      bridgeReady = true;
      stopProbeTimer();
      liveButton.classList.remove('waiting');
      liveButton.classList.add('live');
      liveButton.textContent = 'Desconectar LIVE';
      liveStatus.textContent = 'JWPLC Basic · LIVE listo';
      liveStatus.className = 'live-status ready';
      if (diag.connection) diag.connection.textContent = 'JWPLC Basic · LIVE listo';
      lastAckFrame = null;
      forceFull = true;
      requestLiveSync(true);
      return;
    }

    if (clean.startsWith('JWHMI_LIVE_FRAME')) {
      const ackSequence = Number(clean.split(/\s+/)[1]);
      if (Number.isFinite(ackSequence) && ackSequence === awaitingAckSequence && pendingMeta) {
        stopAckTimer();
        const now = performance.now();
        const ackMs = now - pendingMeta.sentAt;
        const fps = lastAckAt > 0 ? 1000 / Math.max(1, now - lastAckAt) : 0;
        lastAckAt = now;
        lastAckFrame = pendingFrame;
        if (pendingMeta.mode === 'FULL') fullCount += 1;
        else regionCount += 1;
        if (diag.ack) diag.ack.textContent = `${ackMs.toFixed(1)} ms`;
        if (diag.fps) diag.fps.textContent = fps > 0 ? fps.toFixed(1) : '—';
        if (diag.fullCount) diag.fullCount.textContent = String(fullCount);
        if (diag.regionCount) diag.regionCount.textContent = String(regionCount);
        logLine(`ACK seq=${ackSequence} ${pendingMeta.mode} ${ackMs.toFixed(1)}ms`);
        awaitingAckSequence = null;
        pendingFrame = null;
        pendingMeta = null;
        liveStatus.textContent = `LIVE · frame ${ackSequence}`;
        liveStatus.className = 'live-status ready';
        if (dirtyRequested) requestLiveSync(false);
      }
      return;
    }

    if (clean.startsWith('JWHMI_LIVE_STATS')) {
      parseStats(clean);
      logLine(`RX ${clean}`);
      return;
    }

    if (clean.startsWith('JWHMI_LIVE_ERROR')) {
      stopAckTimer();
      hostErrors += 1;
      if (diag.errors) diag.errors.textContent = String(hostErrors);
      awaitingAckSequence = null;
      pendingFrame = null;
      pendingMeta = null;
      lastAckFrame = null;
      forceFull = true;
      dirtyRequested = true;
      liveStatus.textContent = `LIVE · ${clean.replace('JWHMI_LIVE_ERROR ', '')}`;
      liveStatus.className = 'live-status waiting';
      logLine(`RX ${clean}; resync FULL`);
      requestLiveSync(true);
      console.warn('[JWPLC LIVE]', clean);
      return;
    }

    logLine(`RX ${clean}`);
    console.debug('[JWPLC LIVE]', clean);
  }

  async function readLoop() {
    const decoder = new TextDecoder();
    try {
      while (reader) {
        const { value, done } = await reader.read();
        if (done) break;
        readBuffer += decoder.decode(value, { stream: true });
        const lines = readBuffer.split(/\r?\n/);
        readBuffer = lines.pop() || '';
        lines.forEach(handleDeviceLine);
      }
    } catch (error) {
      if (port) {
        logLine(`Reader detenido: ${error?.message || error}`);
        console.warn('[JWPLC LIVE] reader stopped', error);
      }
    }
  }

  function portDescription() {
    try {
      const info = port?.getInfo?.() || {};
      if (info.usbVendorId || info.usbProductId) {
        const vid = (info.usbVendorId || 0).toString(16).padStart(4, '0');
        const pid = (info.usbProductId || 0).toString(16).padStart(4, '0');
        return `USB ${vid}:${pid}`;
      }
    } catch (_) {}
    return 'Web Serial';
  }

  async function connectLive() {
    if (!('serial' in navigator)) {
      liveStatus.textContent = 'Web Serial no disponible';
      liveStatus.className = 'live-status waiting';
      return;
    }

    port = await navigator.serial.requestPort();
    await port.open({ baudRate: BAUD_RATE, bufferSize: 4096 });
    writer = port.writable.getWriter();
    reader = port.readable.getReader();
    bridgeReady = false;
    lastAckFrame = null;
    pendingFrame = null;
    pendingMeta = null;
    awaitingAckSequence = null;
    readBuffer = '';
    dirtyRequested = true;
    forceFull = true;
    lastAckAt = 0;

    liveButton.classList.add('waiting');
    liveButton.textContent = 'Esperando bridge…';
    liveStatus.textContent = 'USB conectado · buscando JWPLC';
    liveStatus.className = 'live-status waiting';
    if (diag.connection) diag.connection.textContent = `${portDescription()} · buscando bridge`;
    logLine(`OPEN ${portDescription()} @ ${BAUD_RATE}`);
    readLoop();

    await sendProbe();
    probeTimer = setInterval(() => sendProbe(), PROBE_MS);
    fallbackTimer = setInterval(() => requestLiveSync(false), FALLBACK_SYNC_MS);
  }

  async function disconnectLive() {
    if (fallbackTimer) clearInterval(fallbackTimer);
    fallbackTimer = null;
    stopProbeTimer();
    stopAckTimer();
    if (syncRaf !== null) cancelAnimationFrame(syncRaf);
    syncRaf = null;
    bridgeReady = false;
    awaitingAckSequence = null;
    pendingFrame = null;
    pendingMeta = null;
    lastAckFrame = null;

    try { await reader?.cancel(); } catch (_) {}
    try { reader?.releaseLock(); } catch (_) {}
    reader = null;
    try { writer?.releaseLock(); } catch (_) {}
    writer = null;
    try { await port?.close(); } catch (_) {}
    port = null;

    liveButton.classList.remove('live', 'waiting');
    liveButton.textContent = 'Conectar JWPLC';
    liveStatus.textContent = 'LIVE desconectado';
    liveStatus.className = 'live-status';
    if (diag.connection) diag.connection.textContent = 'Desconectado';
    logLine('CLOSE LIVE');
  }

  liveButton.addEventListener('click', async () => {
    try {
      if (port) await disconnectLive();
      else await connectLive();
    } catch (error) {
      console.error('[JWPLC LIVE] connect failed', error);
      liveStatus.textContent = 'LIVE · no se pudo conectar';
      liveStatus.className = 'live-status waiting';
      logLine(`ERROR CONNECT ${error?.message || error}`);
      try { await disconnectLive(); } catch (_) {}
    }
  });

  // El editor emite este evento después de renderizar su framebuffer. El LIVE
  // lo usa como scheduler principal; ya no depende de un polling de 120 ms.
  window.addEventListener('jwplc:editor-refresh', () => requestLiveSync(false));

  navigator.serial?.addEventListener?.('disconnect', () => {
    if (port) disconnectLive();
  });

  if (!('serial' in navigator)) {
    liveButton.disabled = true;
    liveButton.title = 'Web Serial requiere Chrome/Edge de escritorio sobre localhost/HTTPS';
    liveStatus.textContent = 'Web Serial no disponible';
    if (diag.connection) diag.connection.textContent = 'Web Serial no disponible';
  }

  window.JWPLCHMILive = {
    isConnected: () => Boolean(port),
    isReady: () => bridgeReady,
    sendFrame: () => requestLiveSync(true),
    disconnect: disconnectLive,
    openDiagnostic: showDiagnostic
  };
})();

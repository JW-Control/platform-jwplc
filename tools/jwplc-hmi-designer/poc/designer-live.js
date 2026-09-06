(() => {
  'use strict';

  const WIDTH = 320;
  const HEIGHT = 170;
  const BAUD_RATE = 921600;
  const POLL_MS = 120;
  const PROBE_MS = 500;
  const ACK_TIMEOUT_MS = 4000;
  const TX_CHUNK_BYTES = 1024;
  const MAGIC = [0x4A, 0x57, 0x48, 0x31]; // JWH1
  const PROBE = new Uint8Array([0x4A, 0x57, 0x48, 0x3F]); // JWH?

  const previewCanvas = document.getElementById('previewCanvas');
  const toolbar = document.querySelector('.toolbar');
  const newProjectButton = document.getElementById('newProjectButton');
  const componentTools = [...document.querySelectorAll('.component-tool[data-tool]')];
  const demoButton = document.getElementById('demoButton');
  const demoValueButton = document.getElementById('demoValueButton');

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

  const style = document.createElement('style');
  style.textContent = `
    .live-status{font-size:11px;color:#7893a5;white-space:nowrap;align-self:center}
    .live-status.ready{color:#56e39f}
    .live-status.waiting{color:#f0b35a}
    #liveConnectButton.live{border-color:#35d07f;color:#7af0ae;box-shadow:inset 0 -2px 0 rgba(53,208,127,.35)}
    #liveConnectButton.waiting{border-color:#d99a45;color:#efc078}
  `;
  document.head.appendChild(style);

  let port = null;
  let writer = null;
  let reader = null;
  let bridgeReady = false;
  let sequence = 0;
  let lastHash = null;
  let sendInFlight = false;
  let forceNextFrame = false;
  let pollTimer = null;
  let probeTimer = null;
  let ackTimer = null;
  let readBuffer = '';
  let awaitingAckSequence = null;
  let pendingHash = null;

  function editor() {
    return window.JWPLCHMIEditor || null;
  }

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

  function captureRuns() {
    const ctx = previewCanvas.getContext('2d', { alpha: false });
    const rgba = ctx.getImageData(0, 0, WIDTH, HEIGHT).data;
    const runs = [];
    let hash = 0x811C9DC5 >>> 0;
    let current = -1;
    let count = 0;

    for (let i = 0, p = 0; i < WIDTH * HEIGHT; i += 1, p += 4) {
      const color = rgb888To565(rgba[p], rgba[p + 1], rgba[p + 2]);
      hash ^= color & 0xFF;
      hash = Math.imul(hash, 0x01000193) >>> 0;
      hash ^= color >>> 8;
      hash = Math.imul(hash, 0x01000193) >>> 0;

      if (color === current && count < 0xFFFF) {
        count += 1;
      } else {
        if (count > 0) runs.push([count, current]);
        current = color;
        count = 1;
      }
    }
    if (count > 0) runs.push([count, current]);
    return { runs, hash };
  }

  function encodeFrame(runs) {
    sequence = (sequence + 1) >>> 0;
    const packet = new Uint8Array(16 + runs.length * 4);
    const view = new DataView(packet.buffer);
    packet.set(MAGIC, 0);
    view.setUint32(4, sequence, true);
    view.setUint32(8, runs.length, true);
    view.setUint16(12, WIDTH, true);
    view.setUint16(14, HEIGHT, true);
    let offset = 16;
    runs.forEach(([count, color]) => {
      view.setUint16(offset, count, true);
      view.setUint16(offset + 2, color, true);
      offset += 4;
    });
    return packet;
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

  function armAckTimeout(expectedSequence) {
    stopAckTimer();
    ackTimer = setTimeout(() => {
      if (awaitingAckSequence !== expectedSequence) return;
      console.warn(`[JWPLC LIVE] ACK timeout frame ${expectedSequence}`);
      awaitingAckSequence = null;
      pendingHash = null;
      forceNextFrame = true;
      liveStatus.textContent = 'LIVE · reintentando frame';
      liveStatus.className = 'live-status waiting';
    }, ACK_TIMEOUT_MS);
  }

  async function sendCurrentFrame(force = false) {
    if (!writer || !bridgeReady || sendInFlight || awaitingAckSequence !== null) {
      if (force) forceNextFrame = true;
      return;
    }

    const { runs, hash } = captureRuns();
    if (!force && !forceNextFrame && hash === lastHash) return;

    sendInFlight = true;
    forceNextFrame = false;
    try {
      const packet = encodeFrame(runs);
      const frameSequence = sequence;
      await writePacketChunked(packet);
      awaitingAckSequence = frameSequence;
      pendingHash = hash;
      armAckTimeout(frameSequence);
      liveStatus.textContent = `LIVE · enviando frame ${frameSequence}`;
      liveStatus.className = 'live-status ready';
    } catch (error) {
      awaitingAckSequence = null;
      pendingHash = null;
      liveStatus.textContent = 'LIVE · error de envío';
      liveStatus.className = 'live-status waiting';
      console.error('[JWPLC LIVE] write failed', error);
    } finally {
      sendInFlight = false;
    }
  }

  function handleDeviceLine(line) {
    const clean = line.trim();
    if (!clean) return;

    if (clean.startsWith('JWHMI_LIVE_READY')) {
      if (bridgeReady) return;
      bridgeReady = true;
      stopProbeTimer();
      liveButton.classList.remove('waiting');
      liveButton.classList.add('live');
      liveButton.textContent = 'Desconectar LIVE';
      liveStatus.textContent = 'JWPLC Basic · LIVE listo';
      liveStatus.className = 'live-status ready';
      forceNextFrame = true;
      sendCurrentFrame(true);
      return;
    }

    if (clean.startsWith('JWHMI_LIVE_FRAME')) {
      const ackSequence = Number(clean.split(/\s+/)[1]);
      if (Number.isFinite(ackSequence) && ackSequence === awaitingAckSequence) {
        stopAckTimer();
        awaitingAckSequence = null;
        lastHash = pendingHash;
        pendingHash = null;
        liveStatus.textContent = `LIVE · frame ${ackSequence}`;
        liveStatus.className = 'live-status ready';
        setTimeout(() => sendCurrentFrame(forceNextFrame), 0);
      }
      return;
    }

    if (clean.startsWith('JWHMI_LIVE_ERROR')) {
      stopAckTimer();
      awaitingAckSequence = null;
      pendingHash = null;
      forceNextFrame = true;
      liveStatus.textContent = `LIVE · ${clean.replace('JWHMI_LIVE_ERROR ', '')}`;
      liveStatus.className = 'live-status waiting';
      console.warn('[JWPLC LIVE]', clean);
      return;
    }

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
      if (port) console.warn('[JWPLC LIVE] reader stopped', error);
    }
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
    lastHash = null;
    awaitingAckSequence = null;
    pendingHash = null;
    readBuffer = '';
    liveButton.classList.add('waiting');
    liveButton.textContent = 'Esperando bridge…';
    liveStatus.textContent = 'USB conectado · buscando JWPLC';
    liveStatus.className = 'live-status waiting';
    readLoop();

    await sendProbe();
    probeTimer = setInterval(() => sendProbe(), PROBE_MS);
    pollTimer = setInterval(() => sendCurrentFrame(false), POLL_MS);
  }

  async function disconnectLive() {
    if (pollTimer) clearInterval(pollTimer);
    pollTimer = null;
    stopProbeTimer();
    stopAckTimer();
    bridgeReady = false;
    awaitingAckSequence = null;
    pendingHash = null;

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
  }

  liveButton.addEventListener('click', async () => {
    try {
      if (port) await disconnectLive();
      else await connectLive();
    } catch (error) {
      console.error('[JWPLC LIVE] connect failed', error);
      liveStatus.textContent = 'LIVE · no se pudo conectar';
      liveStatus.className = 'live-status waiting';
      try { await disconnectLive(); } catch (_) {}
    }
  });

  navigator.serial?.addEventListener?.('disconnect', () => {
    if (port) disconnectLive();
  });

  if (!('serial' in navigator)) {
    liveButton.disabled = true;
    liveButton.title = 'Web Serial requiere Chrome/Edge de escritorio sobre localhost/HTTPS';
    liveStatus.textContent = 'Web Serial no disponible';
  }

  window.JWPLCHMILive = {
    isConnected: () => Boolean(port),
    isReady: () => bridgeReady,
    sendFrame: () => sendCurrentFrame(true),
    disconnect: disconnectLive
  };
})();

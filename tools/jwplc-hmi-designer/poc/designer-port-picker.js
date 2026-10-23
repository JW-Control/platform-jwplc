(() => {
  'use strict';

  // =========================================================================
  // JWPLC HMI Designer — Selector de puerto serie (app de escritorio)
  // Electron pide el puerto en «select-serial-port»; electron/main.js manda
  // aquí la lista y este modal devuelve el elegido. Usa los estilos del modal
  // del flasher (designer-flasher.js se carga antes). En el navegador no hace
  // nada: Chrome/Edge muestran su propio selector.
  // =========================================================================

  const native = window.JWPLCHMINative;
  if (!native?.registerSerialPortPicker) return;

  const styleEl = document.createElement('style');
  styleEl.id = 'jwplc-port-picker-styles';
  styleEl.textContent = `
    .jwplc-port-list {
      display: flex;
      flex-direction: column;
      gap: 6px;
      max-height: 260px;
      overflow-y: auto;
    }
    .jwplc-port-item {
      display: grid;
      grid-template-columns: 16px 1fr auto;
      align-items: center;
      gap: 10px;
      padding: 9px 12px;
      background: #070e14;
      border: 1px solid #182a37;
      border-radius: 8px;
      color: #dfe8f0;
      font: inherit;
      text-align: left;
      cursor: pointer;
      transition: border-color 0.15s ease, background 0.15s ease;
    }
    .jwplc-port-item:hover {
      border-color: #2f5168;
    }
    .jwplc-port-item.selected {
      border-color: #ff8a2a;
      background: rgba(255, 138, 42, 0.08);
    }
    .jwplc-port-radio {
      width: 12px;
      height: 12px;
      border-radius: 50%;
      border: 2px solid #486477;
    }
    .jwplc-port-item.selected .jwplc-port-radio {
      border-color: #ff8a2a;
      background: radial-gradient(circle, #ff8a2a 0 3px, transparent 4px);
    }
    .jwplc-port-name {
      font-size: 13px;
      font-weight: 700;
      color: #eef5fb;
    }
    .jwplc-port-detail {
      display: block;
      margin-top: 2px;
      font-size: 11px;
      color: #8da2b3;
    }
    .jwplc-port-chip {
      font: 700 10px/1 "Cascadia Code", Consolas, monospace;
      color: #52c9ff;
      background: rgba(82, 201, 255, 0.1);
      border: 1px solid rgba(82, 201, 255, 0.3);
      padding: 3px 6px;
      border-radius: 4px;
      white-space: nowrap;
    }
    .jwplc-port-empty {
      padding: 14px;
      border: 1px dashed #20394c;
      border-radius: 8px;
      font-size: 12px;
      color: #8da2b3;
      text-align: center;
    }
  `;
  document.head.appendChild(styleEl);

  let current = null;

  function escapeHtml(value) {
    return String(value ?? '').replace(/[&<>"']/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#39;' }[c]));
  }

  // Primero los chips USB-serie conocidos (los del JWPLC), luego por nombre.
  function sortPorts(ports) {
    return [...ports].sort((a, b) => (Boolean(b.chip) - Boolean(a.chip)) || String(a.portName).localeCompare(String(b.portName), undefined, { numeric: true }));
  }

  function finish(portId) {
    if (!current) return;
    const { requestId, backdrop, onKey } = current;
    current = null;
    document.removeEventListener('keydown', onKey, true);
    backdrop.classList.remove('visible');
    setTimeout(() => backdrop.remove(), 220);
    native.chooseSerialPort(requestId, portId || '');
  }

  function renderList() {
    if (!current) return;
    const { list, connectButton } = current;
    const ports = sortPorts(current.ports);
    if (!ports.some((p) => p.portId === current.selected)) {
      current.selected = ports.find((p) => p.chip)?.portId || ports[0]?.portId || null;
    }

    if (!ports.length) {
      list.innerHTML = '<div class="jwplc-port-empty">No se detecta ningún puerto serie.<br/>Conecta el JWPLC por USB; aparecerá aquí sin cerrar esta ventana.</div>';
    } else {
      list.innerHTML = ports.map((p) => {
        const name = p.portName || p.displayName || 'Puerto serie';
        const detail = [p.portName ? p.displayName : '', p.vendorId && p.productId ? `USB ${p.vendorId}:${p.productId}` : '']
          .filter(Boolean).join(' · ');
        return `
          <button type="button" class="jwplc-port-item${p.portId === current.selected ? ' selected' : ''}" data-port-id="${escapeHtml(p.portId)}">
            <span class="jwplc-port-radio"></span>
            <span><span class="jwplc-port-name">${escapeHtml(name)}</span>${detail ? `<span class="jwplc-port-detail">${escapeHtml(detail)}</span>` : ''}</span>
            ${p.chip ? `<span class="jwplc-port-chip">${escapeHtml(p.chip)}</span>` : '<span></span>'}
          </button>`;
      }).join('');
    }
    connectButton.disabled = !current.selected;
  }

  function showPicker({ requestId, ports }) {
    if (current) finish('');

    const backdrop = document.createElement('div');
    backdrop.className = 'jwplc-flash-backdrop';
    backdrop.innerHTML = `
      <div class="jwplc-flash-modal" role="dialog" aria-modal="true" aria-labelledby="jwplcPortTitle">
        <div class="jwplc-flash-header">
          <div class="jwplc-flash-icon">⇄</div>
          <div class="jwplc-flash-title-area">
            <span class="jwplc-flash-badge">JWPLC · PUERTO SERIE</span>
            <h3 id="jwplcPortTitle" class="jwplc-flash-title">Conectar JWPLC</h3>
          </div>
        </div>
        <p class="jwplc-flash-desc">Elige el puerto USB del PLC. La lista se actualiza si conectas o desconectas el cable.</p>
        <div class="jwplc-port-list"></div>
        <div class="jwplc-flash-actions">
          <button type="button" data-action="cancel" class="jwplc-flash-btn jwplc-flash-btn-secondary">Cancelar</button>
          <button type="button" data-action="connect" class="jwplc-flash-btn jwplc-flash-btn-primary">Conectar</button>
        </div>
      </div>
    `;
    document.body.appendChild(backdrop);
    requestAnimationFrame(() => backdrop.classList.add('visible'));

    const list = backdrop.querySelector('.jwplc-port-list');
    const connectButton = backdrop.querySelector('[data-action="connect"]');
    const onKey = (event) => {
      if (event.key === 'Escape') { event.preventDefault(); finish(''); }
      else if (event.key === 'Enter' && current?.selected) { event.preventDefault(); finish(current.selected); }
    };

    current = { requestId, ports: ports || [], selected: null, backdrop, list, connectButton, onKey };

    list.addEventListener('click', (event) => {
      const item = event.target.closest('[data-port-id]');
      if (!item || !current) return;
      current.selected = item.dataset.portId;
      renderList();
    });
    list.addEventListener('dblclick', (event) => {
      const item = event.target.closest('[data-port-id]');
      if (item) finish(item.dataset.portId);
    });
    backdrop.querySelector('[data-action="cancel"]').addEventListener('click', () => finish(''));
    connectButton.addEventListener('click', () => finish(current?.selected));
    document.addEventListener('keydown', onKey, true);

    renderList();
    connectButton.focus();
  }

  native.registerSerialPortPicker({
    onRequest: showPicker,
    onList: ({ requestId, ports }) => {
      if (!current || current.requestId !== requestId) return;
      current.ports = ports || [];
      renderList();
    }
  });

  window.JWPLCHMIPortPicker = { show: showPicker, sortPorts };
})();

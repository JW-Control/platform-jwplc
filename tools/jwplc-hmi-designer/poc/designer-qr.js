// tools/jwplc-hmi-designer/poc/designer-qr.js
// Controlador, pintor e inspector Propuesta 2 para códigos QR (objeto type: 'QR').
(() => {
  'use strict';

  if (window.JWPLCHMIQR) return;

  const core = window.JWPLCQRCore;
  if (!core) return;

  const SCREEN_W = core.SCREEN_W;
  const SCREEN_H = core.SCREEN_H;

  const editor = () => window.JWPLCHMIEditor || null;
  const selectedQr = () => {
    const f = editor()?.getSelectedField?.();
    return f?.type === 'QR' ? f : null;
  };

  const clamp = (v, min, max) => Math.min(max, Math.max(min, v));
  const num = (v, d) => (Number.isFinite(Number(v)) ? Number(v) : d);
  const escapeHtml = (s) => String(s ?? '').replace(/[&<>"]/g, (c) => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
  const sanitizeSymbol = (value, fallback) => {
    const cleaned = String(value || '').trim().replace(/[^A-Za-z0-9_]/g, '_');
    if (!cleaned) return fallback;
    return /^[A-Za-z_]/.test(cleaned) ? cleaned : `_${cleaned}`;
  };

  function hex565(value) {
    const n = Number(value);
    return `0x${(Number.isFinite(n) ? n & 0xFFFF : 0).toString(16).toUpperCase().padStart(4, '0')}`;
  }

  function rgb565ToCss(value) {
    const v = Number(value) & 0xFFFF;
    const r = Math.round(((v >> 11) & 0x1F) * 255 / 31);
    const g = Math.round(((v >> 5) & 0x3F) * 255 / 63);
    const b = Math.round((v & 0x1F) * 255 / 31);
    return `rgb(${r}, ${g}, ${b})`;
  }

  function createDefault(keyOrPage, serial = 1, page = 0) {
    let key = keyOrPage;
    let s = serial;
    let p = page;
    if (typeof keyOrPage === 'number') {
      p = keyOrPage;
      key = `qr-${s}`;
    } else if (typeof key !== 'string') {
      key = `qr-${s}`;
    }

    const field = {
      key,
      type: 'QR',
      name: `QR ${s}`,
      id: `FIELD_QR_${s}`,
      page: Number(p) || 0,
      x: 0,
      y: 0,
      sourceMode: 'STATIC',
      preset: 'URL',
      text: core.PRESETS.URL.defaultText,
      variable: `qrTexto${s}`,
      maxLength: 80,
      scale: 3,
      ecc: 'M',
      quietZone: 4,
      frame: false,
      fgColor: 0x0000,
      bgColor: 0xFFFF,
      frameColor: 0xFFFF
    };
    const m = core.computeMetrics(field);
    field.x = Math.max(0, Math.round((SCREEN_W - m.width) / 2));
    field.y = Math.max(0, Math.round((SCREEN_H - m.height) / 2));
    return field;
  }

  function ensureState(field) {
    if (!field || field.type !== 'QR') return field;
    field.sourceMode = field.sourceMode === 'VARIABLE' ? 'VARIABLE' : 'STATIC';
    if (!core.PRESETS[field.preset]) field.preset = 'URL';
    if (typeof field.text !== 'string') field.text = core.PRESETS[field.preset].defaultText;
    if (!field.variable || typeof field.variable !== 'string') field.variable = 'qrTexto';
    field.maxLength = core.clampMaxLength(field.maxLength);
    field.scale = core.clampScale(field.scale);
    field.ecc = core.normalizeEcc(field.ecc);
    field.quietZone = core.clampQuietZone(field.quietZone);
    field.frame = Boolean(field.frame);
    if (!Number.isFinite(Number(field.fgColor))) field.fgColor = 0x0000;
    if (!Number.isFinite(Number(field.bgColor))) field.bgColor = 0xFFFF;
    if (!Number.isFinite(Number(field.frameColor))) field.frameColor = 0xFFFF;
    field.fgColor = Number(field.fgColor) & 0xFFFF;
    field.bgColor = Number(field.bgColor) & 0xFFFF;
    field.frameColor = Number(field.frameColor) & 0xFFFF;
    if (!Number.isFinite(Number(field.x))) field.x = 0;
    if (!Number.isFinite(Number(field.y))) field.y = 0;
    return field;
  }

  function computeBounds(field) {
    ensureState(field);
    const m = core.computeMetrics(field);
    const x = Math.trunc(Number(field.x) || 0);
    const y = Math.trunc(Number(field.y) || 0);
    return {
      x,
      y,
      minX: x,
      minY: y,
      maxX: x + m.width - 1,
      maxY: y + m.height - 1,
      width: m.width,
      height: m.height
    };
  }

  function paint(field, painter) {
    if (!field || !painter) return;
    ensureState(field);
    const m = core.computeMetrics(field);
    const x0 = Math.trunc(Number(field.x) || 0);
    const y0 = Math.trunc(Number(field.y) || 0);
    const fg = field.fgColor & 0xFFFF;
    const bg = field.bgColor & 0xFFFF;
    const fc = field.frameColor & 0xFFFF;

    painter.fillRect(x0, y0, m.width, m.height, bg);

    if (m.ok && m.encoded?.modules) {
      const modSize = m.moduleCount;
      const mods = m.encoded.modules;
      const ox = x0 + m.framePad + m.quietZone * m.scale;
      const oy = y0 + m.framePad + m.quietZone * m.scale;
      for (let my = 0; my < modSize; my += 1) {
        let mx = 0;
        const rowOff = my * modSize;
        while (mx < modSize) {
          if (!mods[rowOff + mx]) {
            mx += 1;
            continue;
          }
          let run = 1;
          while (mx + run < modSize && mods[rowOff + mx + run]) {
            run += 1;
          }
          painter.fillRect(ox + mx * m.scale, oy + my * m.scale, run * m.scale, m.scale, fg);
          mx += run;
        }
      }
    } else if (painter.drawRect) {
      painter.drawRect(x0 + 2, y0 + 2, Math.max(4, m.width - 4), Math.max(4, m.height - 4), 0xF800);
    }

    if (field.frame && painter.drawRect) {
      painter.drawRect(x0, y0, m.width, m.height, fc);
    }
  }

  function contractText(field) {
    if (!field) return '';
    ensureState(field);
    const m = core.computeMetrics(field);
    const id = sanitizeSymbol(field.id, 'FIELD_QR_1');
    const fg = hex565(field.fgColor);
    const bg = hex565(field.bgColor);
    const fc = hex565(field.frameColor);

    if (field.sourceMode === 'VARIABLE') {
      const v = sanitizeSymbol(field.variable, 'qrTexto');
      const eccCpp = core.ECC_LEVELS[m.ecc]?.cpp || 'QR_LEVEL_M';
      return `// Variable HMI:\nchar ${v}[${field.maxLength + 1}] = "${String(field.text || '').replace(/"/g, '\\"')}";\n\n` +
        `// En tu sketch (se redibuja solo si cambia el texto):\n` +
        `strncpy(${v}, "http://192.168.1.99/ok", sizeof(${v}) - 1);\n` +
        `${v}[sizeof(${v}) - 1] = '\\0';\n` +
        `// Config: ${id}, escala ${m.scale}x, nivel ${eccCpp}, margen ${m.quietZone}, fg ${fg}, bg ${bg}${field.frame ? `, marco ${fc}` : ''}`;
    }

    return `// QR estático en Flash (${m.packedBytes} B) · dibujado una vez en jwplcUIPageEnter():\n` +
      `// ${id}: ${m.moduleCount}x${m.moduleCount} módulos (v${m.version}, nivel ${m.ecc})\n` +
      `// Escala ${m.scale}x, margen ${m.quietZone} -> ${m.width}x${m.height} px\n` +
      `jwplcHMIDrawQR(tft, ${Math.trunc(field.x)}, ${Math.trunc(field.y)}, ${m.moduleCount}, ${m.scale}, ${m.quietZone}, ${fg}, ${bg}, ${field.frame ? 'true' : 'false'}, ${fc}, QR_${id}_BITS);`;
  }

  // ---------------------------------------------------------------------------
  // Inspector Propuesta 2 (#qrInspectorSection)
  // ---------------------------------------------------------------------------

  const tabs = { main: 'design', sub: 'shape' };
  let activeColorKey = 'fgColor';
  let qrColorEditor = null;

  const COLOR_ROWS = [
    { key: 'fgColor', letter: 'M', label: 'Módulos (oscuro)' },
    { key: 'bgColor', letter: 'F', label: 'Fondo / Margen' },
    { key: 'frameColor', letter: 'B', label: 'Marco exterior', onlyFrame: true }
  ];

  function injectStyles() {
    if (document.getElementById('a17-qr-style')) return;
    const style = document.createElement('style');
    style.id = 'a17-qr-style';
    style.textContent = `
      .qr-size-sub { display: block; font-size: 9.5px; opacity: .7; margin-top: 1px; }
      .qr-preset-grid { display: grid; grid-template-columns: repeat(3, minmax(0, 1fr)); gap: 5px; }
      .qr-preset-grid .insp-chip { width: 100%; justify-content: center; text-align: center; padding: 5px 4px; }
      .qr-wifi-box { display: flex; flex-direction: column; gap: 6px; padding: 8px; margin-top: 8px; border: 1px solid var(--line); border-radius: 6px; background: var(--panel-inset); }
      .qr-case-body { display: flex; gap: 12px; align-items: center; }
      .qr-case-canvas { border-radius: 4px; background: #000; image-rendering: pixelated; border: 1px solid var(--line); flex-shrink: 0; }
      .qr-warn-banner { margin-top: 7px; padding: 6px 8px; border-radius: 6px; border: 1px solid var(--orange); background: var(--orange-soft); color: var(--text-heading); font-size: 11px; line-height: 1.35; }
      .qr-err-banner { margin-top: 7px; padding: 6px 8px; border-radius: 6px; border: 1px solid var(--red); background: var(--red-soft); color: var(--text-heading); font-size: 11px; line-height: 1.35; }
      .qr-textarea { width: 100%; box-sizing: border-box; min-height: 54px; resize: vertical; font-family: var(--mono); font-size: 11.5px; line-height: 1.35; padding: 6px 8px; }
      .qr-cap-chips { display: flex; gap: 5px; flex-wrap: wrap; margin-top: 6px; }
    `;
    document.head.appendChild(style);
  }

  function applyTabs(section) {
    section.querySelectorAll('[data-qr-main]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrMain === tabs.main);
    });
    const subWrap = section.querySelector('[data-qr-subtabs]');
    if (subWrap) subWrap.hidden = tabs.main !== 'design';
    section.querySelectorAll('[data-qr-sub]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrSub === tabs.sub);
    });
    section.querySelectorAll('[data-qr-panel]').forEach((panel) => {
      const target = tabs.main === 'design' ? `design:${tabs.sub}` : tabs.main;
      panel.hidden = panel.dataset.qrPanel !== target;
    });
  }

  function mutate(apply, commit = true) {
    const field = selectedQr();
    if (!field) return;
    ensureState(field);
    apply(field);
    ensureState(field);
    editor()?.render?.();
    syncInspector();
    if (commit) editor()?.commitHistory?.();
    window.dispatchEvent(new CustomEvent('jwplc:project-changed'));
  }

  function renderMiniPreview(section, field) {
    const body = section.querySelector('[data-qr-preview-body]');
    const canvas = section.querySelector('[data-qr-preview-canvas]');
    if (!body || !canvas) return;
    const m = core.computeMetrics(field);
    const maxPreview = 96;
    const side = Math.max(42, Math.min(maxPreview, m.width));
    const logicalW = m.width;
    const logicalH = m.height;
    canvas.width = logicalW;
    canvas.height = logicalH;
    canvas.style.width = `${side}px`;
    canvas.style.height = `${side}px`;
    canvas.dataset.logicalW = String(logicalW);
    canvas.dataset.logicalH = String(logicalH);

    const ctx = canvas.getContext('2d');
    const painter = {
      fillRect: (x, y, w, h, c) => {
        ctx.fillStyle = rgb565ToCss(c);
        ctx.fillRect(x, y, w, h);
      },
      drawRect: (x, y, w, h, c) => {
        ctx.strokeStyle = rgb565ToCss(c);
        ctx.lineWidth = 1;
        ctx.strokeRect(x + 0.5, y + 0.5, Math.max(0, w - 1), Math.max(0, h - 1));
      }
    };
    paint({ ...field, x: 0, y: 0 }, painter);

    const pad = m.framePad;
    const qzPx = m.quietZone * m.scale;
    const innerSize = m.moduleCount * m.scale;
    let rects = [];
    if (activeColorKey === 'fgColor') {
      rects = [{ x: pad + qzPx, y: pad + qzPx, w: innerSize, h: innerSize }];
    } else if (activeColorKey === 'bgColor') {
      rects = [{ x: pad, y: pad, w: m.qrSide, h: m.qrSide, inset: true }];
    } else if (activeColorKey === 'frameColor' && field.frame) {
      rects = [{ x: 0, y: 0, w: m.width, h: m.height }];
    }
    window.JWPLCZoneIndicator?.draw?.(body, canvas, rects);
  }

  function syncAppearance(section, field) {
    if (activeColorKey === 'frameColor' && !field.frame) activeColorKey = 'fgColor';
    const legend = section.querySelector('[data-qr-legend]');
    if (legend) {
      legend.innerHTML = COLOR_ROWS
        .filter((r) => !r.onlyFrame || field.frame)
        .map((r) => {
          const val = field[r.key] ?? 0;
          const active = activeColorKey === r.key;
          return `<div class="menu-legend-row${active ? ' active' : ''}" data-qr-legend-row="${r.key}">` +
            `<span class="menu-legend-label">${r.label}</span>` +
            `<button type="button" class="menu-swatch${active ? ' active' : ''}" data-qr-swatch="${r.key}" style="background:${rgb565ToCss(val)}" title="${r.label}">` +
            `<span>${r.letter}</span></button></div>`;
        }).join('');
    }
    if (qrColorEditor) {
      const row = COLOR_ROWS.find((r) => r.key === activeColorKey);
      qrColorEditor.set(field[activeColorKey] ?? 0x0000, row?.label || '');
    }
    const contrast = core.contrastCheck(field.fgColor, field.bgColor);
    const warnEl = section.querySelector('[data-qr-contrast-warn]');
    if (warnEl) {
      if (contrast.inverted) {
        warnEl.hidden = false;
        warnEl.textContent = 'Aviso: los módulos son más claros que el fondo (QR invertido). Muchos lectores móviles requieren módulos oscuros sobre fondo claro.';
      } else if (contrast.lowContrast) {
        warnEl.hidden = false;
        warnEl.textContent = 'Aviso: contraste bajo entre módulos y fondo. Para lectura fiable usa negro (0x0000) sobre blanco (0xFFFF).';
      } else {
        warnEl.hidden = true;
      }
    }
    requestAnimationFrame(() => renderMiniPreview(section, field));
  }

  function applyQuickAlign(field, alignKey) {
    const m = core.computeMetrics(field);
    const w = m.width;
    const h = m.height;
    let x = field.x;
    let y = field.y;
    if (alignKey.includes('left')) x = 0;
    else if (alignKey.includes('right')) x = Math.max(0, SCREEN_W - w);
    else x = Math.max(0, Math.round((SCREEN_W - w) / 2));

    if (alignKey.includes('top')) y = 0;
    else if (alignKey.includes('bottom')) y = Math.max(0, SCREEN_H - h);
    else y = Math.max(0, Math.round((SCREEN_H - h) / 2));

    field.x = clamp(x, 0, SCREEN_W - 1);
    field.y = clamp(y, 0, SCREEN_H - 1);
  }

  function detectQuickAlign(field) {
    const m = core.computeMetrics(field);
    const isL = field.x === 0;
    const isR = field.x === Math.max(0, SCREEN_W - m.width);
    const isCX = Math.abs(field.x - Math.round((SCREEN_W - m.width) / 2)) <= 1;
    const isT = field.y === 0;
    const isB = field.y === Math.max(0, SCREEN_H - m.height);
    const isCY = Math.abs(field.y - Math.round((SCREEN_H - m.height) / 2)) <= 1;

    if (isT && isL) return 'top-left';
    if (isT && isCX) return 'top';
    if (isT && isR) return 'top-right';
    if (isCY && isL) return 'left';
    if (isCY && isCX) return 'center';
    if (isCY && isR) return 'right';
    if (isB && isL) return 'bottom-left';
    if (isB && isCX) return 'bottom';
    if (isB && isR) return 'bottom-right';
    return null;
  }

  function buildInspectorSection() {
    if (typeof document === 'undefined') return null;
    const existing = document.getElementById('qrInspectorSection');
    if (existing) return existing;
    const anchor = document.getElementById('rawTextControlsSection');
    if (!anchor) return null;

    injectStyles();

    const section = document.createElement('section');
    section.id = 'qrInspectorSection';
    section.className = 'inspector-section insp-wrap';
    section.hidden = true;
    section.innerHTML = `
      <div class="inspector-title">
        <h2 class="inspector-heading">
          <svg width="15" height="15" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" style="color:var(--orange); flex-shrink:0;" aria-hidden="true">
            <rect x="3" y="3" width="7" height="7"/><rect x="14" y="3" width="7" height="7"/><rect x="3" y="14" width="7" height="7"/>
            <path d="M14 14h3v3h-3zM18 14h3v3h-3zM14 18h3v3h-3zM18 18h3v3h-3z"/>
          </svg>
          <span>Inspector — Código QR</span>
        </h2>
        <span class="inspector-actions" style="display:inline-flex; gap:4px;">
          <button type="button" class="icon-button" data-qr-action="duplicate" title="Duplicar QR (Ctrl+D)">⧉</button>
          <button type="button" class="icon-button inspector-delete" data-qr-action="delete" title="Eliminar QR (Delete)">🗑</button>
        </span>
      </div>

      <div class="bar-nav-header insp-nav-header">
        <div class="bar-main-tabs" role="tablist">
          <button type="button" class="bar-main-tab insp-main-tab active" data-qr-main="design" role="tab">Diseño</button>
          <button type="button" class="bar-main-tab insp-main-tab" data-qr-main="data" role="tab">Datos</button>
          <button type="button" class="bar-main-tab insp-main-tab" data-qr-main="tech" role="tab">Técnico</button>
        </div>
        <div class="bar-sub-tabs" data-qr-subtabs role="tablist">
          <button type="button" class="bar-sub-tab insp-sub-tab active" data-qr-sub="shape" role="tab">Forma</button>
          <button type="button" class="bar-sub-tab insp-sub-tab" data-qr-sub="content" role="tab">Contenido</button>
          <button type="button" class="bar-sub-tab insp-sub-tab" data-qr-sub="appearance" role="tab">Apariencia</button>
          <button type="button" class="bar-sub-tab insp-sub-tab" data-qr-sub="placement" role="tab">Ubicación</button>
        </div>
      </div>

      <div class="insp-body">
        <!-- DISEÑO · FORMA -->
        <div class="insp-panel" data-qr-panel="design:shape">
          <div class="bar-card">
            <div class="bar-card-section-title">Tamaño de módulo (escala)</div>
            <div class="bar-visual-grid-4">
              <button type="button" class="bar-visual-btn" data-qr-scale="1"><strong>1×</strong><span class="qr-size-sub" data-qr-scale-px="1">—</span></button>
              <button type="button" class="bar-visual-btn" data-qr-scale="2"><strong>2×</strong><span class="qr-size-sub" data-qr-scale-px="2">—</span></button>
              <button type="button" class="bar-visual-btn" data-qr-scale="3"><strong>3×</strong><span class="qr-size-sub" data-qr-scale-px="3">—</span></button>
              <button type="button" class="bar-visual-btn" data-qr-scale="4"><strong>4×</strong><span class="qr-size-sub" data-qr-scale-px="4">—</span></button>
            </div>
            <div class="insp-note">En pantallas 320×170 se recomienda <strong>2×</strong> o <strong>3×</strong> para que cualquier cámara lo lea rápido.</div>
          </div>

          <div class="bar-card">
            <div class="bar-card-section-title">Corrección de errores (ECC)</div>
            <div class="bar-visual-grid-4">
              <button type="button" class="bar-visual-btn" data-qr-ecc="L"><strong>L</strong><span class="qr-size-sub">7%</span></button>
              <button type="button" class="bar-visual-btn" data-qr-ecc="M"><strong>M</strong><span class="qr-size-sub">15%</span></button>
              <button type="button" class="bar-visual-btn" data-qr-ecc="Q"><strong>Q</strong><span class="qr-size-sub">25%</span></button>
              <button type="button" class="bar-visual-btn" data-qr-ecc="H"><strong>H</strong><span class="qr-size-sub">30%</span></button>
            </div>
          </div>

          <div class="bar-card">
            <div class="bar-card-section-title">Margen claro (Quiet Zone)</div>
            <div class="bar-visual-grid-3">
              <button type="button" class="bar-visual-btn" data-qr-quiet="4"><strong>4 mód.</strong><span>Estándar</span></button>
              <button type="button" class="bar-visual-btn" data-qr-quiet="2"><strong>2 mód.</strong><span>Compacto</span></button>
              <button type="button" class="bar-visual-btn" data-qr-quiet="0"><strong>0</strong><span>Sin margen</span></button>
            </div>
          </div>

          <div class="bar-card">
            <div class="bar-card-section-title">Marco exterior</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-qr-frame="0"><span>Sin marco</span></button>
              <button type="button" class="bar-visual-btn" data-qr-frame="1"><span>Con marco (1 px)</span></button>
            </div>
          </div>
        </div>

        <!-- DISEÑO · CONTENIDO -->
        <div class="insp-panel" data-qr-panel="design:content" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Presets rápidos</div>
            <div class="qr-preset-grid">
              ${Object.values(core.PRESETS).map((p) =>
                `<button type="button" class="insp-chip" data-qr-preset="${p.id}" title="${escapeHtml(p.hint)}">${escapeHtml(p.label)}</button>`
              ).join('')}
            </div>
            <div class="insp-note" data-qr-preset-hint></div>

            <div class="qr-wifi-box" data-qr-wifi-box hidden>
              <div class="bar-card-section-title" style="margin:0;">Configuración Wi-Fi</div>
              <label class="field-label">Nombre de red (SSID)
                <input class="field-input" data-qr-wifi="ssid" type="text" maxlength="64" placeholder="MiRed_PLC" />
              </label>
              <div class="insp-two">
                <label class="field-label">Seguridad
                  <select class="field-input" data-qr-wifi="auth">
                    <option value="WPA">WPA / WPA2</option>
                    <option value="WEP">WEP</option>
                    <option value="nopass">Abierta (sin clave)</option>
                  </select>
                </label>
                <label class="field-label" data-qr-wifi-pass-wrap>Contraseña
                  <input class="field-input" data-qr-wifi="password" type="text" maxlength="64" placeholder="12345678" />
                </label>
              </div>
            </div>
          </div>

          <div class="bar-card">
            <div class="bar-card-header">
              <span class="bar-card-title" data-qr-text-title>Link / Texto del QR</span>
              <span class="insp-hint" data-qr-readout="bytes">0 / 213 B</span>
            </div>
            <textarea class="field-input qr-textarea" data-qr-prop="text" maxlength="271" placeholder="http://192.168.1.50"></textarea>
            <div class="insp-readouts insp-gap">
              <span>Versión <strong data-qr-readout="version">v1 (21×21)</strong></span>
              <span>Tamaño <strong data-qr-readout="pixels">—</strong></span>
            </div>
            <div class="qr-err-banner" data-qr-size-warn hidden></div>
          </div>
        </div>

        <!-- DISEÑO · APARIENCIA -->
        <div class="insp-panel" data-qr-panel="design:appearance" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Vista previa y colores</div>
            <div class="zone-host qr-case-body" data-qr-preview-body>
              <canvas class="qr-case-canvas" data-qr-preview-canvas width="66" height="66"></canvas>
              <div class="menu-case-legend" data-qr-legend></div>
            </div>
            <div class="qr-warn-banner" data-qr-contrast-warn hidden></div>
            <div class="insp-gap" data-qr-color-editor></div>
          </div>
        </div>

        <!-- DISEÑO · UBICACIÓN -->
        <div class="insp-panel" data-qr-panel="design:placement" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Página</div>
            <div class="insp-chip-row" data-qr-pages></div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Posición (px)</div>
            <div class="insp-two">
              <label class="field-label">X (px)
                <input class="field-input" data-qr-prop="x" type="number" min="0" max="319" />
              </label>
              <label class="field-label">Y (px)
                <input class="field-input" data-qr-prop="y" type="number" min="0" max="169" />
              </label>
            </div>
            <div class="insp-readouts">
              <span>Caja en TFT <strong data-qr-readout="bounds">—</strong></span>
            </div>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Alineación rápida en pantalla</div>
            <div class="align-matrix-3x3" data-qr-align-grid>
              <button type="button" class="matrix-cell" data-qr-align="top-left" title="Arriba Izquierda">↖</button>
              <button type="button" class="matrix-cell" data-qr-align="top" title="Arriba Centro">↑</button>
              <button type="button" class="matrix-cell" data-qr-align="top-right" title="Arriba Derecha">↗</button>
              <button type="button" class="matrix-cell" data-qr-align="left" title="Centro Izquierda">←</button>
              <button type="button" class="matrix-cell" data-qr-align="center" title="Centrar en pantalla">●</button>
              <button type="button" class="matrix-cell" data-qr-align="right" title="Centro Derecha">→</button>
              <button type="button" class="matrix-cell" data-qr-align="bottom-left" title="Abajo Izquierda">↙</button>
              <button type="button" class="matrix-cell" data-qr-align="bottom" title="Abajo Centro">↓</button>
              <button type="button" class="matrix-cell" data-qr-align="bottom-right" title="Abajo Derecha">↘</button>
            </div>
          </div>
        </div>

        <!-- DATOS -->
        <div class="insp-panel" data-qr-panel="data" hidden>
          <div class="bar-card">
            <div class="bar-card-section-title">Origen del contenido</div>
            <div class="bar-visual-grid-2">
              <button type="button" class="bar-visual-btn" data-qr-source="STATIC"><strong>Fijo</strong><span>Estático en Flash</span></button>
              <button type="button" class="bar-visual-btn" data-qr-source="VARIABLE"><strong>Variable</strong><span>Dinámico (char[])</span></button>
            </div>
            <div class="insp-note" data-qr-static-note>
              El texto/link se compila en Flash y se dibuja una sola vez en <code>jwplcUIPageEnter()</code>: <strong>0 RAM</strong> y no consume del cupo de 64 campos.
            </div>
          </div>

          <div class="bar-card" data-qr-var-card hidden>
            <div class="bar-card-section-title">Variable C++ vinculada <span class="insp-mode-chip">Arduino</span></div>
            <label class="field-label">Variable C++ (<code>char[]</code>)
              <input class="field-input code-input" data-qr-prop="variable" type="text" maxlength="32" />
            </label>
            <div class="bar-card-section-title insp-gap">Capacidad máxima del texto</div>
            <div class="qr-cap-chips">
              <button type="button" class="insp-chip" data-qr-maxlen="48">48 B</button>
              <button type="button" class="insp-chip" data-qr-maxlen="80">80 B</button>
              <button type="button" class="insp-chip" data-qr-maxlen="128">128 B</button>
              <button type="button" class="insp-chip" data-qr-maxlen="213">213 B</button>
            </div>
            <div class="insp-readouts insp-gap">
              <span>Tipo <code class="insp-type-chip" data-qr-readout="cppType">char[81]</code></span>
            </div>
            <div class="insp-note">Usa <code>JWPLC_UIQR</code> de <code>JWPLC_UI_Widgets.h</code> y sólo recalcula/redibuja en la TFT cuando cambia la cadena. El texto de <em>Contenido</em> se usa como valor inicial y vista previa en el lienzo.</div>
          </div>
        </div>

        <!-- TÉCNICO -->
        <div class="insp-panel" data-qr-panel="tech" hidden>
          <div class="bar-card">
            <label class="field-label">Nombre del objeto (sólo Designer)
              <input class="field-input" data-qr-prop="name" type="text" maxlength="24" />
            </label>
            <label class="field-label">ID C++
              <input class="field-input code-input" data-qr-prop="id" type="text" maxlength="32" />
            </label>
          </div>
          <div class="bar-card">
            <div class="bar-card-header">
              <span class="bar-card-title">Contrato / Uso en C++</span>
              <button type="button" class="icon-button" data-qr-action="copy" title="Copiar código">⧉</button>
            </div>
            <pre class="insp-contract" data-qr-contract style="font-size:10px; max-height:170px; overflow-y:auto;"></pre>
          </div>
          <div class="bar-card">
            <div class="bar-card-section-title">Recursos estimados</div>
            <div class="insp-readouts insp-readouts-grid">
              <span>Modo <strong data-qr-readout="mode">—</strong></span>
              <span>Matriz <strong data-qr-readout="matrix">—</strong></span>
              <span>Flash <strong data-qr-readout="flash">—</strong></span>
              <span>RAM <strong data-qr-readout="ram">—</strong></span>
            </div>
          </div>
        </div>
      </div>
    `;

    anchor.insertAdjacentElement('beforebegin', section);

    if (!qrColorEditor && window.JWPLCColorEditor) {
      qrColorEditor = window.JWPLCColorEditor.create({
        paletteAttr: 'data-qr-palette',
        onInput: (v) => mutate((f) => { f[activeColorKey] = v & 0xFFFF; }, false),
        onCommit: (v) => mutate((f) => { f[activeColorKey] = v & 0xFFFF; }, true)
      });
    }
    const colorHost = section.querySelector('[data-qr-color-editor]');
    if (colorHost && qrColorEditor && !colorHost.contains(qrColorEditor.el)) {
      colorHost.appendChild(qrColorEditor.el);
    }

    section.addEventListener('click', (e) => {
      const field = selectedQr();
      if (!field) return;

      const main = e.target.closest('[data-qr-main]');
      if (main) {
        tabs.main = main.dataset.qrMain;
        applyTabs(section);
        if (tabs.main === 'design' && tabs.sub === 'appearance') syncAppearance(section, field);
        return;
      }

      const sub = e.target.closest('[data-qr-sub]');
      if (sub) {
        tabs.sub = sub.dataset.qrSub;
        applyTabs(section);
        if (tabs.sub === 'appearance') syncAppearance(section, field);
        return;
      }

      const action = e.target.closest('[data-qr-action]')?.dataset.qrAction;
      if (action === 'duplicate') {
        editor()?.duplicateSelectedField?.();
        return;
      }
      if (action === 'delete') {
        editor()?.deleteSelectedField?.();
        return;
      }
      if (action === 'copy') {
        navigator.clipboard?.writeText(contractText(field)).catch(() => {});
        return;
      }

      const scaleBtn = e.target.closest('[data-qr-scale]');
      if (scaleBtn) {
        mutate((f) => { f.scale = core.clampScale(scaleBtn.dataset.qrScale); });
        return;
      }

      const eccBtn = e.target.closest('[data-qr-ecc]');
      if (eccBtn) {
        mutate((f) => { f.ecc = core.normalizeEcc(eccBtn.dataset.qrEcc); });
        return;
      }

      const quietBtn = e.target.closest('[data-qr-quiet]');
      if (quietBtn) {
        mutate((f) => { f.quietZone = core.clampQuietZone(quietBtn.dataset.qrQuiet); });
        return;
      }

      const frameBtn = e.target.closest('[data-qr-frame]');
      if (frameBtn) {
        mutate((f) => { f.frame = Number(frameBtn.dataset.qrFrame) === 1; });
        return;
      }

      const presetBtn = e.target.closest('[data-qr-preset]');
      if (presetBtn) {
        const key = presetBtn.dataset.qrPreset;
        const p = core.PRESETS[key];
        if (p) {
          mutate((f) => {
            f.preset = key;
            f.text = p.defaultText;
          });
        }
        return;
      }

      const sourceBtn = e.target.closest('[data-qr-source]');
      if (sourceBtn) {
        mutate((f) => {
          f.sourceMode = sourceBtn.dataset.qrSource === 'VARIABLE' ? 'VARIABLE' : 'STATIC';
        });
        return;
      }

      const maxLenBtn = e.target.closest('[data-qr-maxlen]');
      if (maxLenBtn) {
        mutate((f) => { f.maxLength = core.clampMaxLength(maxLenBtn.dataset.qrMaxlen); });
        return;
      }

      const alignBtn = e.target.closest('[data-qr-align]');
      if (alignBtn) {
        mutate((f) => { applyQuickAlign(f, alignBtn.dataset.qrAlign); });
        return;
      }

      const swatch = e.target.closest('[data-qr-swatch]');
      const legendRow = e.target.closest('[data-qr-legend-row]');
      if (swatch || legendRow) {
        const key = swatch ? swatch.dataset.qrSwatch : legendRow.dataset.qrLegendRow;
        if (key) {
          activeColorKey = key;
          syncAppearance(section, field);
        }
        return;
      }

      const pagesHost = section.querySelector('[data-qr-pages]');
      if (pagesHost && pagesHost.contains(e.target) && editor()?.handlePageChipHostClick?.(pagesHost, e, field)) {
        return;
      }
    });

    function handleWifiInput(commit) {
      const ssid = section.querySelector('[data-qr-wifi="ssid"]')?.value || '';
      const auth = section.querySelector('[data-qr-wifi="auth"]')?.value || 'WPA';
      const password = section.querySelector('[data-qr-wifi="password"]')?.value || '';
      mutate((f) => {
        f.preset = 'WIFI';
        f.text = core.formatWifiText({ auth, ssid, password });
      }, commit);
    }

    function applyPropInput(target, commit) {
      const prop = target.dataset.qrProp;
      if (!prop) return;
      mutate((f) => {
        if (prop === 'x') f.x = clamp(Math.trunc(Number(target.value) || 0), 0, SCREEN_W - 1);
        else if (prop === 'y') f.y = clamp(Math.trunc(Number(target.value) || 0), 0, SCREEN_H - 1);
        else if (prop === 'id') f.id = sanitizeSymbol(target.value, f.id);
        else if (prop === 'variable') f.variable = sanitizeSymbol(target.value, f.variable);
        else if (prop === 'text') f.text = target.value;
        else if (prop === 'name') f.name = target.value;
      }, commit);
    }

    section.addEventListener('input', (e) => {
      if (e.target.matches('[data-qr-prop]')) applyPropInput(e.target, false);
      else if (e.target.matches('[data-qr-wifi]')) handleWifiInput(false);
    });

    section.addEventListener('change', (e) => {
      if (e.target.matches('[data-qr-prop]')) applyPropInput(e.target, true);
      else if (e.target.matches('[data-qr-wifi]')) handleWifiInput(true);
    });

    return section;
  }

  function syncInspector() {
    const field = selectedQr();
    const section = document.getElementById('qrInspectorSection');
    if (!field || !section) return;

    ensureState(field);
    applyTabs(section);

    const m = core.computeMetrics(field);

    // Forma
    section.querySelectorAll('[data-qr-scale]').forEach((b) => {
      const s = Number(b.dataset.qrScale);
      b.classList.toggle('active', s === m.scale);
      const sub = b.querySelector('[data-qr-scale-px]');
      if (sub) {
        const px = (m.moduleCount + 2 * m.quietZone) * s + (field.frame && m.quietZone === 0 ? 2 : 0);
        sub.textContent = `${px}×${px}`;
      }
    });
    section.querySelectorAll('[data-qr-ecc]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrEcc === m.ecc);
    });
    section.querySelectorAll('[data-qr-quiet]').forEach((b) => {
      b.classList.toggle('active', Number(b.dataset.qrQuiet) === m.quietZone);
    });
    section.querySelectorAll('[data-qr-frame]').forEach((b) => {
      b.classList.toggle('active', (Number(b.dataset.qrFrame) === 1) === Boolean(field.frame));
    });

    // Contenido
    section.querySelectorAll('[data-qr-preset]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrPreset === field.preset);
    });
    const presetHint = section.querySelector('[data-qr-preset-hint]');
    if (presetHint) {
      presetHint.textContent = core.PRESETS[field.preset]?.hint || '';
    }
    const wifiBox = section.querySelector('[data-qr-wifi-box]');
    if (wifiBox) {
      const isWifi = field.preset === 'WIFI';
      wifiBox.hidden = !isWifi;
      if (isWifi) {
        const parsed = core.parseWifiText(field.text);
        const ssidIn = section.querySelector('[data-qr-wifi="ssid"]');
        const authSel = section.querySelector('[data-qr-wifi="auth"]');
        const passIn = section.querySelector('[data-qr-wifi="password"]');
        const passWrap = section.querySelector('[data-qr-wifi-pass-wrap]');
        if (ssidIn && document.activeElement !== ssidIn) ssidIn.value = parsed.ssid;
        if (authSel && document.activeElement !== authSel) authSel.value = parsed.auth;
        if (passIn && document.activeElement !== passIn) passIn.value = parsed.password;
        if (passWrap) passWrap.hidden = parsed.auth === 'nopass';
      }
    }

    const textTitle = section.querySelector('[data-qr-text-title]');
    if (textTitle) {
      textTitle.textContent = field.sourceMode === 'VARIABLE' ? 'Valor de prueba / inicial (lienzo)' : 'Link / Texto del QR';
    }
    const textInput = section.querySelector('[data-qr-prop="text"]');
    if (textInput && document.activeElement !== textInput) textInput.value = field.text || '';

    const bytesEl = section.querySelector('[data-qr-readout="bytes"]');
    const maxAllowed = field.sourceMode === 'VARIABLE' ? Math.min(field.maxLength, m.maxCapacity) : m.maxCapacity;
    if (bytesEl) bytesEl.textContent = `${m.byteLength} / ${maxAllowed} B`;

    const verEl = section.querySelector('[data-qr-readout="version"]');
    if (verEl) verEl.textContent = m.ok ? `v${m.version} (${m.moduleCount}×${m.moduleCount})` : 'Error';

    const pxEl = section.querySelector('[data-qr-readout="pixels"]');
    if (pxEl) pxEl.textContent = `${m.width} × ${m.height} px`;

    const sizeWarn = section.querySelector('[data-qr-size-warn]');
    if (sizeWarn) {
      const msgs = [];
      if (!m.ok) msgs.push(m.encoded?.error || 'Texto inválido para QR.');
      if (field.sourceMode === 'VARIABLE' && m.byteLength > field.maxLength) {
        msgs.push(`El texto de prueba (${m.byteLength} B) supera la capacidad declarada (${field.maxLength} B).`);
      }
      if (!m.fitsScreen) {
        msgs.push(`El QR (${m.width}×${m.height} px) supera la pantalla ST7789 (320×170 px): reduce la escala o el texto.`);
      } else if (!m.inBounds) {
        msgs.push(`El QR sale parcialmente de la pantalla desde (${field.x}, ${field.y}).`);
      }
      sizeWarn.hidden = msgs.length === 0;
      sizeWarn.textContent = msgs.join(' ');
    }

    // Apariencia
    if (tabs.main === 'design' && tabs.sub === 'appearance') {
      syncAppearance(section, field);
    }

    // Ubicación
    const pagesHost = section.querySelector('[data-qr-pages]');
    if (pagesHost && editor()?.renderPageChipsHost) {
      editor().renderPageChipsHost(pagesHost, field);
    }
    const xIn = section.querySelector('[data-qr-prop="x"]');
    const yIn = section.querySelector('[data-qr-prop="y"]');
    if (xIn && document.activeElement !== xIn) xIn.value = String(Math.trunc(field.x));
    if (yIn && document.activeElement !== yIn) yIn.value = String(Math.trunc(field.y));
    const boundsEl = section.querySelector('[data-qr-readout="bounds"]');
    if (boundsEl) boundsEl.textContent = `${m.width} × ${m.height} px`;

    const matchedAlign = detectQuickAlign(field);
    section.querySelectorAll('[data-qr-align]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrAlign === matchedAlign);
    });

    // Datos
    section.querySelectorAll('[data-qr-source]').forEach((b) => {
      b.classList.toggle('active', b.dataset.qrSource === field.sourceMode);
    });
    const staticNote = section.querySelector('[data-qr-static-note]');
    const varCard = section.querySelector('[data-qr-var-card]');
    if (staticNote) staticNote.hidden = field.sourceMode === 'VARIABLE';
    if (varCard) varCard.hidden = field.sourceMode !== 'VARIABLE';

    const varIn = section.querySelector('[data-qr-prop="variable"]');
    if (varIn && document.activeElement !== varIn) varIn.value = field.variable || '';
    section.querySelectorAll('[data-qr-maxlen]').forEach((b) => {
      b.classList.toggle('active', Number(b.dataset.qrMaxlen) === field.maxLength);
    });
    const cppTypeEl = section.querySelector('[data-qr-readout="cppType"]');
    if (cppTypeEl) cppTypeEl.textContent = `char[${field.maxLength + 1}]`;

    // Técnico
    const nameIn = section.querySelector('[data-qr-prop="name"]');
    if (nameIn && document.activeElement !== nameIn) nameIn.value = field.name || '';
    const idIn = section.querySelector('[data-qr-prop="id"]');
    if (idIn && document.activeElement !== idIn) idIn.value = field.id || '';
    const contractEl = section.querySelector('[data-qr-contract]');
    if (contractEl) contractEl.textContent = contractText(field);

    const modeEl = section.querySelector('[data-qr-readout="mode"]');
    if (modeEl) modeEl.textContent = field.sourceMode === 'VARIABLE' ? 'Dinámico' : 'Estático';
    const matEl = section.querySelector('[data-qr-readout="matrix"]');
    if (matEl) matEl.textContent = `${m.moduleCount}×${m.moduleCount}`;
    const flashEl = section.querySelector('[data-qr-readout="flash"]');
    if (flashEl) flashEl.textContent = field.sourceMode === 'STATIC' ? `${m.packedBytes} B` : 'JWPLC_UIQR';
    const ramEl = section.querySelector('[data-qr-readout="ram"]');
    if (ramEl) ramEl.textContent = field.sourceMode === 'STATIC' ? '0 B' : `~${(field.maxLength + 1) * 2 + 420} B`;
  }

  window.JWPLCHMIQR = {
    createDefault,
    ensureState,
    computeBounds,
    paint,
    contractText,
    buildInspectorSection,
    syncInspector
  };

  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', () => buildInspectorSection());
    } else {
      buildInspectorSection();
    }
    window.addEventListener('jwplc:editor-refresh', () => syncInspector());
    window.addEventListener('jwplc:themechange', () => syncInspector());
  }
})();

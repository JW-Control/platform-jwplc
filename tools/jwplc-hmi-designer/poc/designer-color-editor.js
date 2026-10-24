(() => {
  'use strict';

  // Alpha15 · Editor de color de la Apariencia (campos, MENU y BAR).
  //
  // Reemplaza el selector nativo (<input type="color">): su ventana la ubica
  // el navegador, tapa las filas de la leyenda y se cierra al cambiar de fila.
  // Este editor va fijo dentro de la tarjeta, bajo la leyenda: se elige la fila
  // (texto o cuadro) y se edita aquí sin abrir ni cerrar nada.
  //
  //   const editor = JWPLCColorEditor.create({
  //     paletteAttr: 'data-field-palette',      // atributo de las pastillas rápidas
  //     onInput: (v565) => {},                  // mientras se arrastra (~25/s)
  //     onCommit: (v565) => {}                  // al soltar / Enter / pastilla
  //   });
  //   host.appendChild(editor.el);
  //   editor.set(v565, 'Barra');                // color y nombre de lo que se edita
  //
  // Todo en RGB565: lo que se ve es lo que dibuja la TFT.

  const PALETTE = [
    [0x0000, 'Negro'], [0xFFFF, 'Blanco'], [0xF800, 'Rojo'], [0x07E0, 'Verde'],
    [0x001F, 'Azul'], [0x07FF, 'Cian'], [0xFFE0, 'Amarillo'], [0xFD20, 'Naranja'],
    [0x8410, 'Gris'], [0x4208, 'Gris oscuro'], [0xF81F, 'Magenta'], [0x18C3, 'Pista']
  ];
  const LIVE_MS = 40;

  const to888 = (v) => ({
    r: Math.round(((v >> 11) & 0x1F) * 255 / 31),
    g: Math.round(((v >> 5) & 0x3F) * 255 / 63),
    b: Math.round((v & 0x1F) * 255 / 31)
  });
  const to565 = (r, g, b) => (((r >> 3) & 0x1F) << 11) | (((g >> 2) & 0x3F) << 5) | ((b >> 3) & 0x1F);
  const css = (v) => { const c = to888(v); return `rgb(${c.r}, ${c.g}, ${c.b})`; };
  const hexCss = (v) => { const c = to888(v); return `#${[c.r, c.g, c.b].map((n) => n.toString(16).padStart(2, '0')).join('')}`; };
  const hex565 = (v) => `0x${(v & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;

  function rgbToHsv(r, g, b) {
    r /= 255; g /= 255; b /= 255;
    const max = Math.max(r, g, b);
    const min = Math.min(r, g, b);
    const d = max - min;
    let h = 0;
    if (d) {
      if (max === r) h = ((g - b) / d) % 6;
      else if (max === g) h = (b - r) / d + 2;
      else h = (r - g) / d + 4;
      h *= 60;
      if (h < 0) h += 360;
    }
    return { h, s: max ? d / max : 0, v: max };
  }

  function hsvToRgb(h, s, v) {
    const c = v * s;
    const x = c * (1 - Math.abs(((h / 60) % 2) - 1));
    const m = v - c;
    const [r, g, b] = h < 60 ? [c, x, 0] : h < 120 ? [x, c, 0] : h < 180 ? [0, c, x]
      : h < 240 ? [0, x, c] : h < 300 ? [x, 0, c] : [c, 0, x];
    return { r: Math.round((r + m) * 255), g: Math.round((g + m) * 255), b: Math.round((b + m) * 255) };
  }

  function parseColor(text) {
    const t = String(text || '').trim();
    let m = /^#?([0-9a-f]{6})$/i.exec(t);
    if (m) {
      const n = parseInt(m[1], 16);
      return to565((n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF);
    }
    m = /^0x([0-9a-f]{1,4})$/i.exec(t);
    if (m) return parseInt(m[1], 16) & 0xFFFF;
    return null;
  }

  function create(options = {}) {
    const paletteAttr = options.paletteAttr || 'data-color-palette';
    const el = document.createElement('div');
    el.className = 'color-editor';
    el.innerHTML = `
      <div class="color-editor-head">
        <span>Editando <strong data-ce-target>—</strong></span>
        <span class="color-editor-swatch" data-ce-preview></span>
      </div>
      <div class="color-editor-sv" data-ce-sv><div class="color-editor-sv-knob" data-ce-sv-knob></div></div>
      <div class="color-editor-hue" data-ce-hue><div class="color-editor-hue-knob" data-ce-hue-knob></div></div>
      <div class="color-editor-fields">
        <label>HEX <input type="text" spellcheck="false" maxlength="7" data-ce-hex /></label>
        <label>RGB565 <input type="text" spellcheck="false" maxlength="6" data-ce-565 /></label>
      </div>
      <div class="insp-swatch-row color-editor-palette">${PALETTE.map(([v, name]) =>
        `<button type="button" class="insp-swatch" ${paletteAttr}="${v}" title="${name} · ${hex565(v)}" style="background:${css(v)}"></button>`).join('')}</div>`;

    const sv = el.querySelector('[data-ce-sv]');
    const hue = el.querySelector('[data-ce-hue]');
    const svKnob = el.querySelector('[data-ce-sv-knob]');
    const hueKnob = el.querySelector('[data-ce-hue-knob]');
    const hexInput = el.querySelector('[data-ce-hex]');
    const i565 = el.querySelector('[data-ce-565]');
    const preview = el.querySelector('[data-ce-preview]');
    const target = el.querySelector('[data-ce-target]');

    let value = 0;
    let hsv = { h: 0, s: 0, v: 0 };
    let lastLive = 0;
    let dragging = false; // mientras se arrastra, el inspector no reposiciona el cursor

    function paint(skipInputs = false) {
      sv.style.background = `linear-gradient(to top, #000, transparent), linear-gradient(to right, #fff, hsl(${hsv.h}, 100%, 50%))`;
      svKnob.style.left = `${hsv.s * 100}%`;
      svKnob.style.top = `${(1 - hsv.v) * 100}%`;
      hueKnob.style.left = `${(hsv.h / 360) * 100}%`;
      preview.style.background = css(value);
      if (!skipInputs) {
        if (document.activeElement !== hexInput) hexInput.value = hexCss(value);
        if (document.activeElement !== i565) i565.value = hex565(value);
      }
      el.querySelectorAll(`[${paletteAttr}]`).forEach((b) =>
        b.classList.toggle('active', (Number(b.getAttribute(paletteAttr)) & 0xFFFF) === value));
    }

    function set(v565, name) {
      if (name !== undefined) target.textContent = name;
      if (dragging) return;
      value = Number(v565) & 0xFFFF;
      // Conserva el tono al editar grises (s = 0) o negro (v = 0).
      const c = to888(value);
      const next = rgbToHsv(c.r, c.g, c.b);
      hsv = { h: next.s && next.v ? next.h : hsv.h, s: next.v ? next.s : hsv.s, v: next.v };
      paint();
    }

    function emit(commit) {
      const rgb = hsvToRgb(hsv.h, hsv.s, hsv.v);
      value = to565(rgb.r, rgb.g, rgb.b);
      paint();
      if (commit) options.onCommit?.(value);
      else {
        const now = performance.now();
        if (now - lastLive >= LIVE_MS) { lastLive = now; options.onInput?.(value); }
      }
    }

    function drag(area, apply) {
      area.addEventListener('pointerdown', (event) => {
        event.preventDefault();
        area.setPointerCapture(event.pointerId);
        dragging = true;
        const move = (e) => {
          const r = area.getBoundingClientRect();
          apply(Math.min(1, Math.max(0, (e.clientX - r.left) / r.width)), Math.min(1, Math.max(0, (e.clientY - r.top) / r.height)));
          emit(false);
        };
        const up = (e) => {
          area.removeEventListener('pointermove', move);
          area.removeEventListener('pointerup', up);
          area.removeEventListener('pointercancel', up);
          move(e);
          dragging = false;
          emit(true);
        };
        area.addEventListener('pointermove', move);
        area.addEventListener('pointerup', up);
        area.addEventListener('pointercancel', up);
        move(event);
      });
    }

    drag(sv, (x, y) => { hsv.s = x; hsv.v = 1 - y; });
    drag(hue, (x) => { hsv.h = Math.min(359.9, x * 360); });

    function bindText(input) {
      input.addEventListener('input', () => {
        const v = parseColor(input.value);
        if (v === null) return;
        set(v);
        paint(true);
        options.onInput?.(value);
      });
      const commit = () => {
        const v = parseColor(input.value);
        if (v !== null) { set(v); options.onCommit?.(value); } else paint();
      };
      input.addEventListener('change', commit);
      input.addEventListener('keydown', (e) => { if (e.key === 'Enter') { e.preventDefault(); input.blur(); } });
    }
    bindText(hexInput);
    bindText(i565);

    el.addEventListener('click', (event) => {
      const pill = event.target.closest(`[${paletteAttr}]`);
      if (!pill) return;
      event.stopPropagation(); // el inspector no debe procesar la pastilla otra vez
      set(Number(pill.getAttribute(paletteAttr)));
      options.onCommit?.(value);
    });

    return { el, set, get: () => value };
  }

  window.JWPLCColorEditor = { create, parseColor, PALETTE };
})();

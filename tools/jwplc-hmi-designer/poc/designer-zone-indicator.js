(() => {
  'use strict';

  // Alpha15 · Apariencia A1: indicador de la zona que pinta el color en
  // edición (TEXT/VALUE/BOOL y MENU). Reemplaza las flechas largas de la
  // leyenda (A15_7_APARIENCIA_A1_ANALISIS_20261018.md):
  //
  //   1. contorno de doble trazo (negro afuera, acento adentro) a OUTSET px de
  //      la zona; con `inset` se dibuja hacia adentro (fondo, en el margen);
  //   2. marcador ▶ en un canal fijo a la izquierda del mini-modelo, a la
  //      altura de la zona, y una banda en la mini-regla de arriba con su ancho.
  //
  // Todo va en un SVG encima: los píxeles del mini-modelo no cambian, y el
  // indicador no usa ningún color de la paleta RGB565 de la HMI (el doble
  // trazo se distingue también sobre una fila de edición naranja del MENU).
  //
  // draw(body, canvas, rects)
  //   body    contenedor (position: relative) del canvas; clase zone-host
  //   canvas  mini-modelo; coordenadas lógicas en dataset.logicalW/H
  //   rects   [{ x, y, w, h, inset? }] en píxeles lógicos; vacío = sin indicador
  //
  // Las posiciones salen de getBoundingClientRect(): coinciden con cualquier
  // zoom de Windows o escala CSS del mini-modelo.

  const NS = 'http://www.w3.org/2000/svg';
  const OUTSET = 2;     // px CSS entre la zona y el trazo de acento
  const CHANNEL = 12;   // canal izquierdo del ▶ (igual que el margen del canvas)
  const RULER = 8;      // mini-regla superior

  function svgEl(name, attrs) {
    const el = document.createElementNS(NS, name);
    Object.entries(attrs).forEach(([k, v]) => el.setAttribute(k, String(v)));
    return el;
  }

  function strokeRect(svg, x, y, w, h, cls) {
    svg.appendChild(svgEl('rect', {
      x: x + 0.5, y: y + 0.5, width: Math.max(1, w - 1), height: Math.max(1, h - 1), class: cls
    }));
  }

  function draw(body, canvas, rects) {
    if (!body || !canvas) return;
    let svg = body.querySelector(':scope > svg.zone-indicator');
    if (!svg) {
      svg = svgEl('svg', { class: 'zone-indicator', 'aria-hidden': 'true' });
      body.appendChild(svg);
    }
    svg.replaceChildren();
    const bodyRect = body.getBoundingClientRect();
    const canvasRect = canvas.getBoundingClientRect();
    if (!bodyRect.width || !canvasRect.width) return;
    svg.setAttribute('width', String(bodyRect.width));
    svg.setAttribute('height', String(bodyRect.height));
    svg.setAttribute('viewBox', `0 0 ${bodyRect.width} ${bodyRect.height}`);

    const logicalW = Number(canvas.dataset.logicalW) || canvas.width;
    const logicalH = Number(canvas.dataset.logicalH) || canvas.height;
    const sx = canvasRect.width / logicalW;
    const sy = canvasRect.height / logicalH;
    const ox = canvasRect.left - bodyRect.left;
    const oy = canvasRect.top - bodyRect.top;

    // Mini-regla: siempre visible, con la banda de la zona activa encima.
    svg.appendChild(svgEl('line', {
      x1: ox, y1: oy - 3.5, x2: ox + canvasRect.width, y2: oy - 3.5, class: 'zone-ruler'
    }));

    const list = (rects || []).filter((r) => r && r.w > 0 && r.h > 0);
    if (!list.length) return;

    let minX = Infinity; let minY = Infinity; let maxX = -Infinity; let maxY = -Infinity;
    list.forEach((r) => {
      const x = ox + r.x * sx;
      const y = oy + r.y * sy;
      const w = r.w * sx;
      const h = r.h * sy;
      minX = Math.min(minX, x); minY = Math.min(minY, y);
      maxX = Math.max(maxX, x + w); maxY = Math.max(maxY, y + h);
      const d = r.inset ? -(OUTSET - 1) : OUTSET;
      // Negro del lado de la HMI, acento del lado de la zona.
      const outer = r.inset ? d : d + 1;
      const inner = r.inset ? d - 1 : d;
      strokeRect(svg, x - outer, y - outer, w + 2 * outer, h + 2 * outer, 'zone-outline-dark');
      strokeRect(svg, x - inner, y - inner, w + 2 * inner, h + 2 * inner, 'zone-outline-accent');
    });

    // Banda de la regla con el ancho de la zona.
    svg.appendChild(svgEl('rect', {
      x: minX, y: oy - RULER + 1, width: Math.max(2, maxX - minX), height: 3, class: 'zone-ruler-band'
    }));

    // ▶ a la altura de la zona, en el canal izquierdo.
    const cy = Math.max(oy + 4, Math.min(oy + canvasRect.height - 4, (minY + maxY) / 2));
    const tipX = ox - 3;
    const baseX = Math.max(1, ox - CHANNEL + 2);
    svg.appendChild(svgEl('path', {
      d: `M ${baseX} ${cy - 4.5} L ${tipX} ${cy} L ${baseX} ${cy + 4.5} Z`,
      class: 'zone-marker'
    }));
  }

  window.JWPLCZoneIndicator = { draw, CHANNEL, RULER };
})();

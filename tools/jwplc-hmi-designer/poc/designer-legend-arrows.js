(() => {
  'use strict';

  // A15-7: flechas de la leyenda de colores (Apariencia de campos y MENU).
  //
  // Cada fila de la leyenda (a la derecha del mini-modelo) se une con la zona
  // que colorea mediante una flecha con codos de 90°:
  //   fila -> izquierda hasta su carril -> vertical hasta la zona -> zona.
  // La fila del color en edición va resaltada (naranja, más gruesa) y su zona
  // se enmarca con línea discontinua en el mini-modelo.
  //
  // draw(body, canvas, items, placement)
  //   body      contenedor (position: relative) con el canvas y la leyenda
  //   canvas    mini-modelo; coordenadas lógicas = framebuffer recortado
  //             (dataset.logicalW/H)
  //   items     [{ row: Element, zone: {x,y,w,h}, tipY?, active }] en píxeles
  //             lógicos; la flecha llega al borde de la zona a la altura tipY
  //             (por defecto, media altura)
  //   placement 'right': leyenda a la derecha, la flecha entra por el borde
  //             derecho de la zona; 'below': leyenda debajo, los carriles van
  //             por la izquierda y la flecha entra por el borde izquierdo.

  const NS = 'http://www.w3.org/2000/svg';
  const LANE_GAP = 5;
  const RADIUS = 4;

  function svgEl(name, attrs) {
    const el = document.createElementNS(NS, name);
    Object.entries(attrs).forEach(([k, v]) => el.setAttribute(k, String(v)));
    return el;
  }

  // Codos redondeados: tramos horizontales/verticales unidos por un cuarto de
  // curva (Q) de radio RADIUS, acotado a la mitad del tramo más corto.
  function elbowPath(points) {
    let d = `M ${points[0].x} ${points[0].y}`;
    for (let i = 1; i < points.length - 1; i++) {
      const p0 = points[i - 1];
      const p1 = points[i];
      const p2 = points[i + 1];
      const inLen = Math.hypot(p1.x - p0.x, p1.y - p0.y);
      const outLen = Math.hypot(p2.x - p1.x, p2.y - p1.y);
      const r = Math.min(RADIUS, inLen / 2, outLen / 2);
      if (r < 0.5) { d += ` L ${p1.x} ${p1.y}`; continue; }
      const a = { x: p1.x - (p1.x - p0.x) / inLen * r, y: p1.y - (p1.y - p0.y) / inLen * r };
      const b = { x: p1.x + (p2.x - p1.x) / outLen * r, y: p1.y + (p2.y - p1.y) / outLen * r };
      d += ` L ${a.x} ${a.y} Q ${p1.x} ${p1.y} ${b.x} ${b.y}`;
    }
    const last = points[points.length - 1];
    return `${d} L ${last.x} ${last.y}`;
  }

  // Ancho del canal entre el mini-modelo y la leyenda para n carriles.
  function gutterFor(count) {
    return 12 + Math.max(1, count) * LANE_GAP;
  }

  function draw(body, canvas, items, placement = 'right') {
    if (!body || !canvas) return;
    let svg = body.querySelector(':scope > svg.legend-arrows');
    if (!svg) {
      svg = svgEl('svg', { class: 'legend-arrows', 'aria-hidden': 'true' });
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
    const toX = (x) => ox + x * sx;
    const toY = (y) => oy + y * sy;
    const canvasRight = ox + canvasRect.width;
    const below = placement === 'below';

    const defs = svgEl('defs', {});
    ['idle', 'active'].forEach((kind) => {
      const marker = svgEl('marker', {
        id: `legend-arrow-${kind}-${body.dataset.arrowId || (body.dataset.arrowId = Math.random().toString(36).slice(2, 8))}`,
        viewBox: '0 0 8 8', refX: 7, refY: 4, markerWidth: kind === 'active' ? 7 : 6, markerHeight: kind === 'active' ? 7 : 6, orient: 'auto-start-reverse'
      });
      marker.appendChild(svgEl('path', { d: 'M 0 0 L 8 4 L 0 8 z', class: `legend-arrow-head ${kind}` }));
      defs.appendChild(marker);
    });
    svg.appendChild(defs);

    const tipOf = (item) => ({
      x: toX(below ? item.zone.x : item.zone.x + item.zone.w),
      y: toY(item.tipY ?? item.zone.y + item.zone.h / 2)
    });

    // Carriles. A la derecha: el más cercano al mini-modelo para la zona más
    // alta. Debajo: la fila más baja usa el carril más exterior, así su tramo
    // horizontal no cruza los verticales de las filas de arriba.
    const ordered = items
      .map((item, index) => ({ index, key: below ? index : tipOf(item).y }))
      .sort((a, b) => a.key - b.key);
    const lanes = new Map();
    ordered.forEach((entry, laneIndex) => lanes.set(entry.index, laneIndex));

    // Activa al final: queda por encima de las demás.
    const drawOrder = items.map((item, index) => ({ item, index })).sort((a, b) => Number(a.item.active) - Number(b.item.active));
    drawOrder.forEach(({ item, index }) => {
      const rowRect = item.row.getBoundingClientRect();
      if (!rowRect.height) return;
      const kind = item.active ? 'active' : 'idle';
      const start = { x: rowRect.left - bodyRect.left - 2, y: rowRect.top - bodyRect.top + rowRect.height / 2 };
      const tip = tipOf(item);
      const laneX = below
        ? Math.max(2, ox - 6 - lanes.get(index) * LANE_GAP)
        : Math.min(start.x - 4, canvasRight + 6 + lanes.get(index) * LANE_GAP);
      const points = !below && Math.abs(start.y - tip.y) < 1.5
        ? [start, { x: tip.x, y: start.y }]
        : [start, { x: laneX, y: start.y }, { x: laneX, y: tip.y }, tip];
      svg.appendChild(svgEl('path', {
        d: elbowPath(points),
        class: `legend-arrow ${kind}`,
        'marker-end': `url(#legend-arrow-${kind}-${body.dataset.arrowId})`
      }));
      if (item.active && item.zone) {
        svg.appendChild(svgEl('rect', {
          x: toX(item.zone.x) - 1.5,
          y: toY(item.zone.y) - 1.5,
          width: Math.max(2, item.zone.w * sx) + 3,
          height: Math.max(2, item.zone.h * sy) + 3,
          rx: 2,
          class: 'legend-zone'
        }));
      }
    });
  }

  window.JWPLCLegendArrows = { draw, gutterFor };
})();

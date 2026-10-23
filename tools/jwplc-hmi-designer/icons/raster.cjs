// Rasterizador de la biblioteca de iconos (Alpha16).
//
// Cada icono se dibuja con primitivas en una cuadrícula de diseño de 100 x 100
// unidades y se muestrea en el centro de cada píxel (sin antialias, como la
// TFT): el resultado es un índice de "rol" por píxel. Los roles se convierten
// después en índices de paleta (0 = transparente) y cada estado del icono da
// su color a cada rol.
'use strict';

function createGrid(size) {
  const roles = new Array(size * size).fill('.');
  const unit = 100 / size;
  // Ancho de trazo de contorno: 1 px hasta 39 px, 2 px a 48, 3 px a 64.
  const outlinePx = Math.max(1, Math.floor(size / 20));
  const g = {
    size,
    unit,
    o: outlinePx * unit,
    px: (n) => n * unit,
    roles,
    // Pinta `role` en cada píxel cuyo centro cumple `inside(x, y)` (unidades de diseño).
    paint(role, inside) {
      for (let j = 0; j < size; j += 1) {
        const y = (j + 0.5) * unit;
        for (let i = 0; i < size; i += 1) {
          const x = (i + 0.5) * unit;
          if (inside(x, y)) roles[j * size + i] = role;
        }
      }
      return g;
    },
    circle(role, cx, cy, r) {
      return g.paint(role, (x, y) => (x - cx) ** 2 + (y - cy) ** 2 <= r * r);
    },
    ring(role, cx, cy, r, w) {
      return g.paint(role, (x, y) => {
        const d = Math.hypot(x - cx, y - cy);
        return d <= r && d > r - w;
      });
    },
    // Arco de anillo entre a0 y a1 grados (0 = derecha, sentido horario en pantalla).
    arc(role, cx, cy, r, w, a0, a1) {
      return g.paint(role, (x, y) => {
        const d = Math.hypot(x - cx, y - cy);
        if (d > r || d <= r - w) return false;
        let a = Math.atan2(y - cy, x - cx) * 180 / Math.PI;
        if (a < a0) a += 360;
        return a >= a0 && a <= a1;
      });
    },
    ellipse(role, cx, cy, rx, ry) {
      return g.paint(role, (x, y) => ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1);
    },
    rect(role, x0, y0, w, h, radius = 0) {
      return g.paint(role, (x, y) => {
        if (x < x0 || x > x0 + w || y < y0 || y > y0 + h) return false;
        if (radius <= 0) return true;
        const qx = Math.max(x0 + radius - x, 0, x - (x0 + w - radius));
        const qy = Math.max(y0 + radius - y, 0, y - (y0 + h - radius));
        return qx * qx + qy * qy <= radius * radius;
      });
    },
    // Rectángulo con contorno: relleno `fill` y borde `stroke` de ancho g.o (hacia adentro).
    box(stroke, fill, x0, y0, w, h, radius = 0) {
      g.rect(stroke, x0, y0, w, h, radius);
      return g.rect(fill, x0 + g.o, y0 + g.o, w - 2 * g.o, h - 2 * g.o, Math.max(0, radius - g.o));
    },
    disc(stroke, fill, cx, cy, r) {
      g.circle(stroke, cx, cy, r);
      return g.circle(fill, cx, cy, r - g.o);
    },
    poly(role, pts) {
      return g.paint(role, (x, y) => {
        let inside = false;
        for (let a = 0, b = pts.length - 1; a < pts.length; b = a, a += 1) {
          const [xa, ya] = pts[a];
          const [xb, yb] = pts[b];
          if ((ya > y) !== (yb > y) && x < ((xb - xa) * (y - ya)) / (yb - ya) + xa) inside = !inside;
        }
        return inside;
      });
    },
    line(role, x1, y1, x2, y2, w) {
      const half = w / 2;
      return g.paint(role, (x, y) => {
        const dx = x2 - x1;
        const dy = y2 - y1;
        const len2 = dx * dx + dy * dy;
        const t = len2 ? Math.max(0, Math.min(1, ((x - x1) * dx + (y - y1) * dy) / len2)) : 0;
        return Math.hypot(x - (x1 + t * dx), y - (y1 + t * dy)) <= half;
      });
    },
    polyline(role, pts, w, closed = false) {
      for (let k = 0; k < pts.length - 1; k += 1) g.line(role, ...pts[k], ...pts[k + 1], w);
      if (closed) g.line(role, ...pts[pts.length - 1], ...pts[0], w);
      return g;
    },
    // Polígono con contorno: relleno + trazo centrado en los bordes.
    shape(stroke, fill, pts) {
      g.poly(fill, pts);
      return g.polyline(stroke, pts, g.o * 1.5, true);
    },
    rotate(pts, cx, cy, deg) {
      const a = deg * Math.PI / 180;
      const c = Math.cos(a);
      const s = Math.sin(a);
      return pts.map(([x, y]) => [cx + (x - cx) * c - (y - cy) * s, cy + (x - cx) * s + (y - cy) * c]);
    }
  };
  return g;
}

module.exports = { createGrid };

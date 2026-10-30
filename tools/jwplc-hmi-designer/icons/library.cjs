// Biblioteca de iconos con estado del HMI Designer (Alpha16): fuente.
'use strict';

const C = {
  white: 0xFFFF, light: 0xC618, mid: 0x8410, gray: 0x632C, dark: 0x4208, darker: 0x2124, black: 0x0000,
  green: 0x07E0, greenDim: 0x0320, greenHi: 0xAFF5,
  red: 0xF800, redDim: 0x6000, redHi: 0xFDB6,
  yellow: 0xFFE0, yellowDim: 0x6300, yellowHi: 0xFFF6,
  amber: 0xFD20, orange: 0xFC00,
  blue: 0x041F, blueDim: 0x0110, water: 0x2D7F, cyan: 0x07FF, cyanHi: 0xCFFF, steel: 0x9CF3
};

const RUN3 = [
  { name: 'Parado', colors: { S: C.gray } },
  { name: 'En marcha', colors: { S: C.green } },
  { name: 'Falla', colors: { S: C.red } }
];

function withColors(states, shared) {
  return states.map((state) => ({ name: state.name, colors: { ...shared, ...state.colors } }));
}

const CATEGORIES = [
  { id: 'motores', name: 'Motores' },
  { id: 'bombas', name: 'Bombas y ventiladores' },
  { id: 'pilotos', name: 'Pilotos y señales' },
  { id: 'valvulas', name: 'Válvulas' },
  { id: 'equipos', name: 'Tanques y equipos' },
  { id: 'sensores', name: 'Sensores' },
  { id: 'alarmas', name: 'Alarmas' },
  { id: 'botones', name: 'Botones y selectores' }
];

const ICONS = [
  // ------------------------------------------------------------- Motores
  {
    id: 'motores/motor', name: 'Motor', category: 'motores', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, T: C.black, R: C.dark } },
      { name: 'En marcha', intervalMs: 120, frameCount: 4, colors: { O: C.light, S: C.green, T: C.black, R: C.greenHi } },
      { name: 'Falla', colors: { O: C.light, S: C.red, T: C.black, R: C.redHi } }
    ],
    draw(g, st, f = 0) {
      g.disc('O', 'S', 50, 50, 46);
      g.polyline('T', [[29, 70], [29, 31], [50, 56], [71, 31], [71, 70]], Math.max(g.px(2), 9));
      if (st === 1) {
        const deg = f * 45;
        const pts = [[50, 8], [50, 16]];
        [deg, deg + 90, deg + 180, deg + 270].forEach((d) => {
          const [[x1, y1], [x2, y2]] = g.rotate(pts, 50, 50, d);
          g.line('R', x1, y1, x2, y2, Math.max(g.px(2), 7));
        });
      }
    }
  },
  {
    id: 'motores/motor_dos_sentidos', name: 'Motor dos sentidos', category: 'motores', sizes: [24, 32, 48],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, T: C.black, A: C.gray } },
      { name: 'Adelante', colors: { O: C.light, S: C.green, T: C.black, A: C.white } },
      { name: 'Atrás', colors: { O: C.light, S: C.amber, T: C.black, A: C.white } }
    ],
    draw(g, st) {
      g.disc('O', 'S', 50, 56, 40);
      g.polyline('T', [[33, 73], [33, 41], [50, 61], [67, 41], [67, 73]], Math.max(g.px(2), 8));
      const w = Math.max(g.px(2), 7);
      if (st === 1) { g.line('A', 22, 8, 74, 8, w); g.poly('A', [[70, 0], [90, 8], [70, 16]]); }
      if (st === 2) { g.line('A', 26, 8, 78, 8, w); g.poly('A', [[30, 0], [10, 8], [30, 16]]); }
    }
  },
  // ------------------------------------------------------------- Bombas y ventiladores
  {
    id: 'bombas/bomba', name: 'Bomba', category: 'bombas', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, B: C.darker, I: C.dark } },
      { name: 'En marcha', intervalMs: 120, frameCount: 3, colors: { O: C.light, S: C.green, B: C.darker, I: C.white } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.darker, I: C.redHi } }
    ],
    draw(g, st, f = 0) {
      g.box('O', 'B', 20, 88, 60, 10, 2);
      g.rect('O', 58, 6, 26, 10, 2);
      g.rect('B', 62, 16, 18, 16);
      g.line('O', 62, 16, 62, 32, Math.max(g.px(1), g.o));
      g.line('O', 80, 16, 80, 32, Math.max(g.px(1), g.o));
      g.disc('O', 'S', 46, 52, 36);
      g.disc('O', 'B', 46, 52, 18);
      const offset = (st === 1 ? f * 40 : 0);
      const blade = [[46, 52], [42, 40], [50, 38]];
      [0, 120, 240].forEach((deg) => g.poly('I', g.rotate(blade, 46, 52, deg + offset)));
      g.circle('O', 46, 52, 5);
    }
  },
  {
    id: 'bombas/ventilador', name: 'Ventilador', category: 'bombas', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.light, B: C.darker, S: C.gray } },
      { name: 'En marcha', intervalMs: 90, frameCount: 3, colors: { O: C.light, B: C.darker, S: C.green } },
      { name: 'Falla', colors: { O: C.light, B: C.darker, S: C.red } }
    ],
    draw(g, st, f = 0) {
      g.disc('O', 'B', 50, 50, 46);
      const blade = [[50, 50], [40, 30], [46, 12], [60, 14], [62, 34]];
      const offset = (st === 1 ? f * 40 : 0);
      [0, 120, 240].forEach((deg) => g.poly('S', g.rotate(blade, 50, 50, deg + offset)));
      g.circle('O', 50, 50, 9);
    }
  },
  {
    id: 'bombas/compresor', name: 'Compresor', category: 'bombas', sizes: [24, 32, 48],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, B: C.darker, M: C.white, N: C.black } },
      { name: 'En marcha', colors: { O: C.light, S: C.green, B: C.darker, M: C.white, N: C.black } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.darker, M: C.white, N: C.black } }
    ],
    draw(g) {
      g.box('O', 'B', 16, 86, 68, 11, 2);
      g.box('O', 'S', 22, 32, 56, 54, 4);
      const w = Math.max(g.px(1), 4);
      [44, 54, 64, 74].forEach((y) => g.line('B', 14, y, 86, y, w));
      g.box('O', 'B', 28, 16, 44, 16, 3);
      g.disc('O', 'M', 72, 16, 12);
      g.line('N', 72, 16, 78, 12, Math.max(g.px(1), 3));
    }
  },
  // ------------------------------------------------------------- Pilotos y señales
  {
    id: 'pilotos/piloto', name: 'Piloto (4 estados)', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Verde', colors: { O: C.light, B: C.darker, S: C.green, H: C.greenHi } },
      { name: 'Amarillo', colors: { O: C.light, B: C.darker, S: C.yellow, H: C.yellowHi } },
      { name: 'Rojo', colors: { O: C.light, B: C.darker, S: C.red, H: C.redHi } }
    ],
    draw(g) {
      g.disc('O', 'B', 50, 50, 47);
      g.circle('S', 50, 50, 34);
      g.circle('H', 38, 38, 9);
    }
  },
  {
    id: 'pilotos/piloto_verde', name: 'Piloto verde (bool)', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.green, H: C.greenHi } }
    ],
    draw(g) {
      g.disc('O', 'B', 50, 50, 47);
      g.circle('S', 50, 50, 34);
      g.circle('H', 38, 38, 9);
    }
  },
  {
    id: 'pilotos/piloto_rojo', name: 'Piloto rojo (bool)', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.red, H: C.redHi } }
    ],
    draw(g) {
      g.disc('O', 'B', 50, 50, 47);
      g.circle('S', 50, 50, 34);
      g.circle('H', 38, 38, 9);
    }
  },
  {
    id: 'pilotos/piloto_amarillo', name: 'Piloto amarillo (bool)', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.yellow, H: C.yellowHi } }
    ],
    draw(g) {
      g.disc('O', 'B', 50, 50, 47);
      g.circle('S', 50, 50, 34);
      g.circle('H', 38, 38, 9);
    }
  },
  {
    id: 'pilotos/piloto_azul', name: 'Piloto azul (bool)', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.cyan, H: C.cyanHi } }
    ],
    draw(g) {
      g.disc('O', 'B', 50, 50, 47);
      g.circle('S', 50, 50, 34);
      g.circle('H', 38, 38, 9);
    }
  },
  {
    id: 'pilotos/piloto_cuadrado', name: 'Piloto cuadrado verde', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.green, H: C.greenHi } }
    ],
    draw(g) {
      g.box('O', 'B', 6, 6, 88, 88, 8);
      g.box('O', 'S', 20, 20, 60, 60, 4);
      g.rect('H', 24, 24, 18, 18, 2);
    }
  },
  {
    id: 'pilotos/piloto_cuadrado_rojo', name: 'Piloto cuadrado rojo', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { O: C.light, B: C.darker, S: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { O: C.light, B: C.darker, S: C.red, H: C.redHi } }
    ],
    draw(g) {
      g.box('O', 'B', 6, 6, 88, 88, 8);
      g.box('O', 'S', 20, 20, 60, 60, 4);
      g.rect('H', 24, 24, 18, 18, 2);
    }
  },
  {
    id: 'pilotos/piloto_bisel', name: 'Piloto industrial biselado', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { W: C.white, M: C.light, D: C.dark, S: C.darker, L: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { W: C.white, M: C.light, D: C.dark, S: C.darker, L: C.green, H: C.greenHi } }
    ],
    draw(g) {
      // Bisel cromado exterior con reflejo
      g.circle('M', 50, 50, 48);
      g.arc('W', 50, 50, 48, Math.max(g.px(2), 8), 210, 330);
      g.arc('D', 50, 50, 48, Math.max(g.px(2), 8), 30, 150);
      // Aro negro interno
      g.circle('S', 50, 50, 36);
      // Lente esférica central
      g.circle('L', 50, 50, 30);
      g.circle('H', 40, 40, 8);
    }
  },
  {
    id: 'pilotos/baliza', name: 'Torre de señalización', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagada', colors: { O: C.light, R: C.redDim, Y: C.yellowDim, G: C.greenDim, B: C.dark } },
      { name: 'Verde', colors: { O: C.light, R: C.redDim, Y: C.yellowDim, G: C.green, B: C.dark } },
      { name: 'Amarilla', colors: { O: C.light, R: C.redDim, Y: C.yellow, G: C.greenDim, B: C.dark } },
      { name: 'Roja', colors: { O: C.light, R: C.red, Y: C.yellowDim, G: C.greenDim, B: C.dark } }
    ],
    draw(g) {
      g.rect('O', 28, 2, 44, 78, 10);
      g.rect('R', 28 + g.o, 2 + g.o, 44 - 2 * g.o, 25 - g.o, Math.max(0, 10 - g.o));
      g.rect('Y', 28 + g.o, 28, 44 - 2 * g.o, 24);
      g.rect('G', 28 + g.o, 53, 44 - 2 * g.o, 27 - g.o);
      g.rect('B', 36, 80, 28, 10);
      g.rect('O', 24, 90, 52, 9);
    }
  },
  // ------------------------------------------------------------- Válvulas
  {
    id: 'valvulas/valvula', name: 'Válvula', category: 'valvulas', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Cerrada', colors: { O: C.light, S: C.gray, B: C.darker } },
      { name: 'Abierta', colors: { O: C.light, S: C.green, B: C.darker } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.darker } }
    ],
    draw(g) {
      g.rect('O', 6, 36, 12, 42, 2);
      g.rect('O', 82, 36, 12, 42, 2);
      g.shape('O', 'S', [[18, 40], [50, 57], [18, 74]]);
      g.shape('O', 'S', [[82, 40], [50, 57], [82, 74]]);
      g.disc('O', 'B', 50, 57, 10);
      g.line('O', 50, 48, 50, 18, Math.max(g.px(2), g.o * 2));
      g.rect('O', 26, 12, 48, Math.max(g.px(2), 7), 3);
      g.line('O', 38, 15, 62, 15, Math.max(g.px(1), 3));
    }
  },
  {
    id: 'valvulas/valvula_motorizada', name: 'Válvula motorizada', category: 'valvulas', sizes: [24, 32, 48],
    states: [
      { name: 'Cerrada', colors: { O: C.light, S: C.gray, B: C.dark, T: C.light } },
      { name: 'Abierta', colors: { O: C.light, S: C.green, B: C.dark, T: C.light } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.dark, T: C.light } }
    ],
    draw(g) {
      g.shape('O', 'S', [[6, 46], [50, 70], [6, 94]]);
      g.shape('O', 'S', [[94, 46], [50, 70], [94, 94]]);
      g.line('O', 50, 70, 50, 34, Math.max(g.px(1), g.o * 1.5));
      g.disc('O', 'B', 50, 22, 20);
      g.polyline('T', [[40, 30], [40, 15], [50, 25], [60, 15], [60, 30]], Math.max(g.px(1), 5));
    }
  },
  {
    id: 'valvulas/valvula_solenoide', name: 'Electroválvula', category: 'valvulas', sizes: [24, 32, 48],
    states: [
      { name: 'Cerrada', colors: { O: C.light, S: C.gray, B: C.dark } },
      { name: 'Abierta', colors: { O: C.light, S: C.green, B: C.dark } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.dark } }
    ],
    draw(g) {
      g.shape('O', 'S', [[6, 46], [50, 70], [6, 94]]);
      g.shape('O', 'S', [[94, 46], [50, 70], [94, 94]]);
      g.line('O', 50, 70, 50, 36, Math.max(g.px(1), g.o * 1.5));
      g.box('O', 'B', 30, 4, 40, 32);
      g.line('O', 34, 30, 66, 10, Math.max(g.px(1), g.o * 1.5));
    }
  },
  // ------------------------------------------------------------- Tanques y equipos
  {
    id: 'equipos/tanque', name: 'Tanque (nivel)', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Vacío', colors: { O: C.light, B: C.darker, L: C.water } },
      { name: 'Bajo', colors: { O: C.light, B: C.darker, L: C.water } },
      { name: 'Medio', colors: { O: C.light, B: C.darker, L: C.water } },
      { name: 'Lleno', colors: { O: C.light, B: C.darker, L: C.water } }
    ],
    draw(g, st) {
      g.box('O', 'B', 16, 4, 68, 92, 14);
      const top = [96, 72, 48, 16][st] || 96;
      const inner = 4 + g.o;
      g.paint('L', (x, y) => y >= top && y <= 96 - g.o && x > 16 + g.o && x < 84 - g.o &&
        Math.hypot(Math.max(16 + 14 - x, 0, x - (84 - 14)), Math.max(0, y - (96 - 14))) <= 14 - g.o && y > inner);
    }
  },
  {
    id: 'equipos/silo', name: 'Silo / tolva', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Vacío', colors: { O: C.light, S: C.darker } },
      { name: 'Con material', colors: { O: C.light, S: C.amber } },
      { name: 'Falla', colors: { O: C.light, S: C.red } }
    ],
    draw(g) {
      g.shape('O', 'S', [[18, 6], [82, 6], [82, 58], [58, 84], [42, 84], [18, 58]]);
      g.rect('O', 40, 84, 20, 12);
    }
  },
  {
    id: 'equipos/resistencia', name: 'Resistencia / calefactor', category: 'equipos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagada', colors: { O: C.light, B: C.darker, S: C.mid } },
      { name: 'Encendida', colors: { O: C.light, B: C.darker, S: C.orange } }
    ],
    draw(g) {
      g.box('O', 'B', 4, 18, 92, 64, 8);
      g.polyline('S', [[12, 50], [22, 30], [34, 70], [46, 30], [58, 70], [70, 30], [80, 70], [88, 50]], Math.max(g.px(2), 8));
    }
  },
  {
    id: 'equipos/agitador', name: 'Agitador / mezclador', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, B: C.darker } },
      { name: 'En marcha', intervalMs: 140, frameCount: 4, colors: { O: C.light, S: C.green, B: C.darker } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.darker } }
    ],
    draw(g, st, f = 0) {
      g.rect('O', 28, 2, 44, 8, 2);
      g.box('O', 'S', 34, 10, 32, 24, 3);
      g.line('O', 50, 34, 50, 88, Math.max(g.px(2), 6));
      const offset = (st === 1 ? f * 45 : 0);
      const blade1 = [[50, 72], [24, 66], [26, 78]];
      const blade2 = [[50, 72], [76, 66], [74, 78]];
      g.poly('S', g.rotate(blade1, 50, 72, offset));
      g.poly('S', g.rotate(blade2, 50, 72, offset));
      const blade3 = [[50, 86], [28, 80], [30, 92]];
      const blade4 = [[50, 86], [72, 80], [70, 92]];
      g.poly('S', g.rotate(blade3, 50, 86, -offset));
      g.poly('S', g.rotate(blade4, 50, 86, -offset));
    }
  },
  {
    id: 'equipos/transportador', name: 'Cinta transportadora', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.light, S: C.gray, B: C.dark, A: C.dark } },
      { name: 'En marcha', intervalMs: 130, frameCount: 3, colors: { O: C.light, S: C.green, B: C.dark, A: C.white } },
      { name: 'Falla', colors: { O: C.light, S: C.red, B: C.dark, A: C.dark } }
    ],
    draw(g, st, f = 0) {
      g.rect('O', 14, 30, 72, Math.max(g.px(2), 8));
      g.rect('O', 14, 62, 72, Math.max(g.px(2), 8));
      g.disc('O', 'S', 18, 50, 18);
      g.disc('O', 'S', 82, 50, 18);
      const deg = (st === 1 ? f * 45 : 0);
      const [[x1, y1], [x2, y2]] = g.rotate([[18, 38], [18, 62]], 18, 50, deg);
      g.line('B', x1, y1, x2, y2, Math.max(g.px(1), 3));
      const [[x3, y3], [x4, y4]] = g.rotate([[82, 38], [82, 62]], 82, 50, deg);
      g.line('B', x3, y3, x4, y4, Math.max(g.px(1), 3));
      g.circle('B', 18, 50, 5);
      g.circle('B', 82, 50, 5);
      const packX = (st === 1 ? 32 + f * 10 : 40);
      g.box('O', 'A', packX, 16, 20, 14, 2);
    }
  },
  // ------------------------------------------------------------- Sensores
  {
    id: 'sensores/proximidad', name: 'Sensor de proximidad', category: 'sensores', sizes: [24, 32, 48],
    states: [
      { name: 'Sin detección', colors: { O: C.light, B: C.dark, S: C.mid, W: C.yellow } },
      { name: 'Detección', colors: { O: C.light, B: C.dark, S: C.yellow, W: C.yellow } }
    ],
    draw(g, st) {
      g.box('O', 'B', 4, 34, 52, 32, 4);
      g.rect('S', 56, 30, 12, 40, 3);
      if (st === 1) {
        const w = Math.max(g.px(1), 6);
        g.arc('W', 60, 50, 26, w, -50, 50);
        g.arc('W', 60, 50, 40, w, -45, 45);
      }
    }
  },
  {
    id: 'sensores/termometro', name: 'Temperatura', category: 'sensores', sizes: [24, 32, 48],
    states: [
      { name: 'Baja', colors: { O: C.light, B: C.darker, S: C.blue } },
      { name: 'Normal', colors: { O: C.light, B: C.darker, S: C.green } },
      { name: 'Alta', colors: { O: C.light, B: C.darker, S: C.red } }
    ],
    draw(g, st) {
      g.rect('O', 36, 2, 28, 74, 14);
      g.disc('O', 'B', 50, 78, 20);
      g.rect('B', 36 + g.o, 2 + g.o, 28 - 2 * g.o, 74, 14 - g.o);
      g.circle('S', 50, 78, 20 - 2 * g.o);
      const top = [62, 40, 14][st] ?? 62;
      g.rect('S', 44, top, 12, 78 - top);
    }
  },
  {
    id: 'sensores/manometro', name: 'Presión', category: 'sensores', sizes: [24, 32, 48],
    states: [
      { name: 'Baja', colors: { O: C.light, F: C.white, N: C.black, S: C.blue } },
      { name: 'Normal', colors: { O: C.light, F: C.white, N: C.black, S: C.green } },
      { name: 'Alta', colors: { O: C.light, F: C.white, N: C.black, S: C.red } }
    ],
    draw(g, st) {
      g.disc('O', 'F', 50, 50, 46);
      g.arc('S', 50, 50, 46 - g.o, Math.max(g.px(2), 9), 135, 405);
      const angle = [150, 270, 390][st] ?? 150;
      const a = angle * Math.PI / 180;
      g.line('N', 50, 50, 50 + Math.cos(a) * 32, 50 + Math.sin(a) * 32, Math.max(g.px(2), 7));
      g.circle('N', 50, 50, 7);
    }
  },
  // ------------------------------------------------------------- Alarmas
  {
    id: 'alarmas/alarma', name: 'Alarma', category: 'alarmas', sizes: [24, 32, 48],
    states: [
      { name: 'Normal', colors: { O: C.light, S: C.gray, T: C.darker } },
      { name: 'Activa', colors: { O: C.light, S: C.yellow, T: C.black } }
    ],
    draw(g) {
      g.shape('O', 'S', [[50, 6], [95, 90], [5, 90]]);
      g.rect('T', 44, 32, 12, 34, 2);
      g.rect('T', 44, 72, 12, 11, 2);
    }
  },
  {
    id: 'alarmas/campana', name: 'Campana / bocina', category: 'alarmas', sizes: [24, 32, 48],
    states: [
      { name: 'Silencio', colors: { O: C.light, S: C.gray, W: C.yellow } },
      { name: 'Sonando', colors: { O: C.light, S: C.yellow, W: C.yellow } }
    ],
    draw(g, st) {
      g.shape('O', 'S', [[50, 10], [66, 18], [70, 40], [74, 62], [84, 72], [16, 72], [26, 62], [30, 40], [34, 18]]);
      g.circle('O', 50, 82, 9);
      if (st === 1) {
        const w = Math.max(g.px(1), 5);
        g.arc('W', 50, 44, 46, w, -60, -20);
        g.arc('W', 50, 44, 46, w, 200, 240);
      }
    }
  },
  {
    id: 'alarmas/sirena', name: 'Baliza giratoria', category: 'alarmas', sizes: [24, 32, 48],
    states: [
      { name: 'Apagada', colors: { O: C.light, S: C.redDim, B: C.dark, W: C.red } },
      { name: 'Encendida', colors: { O: C.light, S: C.red, B: C.dark, W: C.red } }
    ],
    draw(g, st) {
      g.paint('O', (x, y) => y <= 72 && y >= 28 && Math.hypot((x - 50) / 26, Math.max(0, 50 - y) / 22) <= 1);
      g.paint('S', (x, y) => y <= 72 - g.o && y >= 28 + g.o && Math.hypot((x - 50) / (26 - g.o), Math.max(0, 50 - y) / (22 - g.o)) <= 1);
      g.box('O', 'B', 18, 72, 64, 20, 3);
      if (st === 1) {
        const w = Math.max(g.px(1), 6);
        g.line('W', 50, 2, 50, 16, w);
        g.line('W', 10, 30, 20, 38, w);
        g.line('W', 90, 30, 80, 38, w);
        g.line('W', 4, 58, 16, 58, w);
        g.line('W', 96, 58, 84, 58, w);
      }
    }
  },
  // ------------------------------------------------------------- Botones y selectores
  {
    id: 'botones/pulsador', name: 'Pulsador', category: 'botones', sizes: [24, 32, 48],
    states: [
      { name: 'Suelto', colors: { O: C.light, B: C.dark, S: C.green, H: C.greenHi } },
      { name: 'Presionado', colors: { O: C.light, B: C.dark, S: C.greenDim, H: C.greenHi } }
    ],
    draw(g, st) {
      g.box('O', 'B', 4, 4, 92, 92, 10);
      if (st === 0) {
        g.circle('S', 50, 50, 34);
        g.arc('H', 50, 50, 30, Math.max(g.px(1), 6), 200, 250);
      } else {
        g.circle('O', 50, 50, 30);
        g.circle('S', 50, 50, 30 - g.o);
      }
    }
  },
  {
    id: 'botones/paro_emergencia', name: 'Paro de emergencia', category: 'botones', sizes: [24, 32, 48],
    states: [
      { name: 'Normal', colors: { O: C.light, Y: C.yellow, S: C.red, H: C.redHi } },
      { name: 'Activado', colors: { O: C.light, Y: C.yellow, S: C.redDim, H: C.red } }
    ],
    draw(g, st) {
      g.box('O', 'Y', 2, 2, 96, 96, 12);
      if (st === 0) {
        g.disc('O', 'S', 50, 50, 38);
        g.arc('H', 50, 50, 32, Math.max(g.px(1), 6), 200, 250);
      } else {
        g.disc('O', 'S', 50, 50, 30);
        g.ring('H', 50, 50, 30 - g.o, Math.max(g.px(1), 5));
      }
    }
  },
  {
    id: 'botones/selector', name: 'Selector 0-1', category: 'botones', sizes: [24, 32, 48],
    states: [
      { name: 'Posición 0', colors: { O: C.light, B: C.darker, K: C.black, I: C.white } },
      { name: 'Posición 1', colors: { O: C.light, B: C.darker, K: C.black, I: C.green } }
    ],
    draw(g, st) {
      g.disc('O', 'B', 50, 50, 46);
      const deg = st === 0 ? -45 : 45;
      const knob = g.rotate([[40, 14], [60, 14], [60, 86], [40, 86]], 50, 50, deg);
      g.poly('K', knob);
      const [[x1, y1], [x2, y2]] = g.rotate([[50, 20], [50, 42]], 50, 50, deg);
      g.line('I', x1, y1, x2, y2, Math.max(g.px(2), 8));
    }
  },
  {
    id: 'botones/contacto', name: 'Contacto / interruptor', category: 'botones', sizes: [24, 32, 48],
    states: [
      { name: 'Abierto', colors: { O: C.light, S: C.mid } },
      { name: 'Cerrado', colors: { O: C.light, S: C.green } }
    ],
    draw(g, st) {
      const w = Math.max(g.px(2), 8);
      g.line('O', 2, 66, 22, 66, w);
      g.line('O', 78, 66, 98, 66, w);
      g.disc('O', 'S', 24, 66, 9);
      g.disc('O', 'S', 76, 66, 9);
      if (st === 0) g.line('S', 26, 64, 78, 26, w);
      else g.line('S', 26, 58, 76, 58, w);
    }
  }
];

module.exports = { CATEGORIES, ICONS, C };

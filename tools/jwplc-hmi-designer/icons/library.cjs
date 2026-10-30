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
    id: 'motores/motor', name: 'Motor trifásico', category: 'motores', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.darker, S: C.gray, F: C.light, T: C.dark, C: C.darker, W: C.white, K: C.black } },
      { name: 'En marcha', intervalMs: 120, frameCount: 4, colors: { O: C.darker, S: C.green, F: C.greenHi, T: C.dark, C: C.darker, W: C.white, K: C.black } },
      { name: 'Falla', colors: { O: C.darker, S: C.red, F: C.redHi, T: C.dark, C: C.darker, W: C.white, K: C.black } }
    ],
    draw(g, st, f = 0) {
      // Patas de montaje inferiores (B3)
      g.box('O', 'T', 20, 78, 16, 12, 1);
      g.box('O', 'T', 56, 78, 16, 12, 1);
      g.circle('K', 28, 84, 2);
      g.circle('K', 64, 84, 2);

      // Carcasa cilíndrica del estator con aletas
      g.box('O', 'S', 22, 32, 50, 46, 2);
      [42, 52, 62, 72].forEach((y) => g.line('F', 23, y, 71, y, Math.max(g.px(1), 3)));

      // Caja de bornes superior
      g.box('O', 'T', 34, 16, 26, 16, 2);
      g.line('F', 36, 24, 58, 24, Math.max(g.px(1), 2));

      // Tapa trasera y ventilador (izquierda)
      g.box('O', 'C', 8, 34, 14, 42, 3);
      const fanOffset = (st === 1 ? f * 45 : 0);
      const fanBlade = [[15, 55], [11, 46], [19, 44]];
      [0, 90, 180, 270].forEach((deg) => g.poly('F', g.rotate(fanBlade, 15, 55, deg + fanOffset)));
      g.circle('K', 15, 55, 3);

      // Tapa delantera (derecha)
      g.rect('O', 72, 36, 6, 38, 1);

      // Eje de transmisión frontal
      g.box('O', 'W', 78, 48, 16, 14, 1);
      const keyY = (st === 1) ? [49, 53, 58, 53][f % 4] : 51;
      const keyCol = (st === 1 && f % 2 === 1) ? 'F' : 'K';
      g.line(keyCol, 80, keyY, 92, keyY, Math.max(g.px(1), 3));
    }
  },
  {
    id: 'motores/motorreductor', name: 'Motorreductor', category: 'motores', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Parado', colors: { O: C.darker, S: C.gray, F: C.light, T: C.dark, G: C.darker, W: C.white, K: C.black } },
      { name: 'En marcha', intervalMs: 130, frameCount: 4, colors: { O: C.darker, S: C.green, F: C.greenHi, T: C.dark, G: C.darker, W: C.white, K: C.black } },
      { name: 'Falla', colors: { O: C.darker, S: C.red, F: C.redHi, T: C.dark, G: C.darker, W: C.white, K: C.black } }
    ],
    draw(g, st, f = 0) {
      // Motor a la izquierda
      g.box('O', 'S', 12, 34, 34, 42, 2);
      [42, 50, 58, 66].forEach((y) => g.line('F', 13, y, 45, y, Math.max(g.px(1), 2)));
      g.box('O', 'T', 20, 20, 20, 14, 2);
      g.box('O', 'T', 4, 36, 8, 38, 2);

      // Ventilador motor
      const fanOffset = (st === 1 ? f * 45 : 0);
      const fanBlade = [[8, 55], [5, 48], [11, 46]];
      [0, 120, 240].forEach((deg) => g.poly('F', g.rotate(fanBlade, 8, 55, deg + fanOffset)));
      g.box('O', 'T', 10, 76, 14, 10, 1);

      // Caja reductora robusta a la derecha
      g.box('O', 'G', 46, 26, 36, 52, 4);
      g.line('F', 48, 36, 80, 36, Math.max(g.px(1), 2));
      g.box('O', 'T', 68, 78, 14, 10, 1);

      // Visor de nivel de aceite
      g.circle('W', 54, 66, 4);
      g.circle('K', 54, 66, 2);

      // Eje macizo de salida
      g.disc('O', 'W', 70, 48, 12);
      const slowDeg = (st === 1 ? f * 90 : 0);
      const keyPts = g.rotate([[70, 48], [70, 38]], 70, 48, slowDeg);
      g.line('K', keyPts[0][0], keyPts[0][1], keyPts[1][0], keyPts[1][1], Math.max(g.px(1), 3));
      g.circle('K', 70, 48, 4);
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
    id: 'pilotos/piloto', name: 'Piloto luminoso', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagado', colors: { W: C.white, M: C.light, D: C.dark, S: C.darker, L: C.dark, H: C.mid } },
      { name: 'Encendido', colors: { W: C.white, M: C.light, D: C.dark, S: C.darker, L: C.green, H: C.greenHi } }
    ],
    draw(g) {
      g.circle('M', 50, 50, 48);
      g.arc('W', 50, 50, 48, Math.max(g.px(2), 8), 210, 330);
      g.arc('D', 50, 50, 48, Math.max(g.px(2), 8), 30, 150);
      g.circle('S', 50, 50, 36);
      g.circle('L', 50, 50, 30);
      g.circle('H', 40, 40, 8);
    }
  },
  {
    id: 'pilotos/piloto_cuadrado', name: 'Piloto cuadrado', category: 'pilotos', sizes: [24, 32, 48],
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
    id: 'pilotos/baliza', name: 'Torre de señalización', category: 'pilotos', sizes: [24, 32, 48],
    states: [
      { name: 'Apagada', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.redDim, Y: C.yellowDim, G: C.greenDim } },
      { name: 'Verde', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.redDim, Y: C.yellowDim, G: C.green } },
      { name: 'Ámbar', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.redDim, Y: C.amber, G: C.greenDim } },
      { name: 'Ámbar+Verde', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.redDim, Y: C.amber, G: C.green } },
      { name: 'Rojo', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.red, Y: C.yellowDim, G: C.greenDim } },
      { name: 'Rojo+Verde', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.red, Y: C.yellowDim, G: C.green } },
      { name: 'Rojo+Ámbar', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.red, Y: C.amber, G: C.greenDim } },
      { name: 'Rojo+Ámbar+Verde', colors: { O: C.light, M: C.darker, B: C.blueDim, R: C.red, Y: C.amber, G: C.green } },
      { name: 'Azul', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.redDim, Y: C.yellowDim, G: C.greenDim } },
      { name: 'Azul+Verde', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.redDim, Y: C.yellowDim, G: C.green } },
      { name: 'Azul+Ámbar', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.redDim, Y: C.amber, G: C.greenDim } },
      { name: 'Azul+Ámbar+Verde', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.redDim, Y: C.amber, G: C.green } },
      { name: 'Azul+Rojo', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.red, Y: C.yellowDim, G: C.greenDim } },
      { name: 'Azul+Rojo+Verde', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.red, Y: C.yellowDim, G: C.green } },
      { name: 'Azul+Rojo+Ámbar', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.red, Y: C.amber, G: C.greenDim } },
      { name: 'Azul+Rojo+Ámbar+Verde', colors: { O: C.light, M: C.darker, B: C.cyan, R: C.red, Y: C.amber, G: C.green } }
    ],
    draw(g) {
      // Base y mástil inferior
      g.rect('M', 45, 80, 10, 14);
      g.rect('O', 32, 94, 36, 6, 2);
      // Módulos apilados (Piso 4: Azul, Piso 3: Rojo, Piso 2: Ámbar, Piso 1: Verde)
      g.rect('B', 34, 10, 32, 16, 2);
      g.rect('R', 34, 28, 32, 16, 2);
      g.rect('Y', 34, 46, 32, 16, 2);
      g.rect('G', 34, 64, 32, 16, 2);
      // Tapa superior
      g.rect('O', 30, 6, 40, 5, 2);
      // Anillos divisores
      g.rect('O', 32, 26, 36, 3);
      g.rect('O', 32, 44, 36, 3);
      g.rect('O', 32, 62, 36, 3);
      g.rect('O', 32, 80, 36, 3);
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
    id: 'equipos/piston', name: 'Pistón neumático', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Retraído', colors: { O: C.darker, S: C.gray, C: C.light, R: C.white, K: C.black } },
      { name: 'En movimiento', intervalMs: 110, frameCount: 4, colors: { O: C.darker, S: C.green, C: C.light, R: C.white, K: C.black } },
      { name: 'Extendido', colors: { O: C.darker, S: C.green, C: C.light, R: C.white, K: C.black } },
      { name: 'Falla', colors: { O: C.darker, S: C.red, C: C.light, R: C.white, K: C.black } }
    ],
    draw(g, st, f = 0) {
      g.box('O', 'S', 8, 32, 46, 36, 2);
      g.line('C', 10, 42, 52, 42, Math.max(g.px(1), 2));
      g.line('C', 10, 58, 52, 58, Math.max(g.px(1), 2));
      g.box('O', 'C', 4, 28, 8, 44, 1);
      g.rect('K', 5, 22, 6, 6);
      g.box('O', 'C', 54, 28, 8, 44, 1);
      g.rect('K', 55, 22, 6, 6);

      const ext = (st === 0) ? 4 : (st === 2) ? 32 : (st === 3) ? 16 : (8 + f * 8);
      g.box('O', 'R', 62, 44, ext, 12, 1);
      const forkX = 62 + ext;
      g.box('O', 'C', forkX, 40, 8, 20, 2);
      g.circle('K', forkX + 4, 50, 3);
    }
  },
  {
    id: 'equipos/piston_vertical', name: 'Pistón vertical', category: 'equipos', sizes: [24, 32, 48, 64],
    states: [
      { name: 'Retraído', colors: { O: C.darker, S: C.gray, C: C.light, R: C.white, K: C.black } },
      { name: 'En movimiento', intervalMs: 110, frameCount: 4, colors: { O: C.darker, S: C.green, C: C.light, R: C.white, K: C.black } },
      { name: 'Extendido', colors: { O: C.darker, S: C.green, C: C.light, R: C.white, K: C.black } },
      { name: 'Falla', colors: { O: C.darker, S: C.red, C: C.light, R: C.white, K: C.black } }
    ],
    draw(g, st, f = 0) {
      g.box('O', 'C', 28, 88, 44, 8, 1);
      g.rect('K', 72, 89, 6, 6);
      g.box('O', 'S', 32, 46, 36, 42, 2);
      g.line('C', 42, 48, 42, 86, Math.max(g.px(1), 2));
      g.line('C', 58, 48, 58, 86, Math.max(g.px(1), 2));
      g.box('O', 'C', 28, 38, 44, 8, 1);
      g.rect('K', 72, 39, 6, 6);

      const ext = (st === 0) ? 4 : (st === 2) ? 30 : (st === 3) ? 15 : (6 + f * 8);
      g.box('O', 'R', 44, 38 - ext, 12, ext, 1);
      const forkY = 38 - ext - 8;
      g.box('O', 'C', 40, forkY, 20, 8, 2);
      g.circle('K', 50, forkY + 4, 3);
    }
  },
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

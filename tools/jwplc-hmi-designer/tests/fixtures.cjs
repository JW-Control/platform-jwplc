// Proyectos de prueba del codegen (A15-5). Cada uno genera un header que se
// compara con tests/golden/<nombre>.h (codegen-golden.cjs).
'use strict';

const COLORS = { labelColor: 0xFFFF, valueColor: 0x07FF, backgroundColor: 0x0000, frameColor: 0xFFFF };
const BAR_BASE = {
  ...COLORS, label: 'Nivel', unit: '%', capacity: 1, preview: '', valueSize: 1, labelSize: 1,
  frame: false, layout: 'STACKED', align: 'LEFT', barMin: 0, barMax: 100, barValue: 50,
  showLabel: true, barLabel: 'Nivel', barUnit: '%', barWidth: 110
};

const node = (key, parent, type, label, extra = {}) => ({
  key, parent, type, label, variable: '', valueType: '', initial: 0, min: 0, max: 100, step: 1,
  decimals: 1, unit: '', falseText: 'OFF', trueText: 'ON', ...extra
});

module.exports = {
  full: {
    // A15-7: IDLE no por defecto (entrar con OK, volver por inactividad, arrancar en el HMI).
    display: { start: 'HMI', wake: 'BUTTON', wakeButton: 'BTN_OK', back: 'TIMEOUT', timeoutS: 30 },
    pages: [{ id: 0, name: 'Principal' }, { id: 1, name: 'Alarmas' }, { id: 2, name: 'Ajustes' }],
    fields: [
      { ...COLORS, type: 'TEXT', name: 'T', id: 'FIELD_ESTADO', variable: 'estado', capacity: 12, x: 4, y: 4, preview: 'READY', label: 'Estado', unit: '', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0, layerOrder: 5 },
      { ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_TEMP', variable: 'temperatura', x: 4, y: 40, preview: '25.6', label: 'Temp', unit: 'C', integerDigits: 3, decimalDigits: 1, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 6 },
      { ...COLORS, type: 'BOOL', name: 'B', id: 'FIELD_MOTOR', variable: 'motor', x: 4, y: 70, label: 'Motor', unit: '', falseText: 'PARADO', trueText: 'MARCHA', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'CENTER', page: 1, layerOrder: 7 },
      { ...BAR_BASE, type: 'BAR', name: 'G', id: 'FIELD_NIVEL', variable: 'nivel', x: 160, y: 40, page: 2, barPreset: 'CIRCULAR', layerOrder: 8 },
      { ...BAR_BASE, type: 'BAR', name: 'H', id: 'FIELD_GRAD', variable: 'grad', x: 160, y: 90, page: 2, barPreset: 'LINEAR', barFillMode: 'GRADIENT', layerOrder: 9 },
      { type: 'RECT', name: 'Marco', id: 'R1', x: 2, y: 2, x2: 150, y2: 30, frameColor: 0xF800, fillColor: 0x001F, fill: true, cornerRadius: 4, page: 0, layerOrder: 1 },
      { type: 'RAW_TEXT', name: 'Titulo', id: 'RAW_1', text: 'Planta "A"', font: 'SANS_9', size: 1, x: 200, y: 4, textColor: 0xFD20, backgroundColor: 0, transparentBackground: false, page: 0, layerOrder: 2 },
      { type: 'ELLIPSE', name: 'Elipse', id: 'E1', x: 200, y: 40, x2: 260, y2: 70, frameColor: 0xFFFF, fill: true, fillColor: 0x07E0, page: 0, layerOrder: 3 },
      { type: 'TRIANGLE', name: 'Tri', id: 'T1', x: 270, y: 40, x2: 310, y2: 80, frameColor: 0xFFFF, fill: true, fillColor: 0xFFE0, page: 1, layerOrder: 3 },
      { type: 'POLYGON', name: 'Hex', id: 'P1', x: 100, y: 90, x2: 150, y2: 130, sides: 6, frameColor: 0xFFFF, fill: true, fillColor: 0x001F, page: 1, layerOrder: 4 },
      { type: 'LINE', name: 'Linea', id: 'L1', x: 0, y: 140, x2: 319, y2: 140, frameColor: 0x7BEF, page: 2, layerOrder: 10 },
      { key: 'NAV_BAR', type: 'NAV_BAR', name: 'Barra de Navegación', label: 'NAV_BAR', x: 0, y: 147, preset: 'industrial-color', position: 'bottom', height: 22, global: true, hidden: false, locked: false, layerOrder: 999 }
    ]
  },

  fieldsOnly: {
    pages: [{ id: 0, name: 'Principal' }],
    fields: [
      { ...COLORS, type: 'TEXT', name: 'T', id: 'FIELD_MSG', variable: 'mensaje', capacity: 16, x: 4, y: 4, label: 'Msg', unit: '', valueSize: 1, labelSize: 1, frame: true, layout: 'STACKED', align: 'CENTER', page: 0, layerOrder: 1 },
      { ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_P', variable: 'presion', x: 4, y: 40, label: 'P', unit: 'bar', integerDigits: 2, decimalDigits: 2, signedValue: true, leadingZeros: true, valueSize: 3, labelSize: 2, frame: true, layout: 'STACKED', align: 'LEFT', page: 0, layerOrder: 2, backgroundColor: 'TRANSPARENT' },
      { ...COLORS, type: 'BOOL', name: 'B', id: 'FIELD_B', variable: 'valvula', x: 4, y: 100, label: 'Valvula', unit: '', falseText: 'CERRADA', trueText: 'ABIERTA', valueSize: 1, labelSize: 1, frame: true, layout: 'STACKED', align: 'RIGHT', page: 0, layerOrder: 3 },
      { ...BAR_BASE, type: 'BAR', name: 'N', id: 'FIELD_TANQUE', variable: 'tanque', x: 160, y: 4, page: 0, layout: 'INLINE', barWidth: 150, trackColor: 0x18C3, layerOrder: 4 }
    ]
  },

  // A15: barra de navegación estilo "Contador" (sólo NN/TT, abajo a la izquierda).
  navCounter: {
    pages: [{ id: 0, name: 'Principal' }, { id: 1, name: 'Página 2' }],
    fields: [
      { ...COLORS, type: 'TEXT', name: 'T', id: 'FIELD_T', variable: 't', capacity: 6, x: 4, y: 20, preview: 'OK', label: 'T', unit: '', valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0, layerOrder: 2 },
      { key: 'NAV_BAR', type: 'NAV_BAR', name: 'Barra de Navegación', label: 'NAV_BAR', x: 2, y: 155, width: 36, height: 12, preset: 'counter', position: 'bottom', global: true, hidden: false, locked: false, layerOrder: 999 }
    ]
  },
  // Alpha15 I1: VALUE con separador de miles y con duración (variables enteras)
  // y BOOL «Piloto» (con y sin fondo por estado).
  i1Formats: {
    pages: [{ id: 0, name: 'Principal' }],
    fields: [
      { ...COLORS, type: 'VALUE', name: 'C', id: 'FIELD_PIEZAS', variable: 'piezas', x: 4, y: 4, preview: '123456', label: 'Piezas', unit: '', integerDigits: 7, decimalDigits: 0, signedValue: false, leadingZeros: false, thousandsSep: ',', valueSize: 2, labelSize: 1, frame: true, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 1 },
      { ...COLORS, type: 'VALUE', name: 'E', id: 'FIELD_ENERGIA', variable: 'energia', x: 4, y: 40, preview: '1234.5', label: 'kWh', unit: '', integerDigits: 6, decimalDigits: 1, signedValue: false, leadingZeros: false, thousandsSep: '.', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 2 },
      { ...COLORS, type: 'VALUE', name: 'R', id: 'FIELD_RESTANTE', variable: 't_restante', x: 4, y: 76, preview: '303', label: 'Resta', unit: '', valueKind: 'DURATION', durationPattern: 'MM_SS', durationUnit: 'S', integerDigits: 2, decimalDigits: 0, signedValue: true, leadingZeros: false, valueSize: 3, labelSize: 1, frame: true, layout: 'STACKED', align: 'LEFT', page: 0, layerOrder: 3 },
      { ...COLORS, type: 'VALUE', name: 'M', id: 'FIELD_MARCHA', variable: 'marcha_ms', x: 160, y: 76, preview: '3903000', label: 'Marcha', unit: '', valueKind: 'DURATION', durationPattern: 'HH_MM_SS', durationUnit: 'MS', integerDigits: 3, decimalDigits: 0, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'STACKED', align: 'RIGHT', page: 0, layerOrder: 4 },
      { ...COLORS, type: 'VALUE', name: 'K', id: 'FIELD_CICLO', variable: 'ciclo', x: 160, y: 130, preview: '45', label: 'Ciclo', unit: '', valueKind: 'DURATION', durationPattern: 'COMPACT', durationUnit: 'S', integerDigits: 2, decimalDigits: 0, signedValue: false, leadingZeros: false, valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 5 },
      { ...COLORS, type: 'BOOL', name: 'P', id: 'FIELD_BOMBA', variable: 'bomba', x: 4, y: 140, label: 'Bomba', unit: '', falseText: 'OFF', trueText: 'ON', boolStyle: 'PILOT', stateColors: true, falseColor: 0x4208, trueColor: 0x07E0, falseBackground: 0x0000, trueBackground: 0x0000, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'CENTER', page: 0, layerOrder: 6 },
      { ...COLORS, type: 'BOOL', name: 'A', id: 'FIELD_ALARMA', variable: 'alarma', x: 90, y: 140, label: 'Alarma', unit: '', falseText: 'OFF', trueText: 'ON', boolStyle: 'PILOT', stateColors: true, falseColor: 0x4208, trueColor: 0xFFE0, falseBackground: 0x0000, trueBackground: 0xF800, valueSize: 1, labelSize: 1, frame: true, layout: 'INLINE', align: 'CENTER', page: 0, layerOrder: 7 }
    ]
  },

  // Alpha15 I2: VALUE de fecha/hora del RTC (sin variable ni setter).
  i2Clock: {
    pages: [{ id: 0, name: 'Principal' }],
    fields: [
      { ...COLORS, type: 'VALUE', name: 'Hora', id: 'FIELD_HORA', variable: 'hora', x: 4, y: 4, preview: '', label: '', unit: '', valueKind: 'DATETIME', dateTimePattern: 'HH_MM_SS', integerDigits: 2, decimalDigits: 0, signedValue: false, leadingZeros: false, valueSize: 3, labelSize: 1, frame: true, layout: 'INLINE', align: 'LEFT', page: 0, layerOrder: 1 },
      { ...COLORS, type: 'VALUE', name: 'Fecha', id: 'FIELD_FECHA', variable: 'hora', x: 4, y: 44, preview: '', label: 'Fecha', unit: '', valueKind: 'DATETIME', dateTimePattern: 'DD_MM_YYYY', integerDigits: 2, decimalDigits: 0, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'STACKED', align: 'LEFT', page: 0, layerOrder: 2 },
      { ...COLORS, type: 'VALUE', name: 'T', id: 'FIELD_TEMP', variable: 'temperatura', x: 4, y: 90, preview: '25.6', label: 'Temp', unit: 'C', integerDigits: 3, decimalDigits: 1, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 3 }
    ]
  },

  // Alpha15 I3: tildes, ñ y ° en etiquetas, unidades, textos BOOL y RAW_TEXT GLCD.
  i3Accents: {
    pages: [{ id: 0, name: 'Configuración' }],
    fields: [
      { ...COLORS, type: 'TEXT', name: 'T', id: 'FIELD_ESTADO', variable: 'estado', capacity: 12, x: 4, y: 4, preview: 'Listo ñ', label: 'Señal', unit: '', valueSize: 1, labelSize: 1, frame: false, layout: 'INLINE', align: 'LEFT', page: 0, layerOrder: 1 },
      { ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_PRESION', variable: 'presion', x: 4, y: 30, preview: '25.6', label: 'Presión', unit: '°C', integerDigits: 3, decimalDigits: 1, signedValue: false, leadingZeros: false, valueSize: 2, labelSize: 1, frame: true, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 2 },
      { ...COLORS, type: 'BOOL', name: 'B', id: 'FIELD_VALVULA', variable: 'valvula', x: 4, y: 70, label: 'Válvula', unit: '', falseText: 'NO', trueText: 'SÍ', valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'CENTER', page: 0, layerOrder: 3 },
      { type: 'RAW_TEXT', name: 'Titulo', id: 'RAW_1', text: 'Línea 2 · ¿Listo?', font: 'GLCD', size: 1, x: 4, y: 120, textColor: 0xFFFF, backgroundColor: 0, transparentBackground: false, page: 0, layerOrder: 4 }
    ]
  },

  shapesOnly: {
    pages: [{ id: 0, name: 'Principal' }],
    fields: [{ type: 'RECT', name: 'Marco', id: 'R1', x: 2, y: 2, x2: 150, y2: 30, frameColor: 0xF800, fill: false, page: 0, layerOrder: 1 }]
  },

  legacyPageGap: {
    pages: [{ id: 0, name: 'Principal' }, { id: 2, name: 'Ajustes' }],
    fields: [{ ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_X', variable: 'x', x: 4, y: 40, label: 'X', unit: '', integerDigits: 3, decimalDigits: 0, valueSize: 2, labelSize: 1, frame: false, layout: 'INLINE', align: 'RIGHT', page: 2, layerOrder: 1 }]
  },

  menu: {
    pages: [{ id: 0, name: 'Principal' }, { id: 1, name: 'Ajustes' }],
    fields: [
      { ...COLORS, type: 'VALUE', name: 'V', id: 'FIELD_TEMP', variable: 'temperatura', x: 20, y: 60, label: 'Temp', unit: 'C', integerDigits: 3, decimalDigits: 1, signedValue: true, leadingZeros: false, valueSize: 3, labelSize: 1, frame: true, layout: 'INLINE', align: 'RIGHT', page: 0, layerOrder: 1 },
      {
        key: 'menu-1', type: 'MENU', name: 'Ajustes', id: 'MENU_AJUSTES', title: 'Ajustes',
        x: 10, y: 20, width: 300, height: 140, page: 1, textSize: 1, frame: true, layerOrder: 2,
        colors: { text: 0xFFFF, background: 0x0000, value: 0x07FF, selectText: 0x0000, selectBackground: 0x07FF, editText: 0x0000, editBackground: 0xFD20, title: 0x0000, titleBackground: 0xFFFF, frame: 0xFFFF },
        nodes: [
          node('t', null, 'VIEW', 'Temperatura', { variable: 'temperatura', valueType: 'float', initial: 21.5, decimals: 1, unit: 'C' }),
          node('c', null, 'EDIT_NUM', 'Consigna', { variable: 'consigna', valueType: 'float', initial: 45, min: 0, max: 100, step: 0.5, decimals: 1, unit: 'C' }),
          node('b', null, 'EDIT_BOOL', 'Bomba', { variable: 'bomba', valueType: 'bool', initial: 0, falseText: 'PARADA', trueText: 'MARCHA' }),
          node('a', null, 'SUBMENU', 'Avanzado'),
          node('r', 'a', 'EDIT_NUM', 'Retardo', { variable: 'retardo', valueType: 'int32', initial: 30, min: 0, max: 600, step: 5, decimals: 0, unit: 's' }),
          node('v', 'a', 'VIEW', 'Bomba', { variable: 'bomba', valueType: 'bool', falseText: 'OFF', trueText: 'ON' })
        ]
      }
    ]
  }
};

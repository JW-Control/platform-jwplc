// Especificación del formato .jwhmi (JWPLC_HMI_PROJECT v1) para agentes y CLI.
//
// Fuente única de verdad de: el esquema JSON (schema/jwhmi.schema.json), la
// referencia en Markdown (agent/REFERENCIA_JWHMI.md), la normalización y la
// validación de cli/jwhmi.cjs. `node cli/jwhmi.cjs schema` regenera los dos
// archivos; tests/agent-cli.test.cjs verifica que estén al día y que los
// valores por defecto coincidan con los que crea el Designer.
//
// Cada propiedad: { kind, desc, default?, enum?, min?, max?, required?, internal? }
//   kind: int | number | bool | string | color | symbol | enum | object | array | any
//   default: valor que pone la normalización cuando falta (lo que crea el Designer).
//   internal: el Designer la guarda pero no hace falta escribirla (no se documenta en la guía).
'use strict';

const SCREEN = Object.freeze({ width: 320, height: 170 });
const LIMITS = Object.freeze({ pages: 16, fields: 32, pixelMaps: 16, menus: 4, menuNodes: 64, menuDepth: 3, nameLength: 48 });

// Colores con nombre (RGB565). Los ocho primeros son la paleta del Designer;
// el resto, los TFT_* de TFT_eSPI más comunes.
const NAMED_COLORS = Object.freeze({
  BLACK: 0x0000, WHITE: 0xFFFF, RED: 0xF800, GREEN: 0x07E0, BLUE: 0x001F, CYAN: 0x07FF, YELLOW: 0xFFE0, ORANGE: 0xFD20,
  NAVY: 0x000F, DARKGREEN: 0x03E0, DARKCYAN: 0x03EF, MAROON: 0x7800, PURPLE: 0x780F, OLIVE: 0x7BE0,
  LIGHTGREY: 0xD69A, DARKGREY: 0x7BEF, GREY: 0x8410, MAGENTA: 0xF81F, PINK: 0xFE19, GREENYELLOW: 0xB7E0, BROWN: 0x9A60, GOLD: 0xFEA0
});

const FONTS = Object.freeze(['GLCD', 'FONT2', 'FONT4',
  ...['SANS', 'SERIF', 'MONO'].flatMap((f) => [9, 12, 18, 24].flatMap((pt) => [`${f}_${pt}`, `${f}_BOLD_${pt}`]))]);

const int = (min, max, desc, extra = {}) => ({ kind: 'int', min, max, desc, ...extra });
const num = (desc, extra = {}) => ({ kind: 'number', desc, ...extra });
const bool = (desc, extra = {}) => ({ kind: 'bool', desc, ...extra });
const str = (desc, extra = {}) => ({ kind: 'string', desc, ...extra });
const color = (desc, extra = {}) => ({ kind: 'color', desc, ...extra });
const oneOf = (values, desc, extra = {}) => ({ kind: 'enum', enum: values, desc, ...extra });

// --- Propiedades comunes ------------------------------------------------------

const BASE = {
  type: { kind: 'string', desc: 'Tipo de objeto.', required: true },
  name: str('Nombre visible en la lista de objetos del Designer (no llega al C++).'),
  id: { kind: 'symbol', desc: 'Identificador C++ del objeto (único en el proyecto). Si falta se genera FIELD_<TIPO>_<n>.' },
  page: int(0, LIMITS.pages - 1, 'Página (id) donde se dibuja.', { default: 0 }),
  layerOrder: int(0, 9999, 'Orden de capa: menor = más al frente. Si falta en todos, el orden del arreglo manda (el último queda encima).'),
  hidden: bool('Oculto en el lienzo y fuera del header.', { internal: true }),
  locked: bool('Bloqueado en el lienzo (no se puede mover con el ratón).', { internal: true }),
  cornerRadius: int(0, 85, 'Lo agrega el Designer al abrir (sólo lo usan RECT y MENU).', { internal: true })
};

const POS = {
  x: int(0, SCREEN.width - 1, 'Esquina superior izquierda, X (px).', { required: true }),
  y: int(0, SCREEN.height - 1, 'Esquina superior izquierda, Y (px).', { required: true })
};

// TEXT / VALUE / BOOL / BAR: campos dinámicos del runtime JWPLC_UI.
const FIELD = {
  ...BASE,
  ...POS,
  variable: { kind: 'symbol', desc: 'Variable C++ que el header declara y tu sketch asigna. Si falta se genera.' },
  label: str('Etiqueta. Vacía = sin etiqueta.', { default: '' }),
  unit: str('Unidad tras el valor. Vacía = sin unidad.', { default: '' }),
  labelSize: int(1, 4, 'Escala de la etiqueta (celda GLCD 6×8 px × escala).', { default: 1 }),
  valueSize: int(1, 4, 'Escala del valor.', { default: 2 }),
  frame: bool('Marco alrededor del campo.', { default: false }),
  layout: oneOf(['INLINE', 'STACKED'], 'INLINE: etiqueta y valor en una fila. STACKED: etiqueta arriba, valor abajo.', { default: 'INLINE' }),
  align: oneOf(['LEFT', 'CENTER', 'RIGHT'], 'Alineación del valor dentro del campo.', { default: 'LEFT' }),
  labelColor: color('Color de la etiqueta.', { default: 0xFFFF }),
  valueColor: color('Color del valor.', { default: 0x07FF }),
  backgroundColor: color('Fondo del campo.', { default: 0x0000 }),
  frameColor: color('Color del marco.', { default: 0xFFFF }),
  preview: str('Texto de muestra en el lienzo del Designer (no llega al C++).')
};

const TYPES = {
  TEXT: {
    group: 'campo',
    desc: 'Texto dinámico. Declara `char variable[2·capacity+1]`; el sketch lo llena y el runtime lo redibuja si cambia.',
    props: {
      ...FIELD,
      capacity: int(1, 39, 'Caracteres reservados en pantalla (el ancho del campo sale de aquí).', { default: 12 })
    }
  },
  VALUE: {
    group: 'campo',
    desc: 'Número con formato (o duración, o fecha/hora del RTC). Declara `float` (o `int32_t`/`uint32_t` en duraciones y contadores con miles).',
    props: {
      ...FIELD,
      align: { ...FIELD.align, default: 'RIGHT' },
      valueKind: oneOf(['NUMBER', 'DURATION', 'DATETIME'], 'NUMBER: número. DURATION: segundos o ms mostrados como tiempo. DATETIME: fecha/hora del RTC, sin variable ni setter.', { doc: 'NUMBER' }),
      integerDigits: int(1, 9, 'Dígitos enteros reservados.', { default: 3 }),
      decimalDigits: int(0, 6, 'Decimales (NUMBER).', { default: 1 }),
      signedValue: bool('Reserva el signo menos.', { default: false }),
      leadingZeros: bool('Rellena con ceros a la izquierda.', { default: false }),
      thousandsSep: oneOf(['', ',', '.', ' '], 'Separador de miles (NUMBER). Sin decimales declara un entero de 32 bits.', { doc: '' }),
      durationPattern: oneOf(['SS', 'MM_SS', 'HH_MM_SS', 'COMPACT'], 'Formato de la duración (DURATION).', { doc: 'MM_SS' }),
      durationUnit: oneOf(['S', 'MS'], 'Unidad de la variable de duración (DURATION).', { doc: 'S' }),
      dateTimePattern: oneOf(['HH_MM', 'HH_MM_SS', 'DD_MM', 'DD_MM_YYYY', 'DD_MM_YYYY_HH_MM'], 'Formato de fecha/hora (DATETIME).', { doc: 'HH_MM' })
    }
  },
  BOOL: {
    group: 'campo',
    desc: 'Estado ON/OFF. Declara `bool`. Muestra un texto por estado o un piloto (círculo).',
    props: {
      ...FIELD,
      align: { ...FIELD.align, default: 'CENTER' },
      falseText: str('Texto para false.', { default: 'OFF' }),
      trueText: str('Texto para true.', { default: 'ON' }),
      boolStyle: oneOf(['TEXT', 'PILOT'], 'TEXT: texto por estado. PILOT: círculo de color (7 px × valueSize).', { doc: 'TEXT' }),
      stateColors: bool('TEXT: colores distintos por estado (falseColor/trueColor y fondos).', { doc: false }),
      falseColor: color('Color del texto o del piloto en false.'),
      trueColor: color('Color del texto o del piloto en true.'),
      falseBackground: color('Fondo del campo en false.'),
      trueBackground: color('Fondo del campo en true.'),
      boolValue: bool('Estado de muestra en el lienzo.', { default: false }),
      capacity: int(1, 39, 'Uso interno del Designer.', { default: 3, internal: true })
    }
  },
  BAR: {
    group: 'campo',
    desc: 'Barra de nivel horizontal (JWPLC_UIBarField). Declara `float`. El firmware dibuja sólo la barra lineal sólida.',
    props: {
      ...FIELD,
      layout: { ...FIELD.layout, default: 'STACKED' },
      valueSize: { ...FIELD.valueSize, default: 1 },
      showLabel: bool('Muestra barLabel.', { default: true }),
      barLabel: str('Etiqueta de la barra.', { default: 'Nivel' }),
      barUnit: str('Unidad tras el porcentaje.', { default: '%' }),
      barMin: num('Valor de la variable que dibuja la barra vacía.', { default: 0 }),
      barMax: num('Valor de la variable que dibuja la barra llena.', { default: 100 }),
      barValue: num('Valor de muestra en el lienzo.', { default: 50 }),
      barWidth: int(20, SCREEN.width, 'Ancho total del campo (px).', { default: 110 }),
      trackColor: color('Fondo de la pista (parte vacía).', { default: 0x0000 }),
      valueColor: { ...FIELD.valueColor, desc: 'Color del relleno.' },
      // Lo que sigue lo normaliza designer-bar.js (applyFirmwareCaps): el firmware no lo dibuja.
      capacity: int(1, 39, 'Uso interno.', { default: 1, internal: true }),
      barPreset: oneOf(['LINEAR', 'CIRCULAR', 'GRADIENT'], 'Sólo LINEAR (el resto cae a LINEAR).', { default: 'LINEAR', internal: true }),
      barFillMode: oneOf(['SOLID', 'GRADIENT'], 'Sólo SOLID.', { default: 'SOLID', internal: true }),
      barOrientation: oneOf(['HORIZONTAL', 'VERTICAL'], 'Sólo HORIZONTAL.', { default: 'HORIZONTAL', internal: true }),
      barStyle: { kind: 'string', desc: 'Sólo SOLID.', default: 'SOLID', internal: true },
      linearPctPos: { kind: 'string', desc: 'Sólo NONE.', default: 'NONE', internal: true },
      barCornerRadius: int(0, 20, 'Sólo 0.', { default: 0, internal: true }),
      barBorderEnabled: bool('Sólo false.', { default: false, internal: true }),
      barBorderColor: color('Sin uso.', { default: 0xFFFF, internal: true }),
      barBorderWidth: int(1, 4, 'Sin uso.', { default: 1, internal: true }),
      barAutoWidth: bool('Sólo false.', { default: false, internal: true }),
      barHeight: int(1, SCREEN.height, 'Fijo.', { default: 12, internal: true }),
      alignV: { kind: 'string', desc: 'Lo fija el Designer.', default: 'TOP', internal: true },
      alignH: { kind: 'string', desc: 'Lo fija el Designer.', default: 'LEFT', internal: true },
      valueColorEnd: color('Sin uso (degradado).', { default: 0x07E0, internal: true }),
      circleRadius: { kind: 'number', desc: 'Sin uso (circular).', default: 26, internal: true },
      circleThickness: { kind: 'number', desc: 'Sin uso.', default: 5, internal: true },
      circleSweep: { kind: 'number', desc: 'Sin uso.', default: 360, internal: true },
      circleDirection: { kind: 'string', desc: 'Sin uso.', default: 'CW', internal: true },
      circleStartAngle: { kind: 'string', desc: 'Sin uso.', default: 'TOP', internal: true },
      circleStyle: { kind: 'string', desc: 'Sin uso.', default: 'CONTINUOUS', internal: true },
      circleCenterMode: { kind: 'string', desc: 'Sin uso.', default: 'PERCENT', internal: true },
      circleColorMode: { kind: 'string', desc: 'Sin uso.', default: 'SOLID', internal: true },
      circleShowPercent: { kind: 'bool', desc: 'Sin uso.', default: true, internal: true },
      circleTextSize: { kind: 'number', desc: 'Sin uso.', default: 1, internal: true },
      circleTextColor: color('Sin uso.', { default: 0xFFFF, internal: true }),
      circleTrackColor: color('Sin uso.', { default: 0x0000, internal: true }),
      linearPctColor: color('Sin uso.', { default: 0xFFFF, internal: true }),
      transparentBackground: { kind: 'bool', desc: 'Sin uso.', internal: true }
    }
  },
  ICON: {
    group: 'campo',
    desc: 'Icono con estado (motor, piloto, válvula…; JWPLC_UIIconField): una imagen por estado, sin animación. Declara `bool` (2 estados) o `uint8_t` (0..N-1, hasta 8). Catálogo: «Iconos del catálogo» o `jwhmi icons`.',
    props: {
      ...FIELD,
      iconId: str('Icono del catálogo, `categoria/nombre` (p. ej. `motores/motor`).', { required: true }),
      iconSize: oneOf([24, 32, 48, 64], 'Lado del icono (px). Sólo los tamaños que tiene ese icono (64 en algunos).', { default: 32 }),
      iconVarType: oneOf(['bool', 'uint8'], 'Tipo de la variable: bool usa los estados 0 y 1; uint8 todos (0..N-1). Por defecto bool con 2 estados y uint8 con más.', { doc: 'bool' }),
      iconState: int(0, 7, 'Estado de muestra en el lienzo.', { default: 0 }),
      label: str('Etiqueta opcional (arriba con STACKED, a la izquierda con INLINE). Vacía = sin etiqueta.', { default: '' }),
      layout: { ...FIELD.layout, default: 'STACKED' },
      valueColor: color('Sin uso (los colores salen del icono).', { default: 0xFFFF, internal: true }),
      backgroundColor: color('Fondo del campo; también el de los píxeles transparentes del icono.', { default: 0x0000 }),
      transparentBackground: bool('Sin fondo: no pinta el recuadro ni detrás de la etiqueta, y lo transparente en todos los estados deja ver la gráfica estática (RECT, RAW_TEXT…). Lo que cambia entre estados usa backgroundColor.', { default: false }),
      valueSize: int(1, 4, 'Uso interno.', { default: 1, internal: true }),
      align: { ...FIELD.align, default: 'LEFT', internal: true },
      unit: str('Sin uso.', { default: '', internal: true }),
      preview: str('Sin uso.', { internal: true }),
      capacity: int(1, 39, 'Uso interno.', { default: 1, internal: true })
    }
  },
  MENU: {
    group: 'menu',
    desc: 'Árbol de menús para ver y editar variables con la botonera. Uno por página, hasta 4 en el proyecto, 64 nodos y 3 niveles.',
    props: {
      ...BASE,
      ...POS,
      title: str('Título de la cabecera.', { default: 'Ajustes' }),
      width: int(40, SCREEN.width, 'Ancho (px).', { default: 300 }),
      height: int(30, SCREEN.height, 'Alto (px). Filas de 12 px (textSize 1) o 20 px (textSize 2) bajo la cabecera.', { default: 124 }),
      textSize: oneOf([1, 2], 'Escala del texto.', { default: 1 }),
      frame: bool('Marco exterior.', { default: true }),
      cornerRadius: int(0, 20, 'Uso interno.', { default: 0, internal: true }),
      colors: { kind: 'object', desc: 'Colores: text, background, value, selectText, selectBackground, editText, editBackground, title, titleBackground, frame. Los que falten toman su valor por defecto.', default: null },
      nodes: { kind: 'array', desc: 'Nodos del árbol (ver «Nodos de MENU»). Se pueden anidar con `children` en lugar de `parent`.', required: true }
    }
  },
  RAW_TEXT: {
    group: 'estatico',
    desc: 'Texto fijo (se dibuja una vez al entrar a la página). Admite fuentes GLCD, bitmap y FreeFonts.',
    props: {
      ...BASE,
      ...POS,
      text: str('Texto. Tildes, ñ y ° sí existen en GLCD.', { required: true }),
      font: oneOf(FONTS, 'Fuente. GLCD = clásica 6×8 escalable; FONT2/FONT4 bitmap de 16/26 px; FreeFonts SANS/SERIF/MONO [BOLD] 9/12/18/24 pt.', { default: 'GLCD' }),
      size: int(1, 4, 'Escala (GLCD y bitmap).', { default: 1 }),
      textColor: color('Color del texto.', { default: 0xFFFF }),
      backgroundColor: color('Fondo detrás del texto (si no es transparente).', { default: 0x0000 }),
      transparentBackground: bool('Sin fondo detrás del texto.', { default: true })
    }
  }
};

// Figuras estáticas: dos puntos (x,y)-(x2,y2) que definen la caja (o los extremos de la línea).
const SHAPE = {
  ...BASE,
  ...POS,
  x2: int(0, SCREEN.width - 1, 'Esquina opuesta (o extremo de la línea), X.', { required: true }),
  y2: int(0, SCREEN.height - 1, 'Esquina opuesta (o extremo de la línea), Y.', { required: true }),
  frameColor: color('Color del contorno (o de la línea).', { default: 0xFFFF }),
  fill: bool('Relleno.', { default: false }),
  fillColor: color('Color de relleno.', { default: 0x001F }),
  borderEnabled: bool('Dibuja el contorno (con relleno se puede quitar).', { doc: true }),
  size: int(1, 8, 'Grosor del trazo (px).', { default: 1 }),
  rotation: num('Rotación en grados alrededor del centro.', { doc: 0 }),
  cornerRadius: int(0, 85, 'Radio de esquinas (RECT).', { default: 0 }),
  sides: int(3, 12, 'Lados (POLYGON).', { default: 5 }),
  variable: { kind: 'string', desc: 'Sin uso.', default: '', internal: true },
  backgroundColor: color('Sin uso.', { default: 0x0000, internal: true })
};
const SHAPE_DESC = {
  LINE: 'Línea de (x,y) a (x2,y2).',
  RECT: 'Rectángulo (cornerRadius para esquinas redondeadas).',
  ELLIPSE: 'Elipse inscrita en la caja.',
  TRIANGLE: 'Triángulo isósceles inscrito en la caja.',
  POLYGON: 'Polígono regular de `sides` lados inscrito en la caja.'
};
for (const [type, desc] of Object.entries(SHAPE_DESC)) TYPES[type] = { group: 'estatico', desc, props: { ...SHAPE } };

TYPES.NAV_BAR = {
  group: 'navegacion',
  desc: 'Barra de navegación entre páginas (◀ ▶ de la botonera). Global: una sola, se ve en todas las páginas.',
  props: {
    type: BASE.type,
    name: { ...BASE.name, default: 'Barra de Navegación' },
    key: { kind: 'string', desc: 'Siempre NAV_BAR.', default: 'NAV_BAR', internal: true },
    label: { kind: 'string', desc: 'Siempre NAV_BAR.', default: 'NAV_BAR', internal: true },
    preset: oneOf(['industrial-color', 'dark-pill', 'segmented-bar', 'compact-dots', 'edge-tabs', 'numeric-pills', 'counter'], 'Estilo. counter: sólo el indicador NN/TT que pinta el firmware (sin barra).', { default: 'industrial-color' }),
    position: oneOf(['bottom', 'top', 'left', 'right'], 'Borde de la pantalla.', { default: 'bottom' }),
    x: int(0, SCREEN.width - 1, 'X (se calcula de position si falta).'),
    y: int(0, SCREEN.height - 1, 'Y (se calcula de position si falta).'),
    width: int(8, SCREEN.width, 'Ancho (320 horizontal, ~24 vertical).'),
    height: int(8, SCREEN.height, 'Alto (~22 horizontal, 170 vertical).', { default: 22 }),
    tabDistribution: oneOf(['center', 'start', 'end', 'stretch'], 'Reparto de las pestañas.', { doc: 'center' }),
    textAlign: oneOf(['center', 'left', 'right'], 'Texto dentro de la pestaña.', { doc: 'center' }),
    fontSize: oneOf(['normal', 'medium', 'micro'], 'Tamaño del texto.', { doc: 'normal' }),
    labelFormat: oneOf(['short', 'number', 'num_short'], 'short: nombre corto de la página; number: 1, 2, 3; num_short: ambos.', { doc: 'short' }),
    bgMode: oneOf(['solid', 'dark', 'transparent'], 'Fondo de la barra.', { doc: 'solid' }),
    borderMode: oneOf(['none', 'line', 'frame'], 'Borde de la barra.', { doc: 'none' }),
    radius: int(0, 11, 'Radio de las pestañas.', { doc: 3 }),
    gap: int(0, 10, 'Separación entre pestañas.', { doc: 3 }),
    orientation: { kind: 'string', desc: 'Sale de position.', internal: true },
    global: { kind: 'bool', desc: 'Siempre true.', default: true, internal: true },
    hidden: { kind: 'bool', desc: 'Oculta = sin barra.', default: false, internal: true },
    locked: { kind: 'bool', desc: 'Bloqueo en el lienzo.', default: false, internal: true },
    layerOrder: { kind: 'int', desc: 'Siempre al frente.', default: 999, internal: true },
    page: { kind: 'int', desc: 'Sin uso (es global).', internal: true },
    cornerRadius: { kind: 'int', desc: 'Lo agrega el Designer al abrir.', internal: true }
  }
};

const MENU_NODE = {
  key: str('Clave única del nodo dentro del MENU (se genera si falta).'),
  parent: { kind: 'string', desc: 'Clave del nodo padre (SUBMENU). null = raíz. Con `children` no hace falta.', default: null },
  type: oneOf(['SUBMENU', 'VIEW', 'EDIT_NUM', 'EDIT_BOOL', 'GO_IDLE'], 'SUBMENU: abre hijos. VIEW: muestra una variable. EDIT_NUM/EDIT_BOOL: el operador la edita. GO_IDLE: vuelve a la pantalla de inicio.', { required: true }),
  label: str('Texto de la fila.', { required: true }),
  variable: { kind: 'symbol', desc: 'Variable C++ (VIEW, EDIT_NUM, EDIT_BOOL). Puede ser la misma que la de un campo si el tipo coincide.', default: '' },
  valueType: oneOf(['float', 'int32', 'bool', ''], 'VIEW: float|int32|bool. EDIT_NUM: float|int32. EDIT_BOOL: bool. SUBMENU/GO_IDLE: vacío.'),
  initial: num('Valor inicial de la variable.', { default: 0 }),
  min: num('Mínimo (EDIT_NUM).', { default: 0 }),
  max: num('Máximo (EDIT_NUM).', { default: 100 }),
  step: num('Paso de ▲▼ (EDIT_NUM). int32: entero.', { default: 1 }),
  decimals: int(0, 6, 'Decimales mostrados (float).', { default: 1 }),
  unit: str('Unidad.', { default: '' }),
  falseText: str('Texto para false (bool).', { default: 'OFF' }),
  trueText: str('Texto para true (bool).', { default: 'ON' }),
  children: { kind: 'array', desc: 'Atajo: hijos anidados de un SUBMENU (la normalización los aplana con `parent`).' }
};
const MENU_COLORS = Object.freeze({
  text: 0xFFFF, background: 0x0000, value: 0x07FF, selectText: 0x0000, selectBackground: 0x07FF,
  editText: 0x0000, editBackground: 0xFD20, title: 0x0000, titleBackground: 0xFFFF, frame: 0xFFFF
});
const MENU_KINDS = Object.freeze({ SUBMENU: [''], VIEW: ['float', 'int32', 'bool'], EDIT_NUM: ['float', 'int32'], EDIT_BOOL: ['bool'], GO_IDLE: [''] });

const PAGE = {
  id: int(0, LIMITS.pages - 1, 'Índice 0..N-1 (se asigna por posición si falta).'),
  name: str('Nombre de la página (también da el símbolo C++ PAGE_<NOMBRE>).', { required: true }),
  shortName: str('Nombre corto para las pestañas de la barra de navegación.'),
  color: color('Color de la pestaña de la página.'),
  fields: { kind: 'array', desc: 'Atajo: objetos de esta página (la normalización los pasa a `fields` con page = id).' }
};

const DISPLAY = {
  start: oneOf(['IDLE', 'HMI'], 'Al encender: pantalla de inicio JWPLC (IDLE) o directo al HMI.', { default: 'IDLE' }),
  wake: oneOf(['ANY', 'BUTTON', 'NONE'], 'Cómo se entra al HMI desde IDLE: cualquier tecla, una tecla (wakeButton) o nunca.', { default: 'ANY' }),
  wakeButton: oneOf(['BTN_LEFT', 'BTN_UP', 'BTN_RIGHT', 'BTN_ESC', 'BTN_OK', 'BTN_DOWN'], 'Tecla de entrada si wake = BUTTON.', { default: 'BTN_OK' }),
  back: oneOf(['ESC', 'TIMEOUT', 'NONE'], 'Cómo se vuelve a IDLE: ESC, por inactividad (timeoutS) o nunca.', { default: 'ESC' }),
  timeoutS: int(1, 3600, 'Segundos sin teclas para volver a IDLE (back = TIMEOUT).', { default: 15 }),
  backButton: { kind: 'string', desc: 'Retirado (proyectos anteriores); se ignora.', internal: true }
};

const PIXELMAP = {
  type: { kind: 'string', desc: 'Siempre PIXELMAP.', default: 'PIXELMAP' },
  name: str('Nombre de la capa.'),
  page: int(0, LIMITS.pages - 1, 'Página.', { default: 0 }),
  x: int(0, SCREEN.width - 1, 'Origen X.', { default: 0 }),
  y: int(0, SCREEN.height - 1, 'Origen Y.', { default: 0 }),
  pixels: { kind: 'array', desc: 'Píxeles {x, y, color} relativos al origen. Una capa = un solo color (para otro color, otra capa).', default: [] },
  editorVisible: { kind: 'bool', desc: 'Visibilidad en el lienzo.', default: true, internal: true },
  editorOpacity: { kind: 'number', desc: 'Opacidad en el lienzo.', default: 1, internal: true },
  layerOrder: { kind: 'int', desc: 'Orden de capa.', internal: true }
};

const PROJECT = {
  format: { kind: 'string', desc: 'Siempre JWPLC_HMI_PROJECT.', default: 'JWPLC_HMI_PROJECT' },
  version: { kind: 'int', desc: 'Siempre 1.', default: 1 },
  projectName: str('Nombre del proyecto (48 caracteres).', { default: 'HMI_ST7789' }),
  target: { kind: 'object', desc: 'Pantalla: ST7789 320×170, rotación 3, RGB565 (fijo).', internal: true },
  savedAt: { kind: 'string', desc: 'Fecha de guardado.', internal: true },
  activePage: { kind: 'int', desc: 'Página abierta en el Designer.', default: 0, internal: true },
  navigationBar: { kind: 'object', desc: 'Copia de la configuración de la NAV_BAR (la rellena la normalización).', internal: true },
  display: { kind: 'object', desc: 'Pantalla IDLE y arranque (ver «display»).' },
  pages: { kind: 'array', desc: 'Páginas (1 a 16).', required: true },
  fields: { kind: 'array', desc: 'Objetos (máximo 32 contando la NAV_BAR).', default: [] },
  pixelMaps: { kind: 'array', desc: 'Capas de píxeles (máximo 16).', default: [] },
  iconAssets: { kind: 'object', desc: 'Copia de los bitmaps de los ICON usados (la guarda el Designer).', internal: true },
  declarativeOnly: { kind: 'bool', desc: 'Siempre true.', default: true, internal: true },
  $schema: { kind: 'string', desc: 'Ruta opcional al esquema JSON (para editores).', internal: true }
};

const TARGET = Object.freeze({ display: 'ST7789', width: 320, height: 170, rotation: 3, pixelFormat: 'RGB565' });

module.exports = {
  SCREEN, LIMITS, NAMED_COLORS, FONTS, TYPES, MENU_NODE, MENU_COLORS, MENU_KINDS, PAGE, DISPLAY, PIXELMAP, PROJECT, TARGET,
  FIELD_TYPES: ['TEXT', 'VALUE', 'BOOL', 'BAR', 'ICON'],
  SHAPE_TYPES: Object.keys(SHAPE_DESC)
};

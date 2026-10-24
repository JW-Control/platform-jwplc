// Bloques de la extensión jwplc_basic para mBlock 5 (público: niños).
//
// Cada bloque declara:
//   category  categoría de la paleta (ver CATEGORIES)
//   text      texto por idioma; [ARG] marca los argumentos (formato mBlock)
//   arguments argumentos; los menús se definen con { label, value }
//   code      código Arduino en el punto del bloque (/*{ARG}*/ inserta el argumento)
//   sections  código para otras secciones de la plantilla (setup, loop, declare);
//             mBlock no repite una línea idéntica, así que los helpers comunes
//             pueden declararse en varios bloques.
//
// Todas las llamadas existen en el package 2.1.0-alpha.14:
// - E/S: digitalWrite/digitalRead (redirigidas a jwplc_* para I0_x/Q0_x),
//   JWPLC_readInputs/readOutputs/writeOutputs (core).
// - Pantalla: JWPLC_Display (setFields, setText, enterUserUI, goIdle) con
//   campos JWPLC_UITextField: sólo redibuja lo que cambia y no expone TFT_eSPI.
// - Botonera: JWPLC_Buttons.pressed/isDown/clearPendingInput (BTN_*).
// - Sonido: JWPLC_Buzzer (GPIO26, no bloqueante) de JWPLC_GlobalPeripherals 1.1.0.
// No se emite pinMode(): el core configura internamente I0_x/Q0_x.

'use strict';

const CATEGORIES = [
    { name: 'events', colors: ['#FFBF00', '#E6AC00', '#CC9900'], text: null },
    { name: 'jwplc_io', colors: ['#1E5FAF', '#1A549B', '#164887'], text: { en: 'Inputs & outputs', es: 'Entradas y salidas' } },
    { name: 'jwplc_screen', colors: ['#7E57C2', '#7048B6', '#5E3AA0'], text: { en: 'Screen', es: 'Pantalla' } },
    { name: 'jwplc_buttons', colors: ['#00A0A0', '#009090', '#008080'], text: { en: 'Buttons', es: 'Botonera' } },
    { name: 'jwplc_sound', colors: ['#CF63CF', '#C94FC9', '#BD42BD'], text: { en: 'Sound', es: 'Sonido' } },
];

const OUTPUT_MENU = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => ({ label: 'Q0.' + i, value: 'Q0_' + i }));
const INPUT_MENU = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => ({ label: 'I0.' + i, value: 'I0_' + i }));
const STATE_MENU = {
    en: [{ label: 'ON', value: 'HIGH' }, { label: 'OFF', value: 'LOW' }],
    es: [{ label: 'ENCENDIDA', value: 'HIGH' }, { label: 'APAGADA', value: 'LOW' }],
};
const BUTTON_MENU = {
    en: [
        { label: 'OK', value: 'BTN_OK' }, { label: 'ESC', value: 'BTN_ESC' },
        { label: '▲ up', value: 'BTN_UP' }, { label: '▼ down', value: 'BTN_DOWN' },
        { label: '◀ left', value: 'BTN_LEFT' }, { label: '▶ right', value: 'BTN_RIGHT' },
    ],
    es: [
        { label: 'OK', value: 'BTN_OK' }, { label: 'ESC', value: 'BTN_ESC' },
        { label: '▲ arriba', value: 'BTN_UP' }, { label: '▼ abajo', value: 'BTN_DOWN' },
        { label: '◀ izquierda', value: 'BTN_LEFT' }, { label: '▶ derecha', value: 'BTN_RIGHT' },
    ],
};
const NOTES = [
    ['C4', 'Do4', 262], ['D4', 'Re4', 294], ['E4', 'Mi4', 330], ['F4', 'Fa4', 349], ['G4', 'Sol4', 392],
    ['A4', 'La4', 440], ['B4', 'Si4', 494], ['C5', 'Do5', 523], ['D5', 'Re5', 587], ['E5', 'Mi5', 659],
    ['F5', 'Fa5', 698], ['G5', 'Sol5', 784], ['A5', 'La5', 880], ['B5', 'Si5', 988], ['C6', 'Do6', 1047],
];
const NOTE_MENU = {
    en: NOTES.map(([en, , hz]) => ({ label: en, value: String(hz) })),
    es: NOTES.map(([, es, hz]) => ({ label: es, value: String(hz) })),
};
const LINE_MENU = [1, 2, 3, 4].map((n) => ({ label: String(n), value: String(n) }));

// Pantalla para niños: 4 líneas de texto sobre los campos HMI de JWPLC_Display.
// setText() sólo invalida el campo si el texto cambió (dirty redraw), así que
// escribir en cada vuelta de loop() no carga el bus SPI.
const SCREEN_HELPERS = [
    '// Pantalla JWPLC: 4 líneas de texto (campos de JWPLC_Display).',
    'static const JWPLC_UIField _JW_SCREEN_FIELDS[] = {',
    '  JWPLC_UITextField(1, 10, 18, nullptr, 24),',
    '  JWPLC_UITextField(2, 10, 52, nullptr, 24),',
    '  JWPLC_UITextField(3, 10, 86, nullptr, 24),',
    '  JWPLC_UITextField(4, 10, 120, nullptr, 24)};',
    '',
    'void _jw_screen_begin() {',
    '  JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);',
    '  JWPLC_Display.setFields(_JW_SCREEN_FIELDS, 4);',
    '  JWPLC_Display.enterUserUI();',
    '}',
    '',
    'void _jw_line(int line, const String &text) {',
    '  if (line >= 1 && line <= 4) JWPLC_Display.setText((uint8_t)line, text.c_str());',
    '}',
    'void _jw_line(int line, const char *text) { _jw_line(line, String(text)); }',
    'void _jw_line(int line, double value) {',
    '  if (value == (double)(long)value) _jw_line(line, String((long)value));',
    '  else _jw_line(line, String(value, 2));',
    '}',
    'void _jw_line(int line, float value) { _jw_line(line, (double)value); }',
    'void _jw_line(int line, int value) { _jw_line(line, String(value)); }',
    'void _jw_line(int line, long value) { _jw_line(line, String(value)); }',
    'void _jw_line(int line, unsigned long value) { _jw_line(line, String(value)); }',
    'void _jw_line(int line, bool value) { _jw_line(line, String(value ? "SI" : "NO")); }',
    '',
    'void _jw_screen_clear() {',
    '  for (int i = 1; i <= 4; i++) _jw_line(i, "");',
    '}',
].join('\n');
const SCREEN_SECTIONS = { declare: SCREEN_HELPERS, setup: '_jw_screen_begin();' };

// "tocar nota/sonar durante" espera a que termine, como en mCore; el buzzer
// en sí es no bloqueante (esp_timer) y delay() cede la CPU al runtime.
const SOUND_HELPERS = [
    'void _jw_tone(double hz, double seconds) {',
    '  unsigned long ms = (unsigned long)(seconds * 1000);',
    '  if (ms > 65000UL) ms = 65000UL;',
    '  JWPLC_Buzzer.tone((uint16_t)hz, (uint16_t)ms);',
    '  delay(ms);',
    '}',
].join('\n');

const BLOCKS = [
    // ---- Eventos ---------------------------------------------------------
    {
        category: 'events',
        opcode: 'jwplc_when_start',
        blockType: 'hat',
        text: { en: 'when JWPLC Basic starts', es: 'cuando JWPLC Basic arranca' },
        arguments: {},
        code: '/*{$BRANCH}*/',
    },
    {
        category: 'events',
        opcode: 'jwplc_when_button',
        blockType: 'hat',
        text: { en: 'when button [B] is pressed', es: 'cuando se pulse el botón [B]' },
        arguments: { B: { type: 'fieldMenu', defaultValue: 'BTN_OK', menu: BUTTON_MENU } },
        code: '',
        sections: {
            setup: 'JWPLC_Buttons.clearPendingInput();',
            loop: 'if (JWPLC_Buttons.pressed(/*{B}*/)) {\n  /*{$BRANCH}*/\n}',
        },
    },
    {
        category: 'events',
        opcode: 'jwplc_when_input_on',
        blockType: 'hat',
        text: { en: 'when input [I] turns on', es: 'cuando la entrada [I] se active' },
        arguments: { I: { type: 'fieldMenu', defaultValue: 'I0_0', menu: INPUT_MENU } },
        code: '',
        sections: {
            loop: '{\n  static bool _jw_prev = false;\n  bool _jw_now = (digitalRead(/*{I}*/) == HIGH);\n' +
                '  if (_jw_now && !_jw_prev) {\n    /*{$BRANCH}*/\n  }\n  _jw_prev = _jw_now;\n}',
        },
    },
    {
        category: 'events',
        opcode: 'jwplc_every_seconds',
        blockType: 'hat',
        text: { en: 'every [SECS] seconds', es: 'cada [SECS] segundos' },
        arguments: { SECS: { type: 'number', defaultValue: 1 } },
        code: '',
        sections: {
            loop: '{\n  static unsigned long _jw_last = 0;\n' +
                '  if (millis() - _jw_last >= (unsigned long)((/*{SECS}*/) * 1000)) {\n' +
                '    _jw_last = millis();\n    /*{$BRANCH}*/\n  }\n}',
        },
    },

    // ---- Entradas y salidas ---------------------------------------------
    {
        category: 'jwplc_io',
        opcode: 'jwplc_write_output',
        blockType: 'command',
        text: { en: 'set output [Q] [STATE]', es: 'poner salida [Q] [STATE]' },
        arguments: {
            Q: { type: 'fieldMenu', defaultValue: 'Q0_0', menu: OUTPUT_MENU },
            STATE: { type: 'fieldMenu', defaultValue: 'HIGH', menu: STATE_MENU },
        },
        code: 'digitalWrite(/*{Q}*/, /*{STATE}*/);',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_toggle_output',
        blockType: 'command',
        text: { en: 'toggle output [Q]', es: 'conmutar salida [Q]' },
        arguments: { Q: { type: 'fieldMenu', defaultValue: 'Q0_0', menu: OUTPUT_MENU } },
        code: 'digitalWrite(/*{Q}*/, !digitalRead(/*{Q}*/));',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_all_outputs_off',
        blockType: 'command',
        text: { en: 'turn all outputs off', es: 'apagar todas las salidas' },
        arguments: {},
        code: 'JWPLC_writeOutputs(0);',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_read_input',
        blockType: 'boolean',
        text: { en: 'input [I] is on?', es: '¿entrada [I] activa?' },
        arguments: { I: { type: 'fieldMenu', defaultValue: 'I0_0', menu: INPUT_MENU } },
        code: '(digitalRead(/*{I}*/) == HIGH)',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_read_output',
        blockType: 'boolean',
        text: { en: 'output [Q] is on?', es: '¿salida [Q] activa?' },
        arguments: { Q: { type: 'fieldMenu', defaultValue: 'Q0_0', menu: OUTPUT_MENU } },
        code: '(digitalRead(/*{Q}*/) == HIGH)',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_write_outputs_byte',
        blockType: 'command',
        text: { en: 'set outputs Q0 to byte [VALUE]', es: 'escribir salidas Q0 con byte [VALUE]' },
        arguments: { VALUE: { type: 'number', defaultValue: 0 } },
        code: 'JWPLC_writeOutputs((uint8_t)(/*{VALUE}*/));',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_inputs_byte',
        blockType: 'number',
        text: { en: 'inputs I0 (byte)', es: 'entradas I0 (byte)' },
        arguments: {},
        code: 'JWPLC_readInputs()',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_outputs_byte',
        blockType: 'number',
        text: { en: 'outputs Q0 (byte)', es: 'salidas Q0 (byte)' },
        arguments: {},
        code: 'JWPLC_readOutputs()',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_millis',
        blockType: 'number',
        text: { en: 'milliseconds since start', es: 'milisegundos desde el arranque' },
        arguments: {},
        code: 'millis()',
    },
    {
        category: 'jwplc_io',
        opcode: 'jwplc_serial_print',
        blockType: 'command',
        text: { en: 'serial print [VALUE]', es: 'imprimir por serie [VALUE]' },
        arguments: { VALUE: { type: 'number', defaultValue: 0 } },
        code: 'Serial.println(/*{VALUE}*/);',
        sections: { setup: 'Serial.begin(115200);' },
    },

    // ---- Pantalla --------------------------------------------------------
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_write',
        blockType: 'command',
        text: { en: 'write [TEXT] on line [LINE]', es: 'escribir [TEXT] en la línea [LINE]' },
        arguments: {
            TEXT: { type: 'string', defaultValue: 'Hola' },
            LINE: { type: 'fieldMenu', defaultValue: '1', menu: LINE_MENU },
        },
        code: '_jw_line(/*{LINE}*/, /*{TEXT}*/);',
        sections: SCREEN_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_clear',
        blockType: 'command',
        text: { en: 'clear screen', es: 'limpiar pantalla' },
        arguments: {},
        code: '_jw_screen_clear();',
        sections: SCREEN_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_show_user',
        blockType: 'command',
        text: { en: 'show my screen', es: 'mostrar mi pantalla' },
        arguments: {},
        code: 'JWPLC_Display.enterUserUI();',
        sections: SCREEN_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_show_io',
        blockType: 'command',
        text: { en: 'show the inputs/outputs screen', es: 'mostrar la pantalla de entradas/salidas' },
        arguments: {},
        code: 'JWPLC_Display.goIdle();',
    },

    // ---- Botonera --------------------------------------------------------
    {
        category: 'jwplc_buttons',
        opcode: 'jwplc_button_down',
        blockType: 'boolean',
        text: { en: 'button [B] held down?', es: '¿botón [B] presionado?' },
        arguments: { B: { type: 'fieldMenu', defaultValue: 'BTN_OK', menu: BUTTON_MENU } },
        code: 'JWPLC_Buttons.isDown(/*{B}*/)',
    },

    // ---- Sonido ----------------------------------------------------------
    {
        category: 'jwplc_sound',
        opcode: 'jwplc_play_note',
        blockType: 'command',
        text: { en: 'play note [NOTE] for [SECS] seconds', es: 'tocar nota [NOTE] durante [SECS] segundos' },
        arguments: {
            NOTE: { type: 'fieldMenu', defaultValue: '262', menu: NOTE_MENU },
            SECS: { type: 'number', defaultValue: 0.5 },
        },
        code: '_jw_tone(/*{NOTE}*/, /*{SECS}*/);',
        sections: { declare: SOUND_HELPERS },
    },
    {
        category: 'jwplc_sound',
        opcode: 'jwplc_play_tone',
        blockType: 'command',
        text: { en: 'play sound at [HZ] Hz for [SECS] seconds', es: 'sonar a [HZ] Hz durante [SECS] segundos' },
        arguments: {
            HZ: { type: 'number', defaultValue: 700 },
            SECS: { type: 'number', defaultValue: 1 },
        },
        code: '_jw_tone(/*{HZ}*/, /*{SECS}*/);',
        sections: { declare: SOUND_HELPERS },
    },
    {
        category: 'jwplc_sound',
        opcode: 'jwplc_beep',
        blockType: 'command',
        text: { en: 'beep', es: 'pitido' },
        arguments: {},
        code: 'JWPLC_Buzzer.beep();',
    },
    {
        category: 'jwplc_sound',
        opcode: 'jwplc_stop_sound',
        blockType: 'command',
        text: { en: 'stop sound', es: 'detener sonido' },
        arguments: {},
        code: 'JWPLC_Buzzer.stop();',
    },
    {
        category: 'jwplc_sound',
        opcode: 'jwplc_set_volume',
        blockType: 'command',
        text: { en: 'set volume to [PCT] %', es: 'poner volumen al [PCT] %' },
        arguments: { PCT: { type: 'number', defaultValue: 50 } },
        code: 'JWPLC_Buzzer.setVolume((uint8_t)constrain((int)((/*{PCT}*/) * 255 / 100), 0, 255));',
    },
];

// Bloques estándar de control con código Arduino ajustado a la plantilla:
// "por siempre" no genera while(1): su cuerpo se emite en la sección loop.
// Las esperas usan delay(), que cede la CPU a jwplcSystemTask (prioridad 2).
const CONTROL_BLOCKS = [
    {
        opcode: 'control_wait', blockType: 'command', branchCount: 0,
        arguments: { DURATION: { type: 'number', defaultValue: 1 } },
        code: 'delay((unsigned long)((/*{DURATION}*/) * 1000));',
    },
    {
        opcode: 'control_forever', blockType: 'conditional', branchCount: 1, arguments: {},
        code: '',
        sections: { loop: '/*{$BRANCH1}*/' },
    },
    {
        opcode: 'control_repeat_until', blockType: 'conditional', branchCount: 1,
        arguments: { CONDITION: { type: 'boolean', defaultValue: false } },
        code: 'while(!(/*{{this.CONDITION ? this.CONDITION : 0}}*/)) {\n  /*{$BRANCH1}*/\n  delay(1);\n}',
    },
    {
        opcode: 'control_wait_until', blockType: 'command', branchCount: 0,
        arguments: { CONDITION: { type: 'boolean', defaultValue: false } },
        code: 'while(!(/*{{this.CONDITION ? this.CONDITION : 0}}*/)) {\n  delay(1);\n}',
    },
];

// Operadores estándar ajustados para niños: mBlock emite "a / b" y con dos
// enteros C++ hace división entera (10 / 3 = 3). Se fuerza coma flotante.
const OPERATOR_BLOCKS = [
    {
        opcode: 'operator_divide', blockType: 'number', branchCount: 0,
        arguments: { NUM1: { type: 'number', defaultValue: '' }, NUM2: { type: 'number', defaultValue: '' } },
        code: '((double)(/*{NUM1}*/) / (double)(/*{NUM2}*/))',
    },
];

module.exports = { CATEGORIES, BLOCKS, CONTROL_BLOCKS, OPERATOR_BLOCKS };

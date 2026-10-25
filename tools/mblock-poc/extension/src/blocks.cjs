// Bloques de la extensión jwplc_basic para mBlock 5 (público: niños).
//
// Cada bloque declara:
//   category  categoría de la paleta (ver CATEGORIES)
//   text      texto por idioma; [ARG] marca los argumentos (formato mBlock)
//   arguments argumentos; los menús se definen con { label, value }
//   code      código Arduino en el punto del bloque (/*{ARG}*/ inserta el argumento)
//   sections  código para otras secciones de la plantilla (include, setup, loop, declare);
//             mBlock no repite una línea idéntica, así que los helpers comunes
//             pueden declararse en varios bloques.
//
// Todas las llamadas existen en el package 2.1.0-alpha.14:
// - E/S: digitalWrite/digitalRead (redirigidas a jwplc_* para I0_x/Q0_x),
//   JWPLC_readInputs/readOutputs/writeOutputs (core).
// - Pantalla: JWPLC_Screen (JWPLC_mBlock.h) sobre JWPLC_Display (campos
//   JWPLC_UITextField: sólo redibuja lo que cambia, no expone TFT_eSPI),
//   más enterUserUI/goIdle directos y el cambio IDLE <-> mi pantalla con las
//   opciones de «Pantalla y arranque» del HMI Designer (setIdleWakeMode,
//   setIdleReturnMode, setIdleTimeoutMs).
// - Botonera: JWPLC_Keypad (JWPLC_mBlock.h) sobre JWPLC_Buttons.pressed/isDown:
//   no toma las teclas que en ese momento usa la pantalla (BTN_*).
// - Sonido: JWPLC_Buzzer (GPIO26, no bloqueante) de JWPLC_GlobalPeripherals 1.1.0
//   y JWPLC_Sound.play (JWPLC_mBlock.h) para "tocar ... durante".
// - Tiempo: JWPLC_Time (vista cacheada del RTC) y JWPLC_Timer/JWPLC_Clock.
// - Memoria: JWPLC_Memory sobre JWPLC_FRAM (bloques con checksum).
// - microSD: JWPLC_Files sobre JWPLC_SD (mutex del bus SPI).
// - WiFi: JWPLC_Net (JWPLC_mBlock_WiFi.h) con WiFi.h/WiFiUdp.h del core ESP32.
// - RS485: JWPLC_RS485 (begin) y JWPLC_Line485 (líneas de texto).
// - Ethernet: JWPLC_Ethernet (useDHCP/setStaticIP; el runtime la atiende).
// - Pantalla HMI: JWPLC_HMI.begin + JWPLC_HmiSet/Get (JWPLC_mBlock_HMI.h) sobre el
//   JWPLC_HMI_Generated.h del HMI Designer (jwplcHMISetup y sus variables).
// - Modbus: JWPLC_Registers (16 registros), JWPLC_ModbusEasy sobre JWPLC_ModbusRTU
//   (esclavo y maestro *Sync) y JWPLC_ModbusTcpEasy sobre JWPLC_ModbusTCP (servidor).
// No se emite pinMode(): el core configura internamente I0_x/Q0_x.

'use strict';

const CATEGORIES = [
    { name: 'events', colors: ['#FFBF00', '#E6AC00', '#CC9900'], text: null },
    { name: 'jwplc_io', colors: ['#1E5FAF', '#1A549B', '#164887'], text: { en: 'Inputs & outputs', es: 'Entradas y salidas' } },
    { name: 'jwplc_screen', colors: ['#7E57C2', '#7048B6', '#5E3AA0'], text: { en: 'Screen', es: 'Pantalla' } },
    { name: 'jwplc_hmi', colors: ['#5C6BC0', '#5260B0', '#45519A'], text: { en: 'HMI screen', es: 'Pantalla HMI' } },
    { name: 'jwplc_buttons', colors: ['#00A0A0', '#009090', '#008080'], text: { en: 'Buttons', es: 'Botonera' } },
    { name: 'jwplc_sound', colors: ['#CF63CF', '#C94FC9', '#BD42BD'], text: { en: 'Sound', es: 'Sonido' } },
    { name: 'jwplc_time', colors: ['#5CB1D6', '#47A8D1', '#2E8EB8'], text: { en: 'Time', es: 'Tiempo' } },
    { name: 'jwplc_memory', colors: ['#E07B39', '#D86D29', '#C25F20'], text: { en: 'Memory', es: 'Memoria' } },
    { name: 'jwplc_sd', colors: ['#8C7AE6', '#7D6AD9', '#6C59C9'], text: { en: 'microSD', es: 'microSD' } },
    { name: 'jwplc_wifi', colors: ['#2EB872', '#28A465', '#209055'], text: { en: 'WiFi', es: 'WiFi' } },
    { name: 'jwplc_rs485', colors: ['#607D8B', '#56717E', '#4B6370'], text: { en: 'RS485', es: 'RS485' } },
    { name: 'jwplc_ethernet', colors: ['#3F7CAC', '#386F9A', '#306088'], text: { en: 'Ethernet', es: 'Ethernet' } },
    { name: 'jwplc_modbus', colors: ['#795548', '#6D4C41', '#5D4037'], text: { en: 'Modbus', es: 'Modbus' } },
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
// «abrir mi pantalla con…»: como «Entrar al HMI» del Designer.
const WAKE_MENU = {
    en: [{ label: 'any button', value: 'JWPLC_ANY_BUTTON' }, ...BUTTON_MENU.en,
        { label: 'no button', value: 'JWPLC_NO_BUTTON' }],
    es: [{ label: 'cualquier botón', value: 'JWPLC_ANY_BUTTON' }, ...BUTTON_MENU.es,
        { label: 'ningún botón', value: 'JWPLC_NO_BUTTON' }],
};
// «volver … con…»: como «Volver a la IDLE» del Designer (ESC o nunca; el tiempo
// va en su propio bloque).
const BACK_MENU = {
    en: [{ label: 'ESC', value: 'BTN_ESC' }, { label: 'no button', value: 'JWPLC_NO_BUTTON' }],
    es: [{ label: 'ESC', value: 'BTN_ESC' }, { label: 'ningún botón', value: 'JWPLC_NO_BUTTON' }],
};
const LINE_MENU = [1, 2, 3, 4].map((n) => ({ label: String(n), value: String(n) }));

// Pantalla y sonido usan JWPLC_mBlock.h (src/arduino/), que la extensión
// envía junto al sketch: el código de la vista previa queda en llamadas
// cortas, como MeMCore.h en mBot. mBlock antepone "#include " a cada include.
const LIB_INCLUDE = '"JWPLC_mBlock.h"';
const SCREEN_SECTIONS = { include: [LIB_INCLUDE], setup: 'JWPLC_Screen.begin();' };
const SOUND_SECTIONS = { include: [LIB_INCLUDE] };
const LIB_SECTIONS = { include: [LIB_INCLUDE] };
// WiFi en un header aparte: los programas sin WiFi no compilan la librería WiFi.
const WIFI_SECTIONS = { include: ['"JWPLC_mBlock_WiFi.h"'] };
// RS485, Modbus RTU, Ethernet y Modbus TCP: un header cada uno, por la misma razón.
const RS485_SECTIONS = { include: ['"JWPLC_mBlock_RS485.h"'] };
const MODBUS_RTU_SECTIONS = { include: ['"JWPLC_mBlock_ModbusRTU.h"'] };
// Iniciar Modbus RTU añade el servicio de tramas en cada vuelta de loop().
const MODBUS_RTU_START_SECTIONS = { include: ['"JWPLC_mBlock_ModbusRTU.h"'], loop: 'JWPLC_ModbusRTU.task();' };
const ETHERNET_SECTIONS = { include: ['"JWPLC_mBlock_Ethernet.h"'] };
const MODBUS_TCP_SECTIONS = { include: ['"JWPLC_mBlock_ModbusTCP.h"'] };
// Pantalla del HMI Designer: el bridge añade su JWPLC_HMI_Generated.h.
const HMI_SECTIONS = { include: ['"JWPLC_mBlock_HMI.h"'] };
// Nombre de una variable del Designer escrito en un hueco de texto: mBlock lo
// entrega entre comillas; la expresión lo deja como identificador C++.
const HMI_VAR = "/*{{String(this.VAR).replace(/[^A-Za-z0-9_]/g, '')}}*/";

const BAUD_MENU = [9600, 19200, 38400, 57600, 115200].map((b) => ({ label: String(b), value: String(b) }));

const TIME_UNIT_MENU = {
    en: [
        { label: 'hour', value: 'hour' }, { label: 'minute', value: 'minute' }, { label: 'second', value: 'second' },
        { label: 'day', value: 'day' }, { label: 'month', value: 'month' }, { label: 'year', value: 'year' },
        { label: 'day of week (1 = Sunday)', value: 'dayOfWeek' },
    ],
    es: [
        { label: 'hora', value: 'hour' }, { label: 'minuto', value: 'minute' }, { label: 'segundo', value: 'second' },
        { label: 'día', value: 'day' }, { label: 'mes', value: 'month' }, { label: 'año', value: 'year' },
        { label: 'día de la semana (1 = domingo)', value: 'dayOfWeek' },
    ],
};

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
            include: [LIB_INCLUDE],
            setup: 'JWPLC_Buttons.clearPendingInput();',
            loop: 'if (JWPLC_Keypad.pressed(/*{B}*/)) {\n  /*{$BRANCH}*/\n}',
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
        code: 'JWPLC_Screen.print(/*{LINE}*/, /*{TEXT}*/);',
        sections: SCREEN_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_clear',
        blockType: 'command',
        text: { en: 'clear screen', es: 'limpiar pantalla' },
        arguments: {},
        code: 'JWPLC_Screen.clear();',
        sections: SCREEN_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_show_user',
        blockType: 'command',
        text: { en: 'show my screen', es: 'mostrar mi pantalla' },
        arguments: {},
        code: 'JWPLC_Display.enterUserUI();',
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_show_io',
        blockType: 'command',
        text: { en: 'show the inputs/outputs screen', es: 'mostrar la pantalla de entradas/salidas' },
        arguments: {},
        code: 'JWPLC_Display.goIdle();',
    },
    // Cambio entre la pantalla de entradas/salidas y mi pantalla. Valen para
    // las 4 líneas y para una pantalla del HMI Designer (no registran campos).
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_open_with',
        blockType: 'command',
        text: { en: 'open my screen with [B]', es: 'abrir mi pantalla con [B]' },
        arguments: { B: { type: 'fieldMenu', defaultValue: 'JWPLC_ANY_BUTTON', menu: WAKE_MENU } },
        code: 'JWPLC_Screen.openWith(/*{B}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_back_with',
        blockType: 'command',
        text: { en: 'return to the inputs/outputs screen with [B]', es: 'volver a la pantalla de entradas/salidas con [B]' },
        arguments: { B: { type: 'fieldMenu', defaultValue: 'BTN_ESC', menu: BACK_MENU } },
        code: 'JWPLC_Screen.closeWith(/*{B}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_back_after',
        blockType: 'command',
        text: {
            en: 'return to the inputs/outputs screen after [S] s without pressing buttons',
            es: 'volver a la pantalla de entradas/salidas tras [S] s sin pulsar botones',
        },
        arguments: { S: { type: 'number', defaultValue: 15 } },
        code: 'JWPLC_Screen.closeAfter(/*{S}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_screen',
        opcode: 'jwplc_screen_shown',
        blockType: 'boolean',
        text: { en: 'my screen shown?', es: '¿se ve mi pantalla?' },
        arguments: {},
        code: 'JWPLC_Screen.isShown()',
        sections: LIB_SECTIONS,
    },

    // ---- Botonera --------------------------------------------------------
    {
        category: 'jwplc_buttons',
        opcode: 'jwplc_button_down',
        blockType: 'boolean',
        text: { en: 'button [B] held down?', es: '¿botón [B] presionado?' },
        arguments: { B: { type: 'fieldMenu', defaultValue: 'BTN_OK', menu: BUTTON_MENU } },
        code: 'JWPLC_Keypad.isDown(/*{B}*/)',
        sections: LIB_SECTIONS,
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
        code: 'JWPLC_Sound.play(/*{NOTE}*/, /*{SECS}*/);',
        sections: SOUND_SECTIONS,
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
        code: 'JWPLC_Sound.play(/*{HZ}*/, /*{SECS}*/);',
        sections: SOUND_SECTIONS,
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
        code: 'JWPLC_Sound.volume(/*{PCT}*/);',
        sections: SOUND_SECTIONS,
    },

    // ---- Tiempo ----------------------------------------------------------
    {
        category: 'jwplc_time',
        opcode: 'jwplc_timer_seconds',
        blockType: 'number',
        text: { en: 'timer (s)', es: 'cronómetro (s)' },
        arguments: {},
        code: 'JWPLC_Timer.seconds()',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_timer_reset',
        blockType: 'command',
        text: { en: 'reset timer', es: 'reiniciar cronómetro' },
        arguments: {},
        code: 'JWPLC_Timer.reset();',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_unit',
        blockType: 'number',
        text: { en: 'current [UNIT]', es: '[UNIT] actual' },
        arguments: { UNIT: { type: 'fieldMenu', defaultValue: 'hour', menu: TIME_UNIT_MENU } },
        code: 'JWPLC_Time./*{UNIT}*/()',
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_time_text',
        blockType: 'string',
        text: { en: 'time as text (HH:MM:SS)', es: 'hora como texto (HH:MM:SS)' },
        arguments: {},
        code: 'JWPLC_Clock.timeText()',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_date_text',
        blockType: 'string',
        text: { en: 'date as text (DD/MM/YYYY)', es: 'fecha como texto (DD/MM/AAAA)' },
        arguments: {},
        code: 'JWPLC_Clock.dateText()',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_temperature',
        blockType: 'number',
        text: { en: 'temperature (°C)', es: 'temperatura (°C)' },
        arguments: {},
        code: 'JWPLC_Clock.temperature()',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_valid',
        blockType: 'boolean',
        text: { en: 'clock has the time?', es: '¿el reloj tiene hora?' },
        arguments: {},
        code: 'JWPLC_Time.valid()',
    },
    {
        category: 'jwplc_time',
        opcode: 'jwplc_clock_set_from_pc',
        blockType: 'command',
        text: { en: 'set the clock with the computer time', es: 'poner en hora el reloj con la computadora' },
        arguments: {},
        code: 'JWPLC_Clock.setFromComputer();',
        sections: LIB_SECTIONS,
    },

    // ---- Memoria (FRAM) --------------------------------------------------
    {
        category: 'jwplc_memory',
        opcode: 'jwplc_memory_save',
        blockType: 'command',
        text: { en: 'save [VALUE] in memory [SLOT]', es: 'guardar [VALUE] en la memoria [SLOT]' },
        arguments: {
            VALUE: { type: 'number', defaultValue: 0 },
            SLOT: { type: 'number', defaultValue: 1 },
        },
        code: 'JWPLC_Memory.save(/*{SLOT}*/, /*{VALUE}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_memory',
        opcode: 'jwplc_memory_load',
        blockType: 'number',
        text: { en: 'value of memory [SLOT]', es: 'valor de la memoria [SLOT]' },
        arguments: { SLOT: { type: 'number', defaultValue: 1 } },
        code: 'JWPLC_Memory.load(/*{SLOT}*/)',
        sections: LIB_SECTIONS,
    },

    // ---- microSD ---------------------------------------------------------
    {
        category: 'jwplc_sd',
        opcode: 'jwplc_sd_ready',
        blockType: 'boolean',
        text: { en: 'microSD card ready?', es: '¿hay tarjeta microSD?' },
        arguments: {},
        code: 'JWPLC_Files.ready()',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_sd',
        opcode: 'jwplc_sd_append',
        blockType: 'command',
        text: { en: 'write [TEXT] in file [FILE]', es: 'escribir [TEXT] en el archivo [FILE]' },
        arguments: {
            TEXT: { type: 'string', defaultValue: 'hola' },
            FILE: { type: 'string', defaultValue: 'datos.txt' },
        },
        code: 'JWPLC_Files.appendLine(/*{FILE}*/, /*{TEXT}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_sd',
        opcode: 'jwplc_sd_exists',
        blockType: 'boolean',
        text: { en: 'file [FILE] exists?', es: '¿existe el archivo [FILE]?' },
        arguments: { FILE: { type: 'string', defaultValue: 'datos.txt' } },
        code: 'JWPLC_Files.exists(/*{FILE}*/)',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_sd',
        opcode: 'jwplc_sd_remove',
        blockType: 'command',
        text: { en: 'delete file [FILE]', es: 'borrar el archivo [FILE]' },
        arguments: { FILE: { type: 'string', defaultValue: 'datos.txt' } },
        code: 'JWPLC_Files.remove(/*{FILE}*/);',
        sections: LIB_SECTIONS,
    },

    // ---- WiFi ------------------------------------------------------------
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_wifi_connect',
        blockType: 'command',
        text: { en: 'connect to WiFi [SSID] password [PASS]', es: 'conectar a WiFi [SSID] contraseña [PASS]' },
        arguments: {
            SSID: { type: 'string', defaultValue: 'MiRed' },
            PASS: { type: 'string', defaultValue: 'clave' },
        },
        code: 'JWPLC_Net.connect(/*{SSID}*/, /*{PASS}*/);',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_wifi_connected',
        blockType: 'boolean',
        text: { en: 'connected to WiFi?', es: '¿conectado a WiFi?' },
        arguments: {},
        code: 'JWPLC_Net.connected()',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_wifi_ip',
        blockType: 'string',
        text: { en: 'IP address', es: 'dirección IP' },
        arguments: {},
        code: 'JWPLC_Net.ip()',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_wifi_disconnect',
        blockType: 'command',
        text: { en: 'disconnect WiFi', es: 'desconectar WiFi' },
        arguments: {},
        code: 'JWPLC_Net.disconnect();',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_net_send',
        blockType: 'command',
        text: { en: 'send message [MSG] on the network', es: 'enviar mensaje [MSG] por la red' },
        arguments: { MSG: { type: 'string', defaultValue: 'hola' } },
        code: 'JWPLC_Net.send(/*{MSG}*/);',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_net_send_value',
        blockType: 'command',
        text: { en: 'send message [MSG] with value [VALUE] on the network', es: 'enviar mensaje [MSG] con valor [VALUE] por la red' },
        arguments: {
            MSG: { type: 'string', defaultValue: 'hola' },
            VALUE: { type: 'number', defaultValue: 1 },
        },
        code: 'JWPLC_Net.send(/*{MSG}*/, /*{VALUE}*/);',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_net_when_received',
        blockType: 'hat',
        text: { en: 'when I receive message [MSG]', es: 'cuando reciba el mensaje [MSG]' },
        arguments: { MSG: { type: 'string', defaultValue: 'hola' } },
        code: '',
        sections: {
            include: WIFI_SECTIONS.include,
            loop: 'if (JWPLC_Net.received(/*{MSG}*/)) {\n  /*{$BRANCH}*/\n}',
        },
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_net_value',
        blockType: 'number',
        text: { en: 'value received with message [MSG]', es: 'valor recibido del mensaje [MSG]' },
        arguments: { MSG: { type: 'string', defaultValue: 'hola' } },
        code: 'JWPLC_Net.value(/*{MSG}*/)',
        sections: WIFI_SECTIONS,
    },
    {
        category: 'jwplc_wifi',
        opcode: 'jwplc_net_channel',
        blockType: 'command',
        text: { en: 'set network channel to [CH]', es: 'poner canal de red [CH]' },
        arguments: { CH: { type: 'number', defaultValue: 1 } },
        code: 'JWPLC_Net.channel(/*{CH}*/);',
        sections: WIFI_SECTIONS,
    },

    // ---- RS485 -----------------------------------------------------------
    {
        category: 'jwplc_rs485',
        opcode: 'jwplc_rs485_begin',
        blockType: 'command',
        text: { en: 'start RS485 at [BAUD] baud', es: 'iniciar RS485 a [BAUD] baudios' },
        arguments: { BAUD: { type: 'fieldMenu', defaultValue: '115200', menu: BAUD_MENU } },
        code: 'JWPLC_RS485.begin(/*{BAUD}*/, SERIAL_8N1);',
        sections: RS485_SECTIONS,
    },
    {
        category: 'jwplc_rs485',
        opcode: 'jwplc_rs485_send_line',
        blockType: 'command',
        text: { en: 'send line [TEXT] by RS485', es: 'enviar línea [TEXT] por RS485' },
        arguments: { TEXT: { type: 'string', defaultValue: 'hola' } },
        code: 'JWPLC_Line485.send(/*{TEXT}*/);',
        sections: RS485_SECTIONS,
    },
    {
        category: 'jwplc_rs485',
        opcode: 'jwplc_rs485_when_line',
        blockType: 'hat',
        text: { en: 'when a line arrives by RS485', es: 'cuando llegue una línea por RS485' },
        arguments: {},
        code: '',
        sections: {
            include: RS485_SECTIONS.include,
            loop: 'if (JWPLC_Line485.available()) {\n  /*{$BRANCH}*/\n}',
        },
    },
    {
        category: 'jwplc_rs485',
        opcode: 'jwplc_rs485_last_line',
        blockType: 'string',
        text: { en: 'last line received by RS485', es: 'última línea recibida por RS485' },
        arguments: {},
        code: 'JWPLC_Line485.line()',
        sections: RS485_SECTIONS,
    },

    // ---- Ethernet --------------------------------------------------------
    {
        category: 'jwplc_ethernet',
        opcode: 'jwplc_eth_dhcp',
        blockType: 'command',
        text: { en: 'use Ethernet with automatic IP (DHCP)', es: 'usar Ethernet con IP automática (DHCP)' },
        arguments: {},
        code: 'JWPLC_EthernetEasy.useDHCP();',
        sections: ETHERNET_SECTIONS,
    },
    {
        category: 'jwplc_ethernet',
        opcode: 'jwplc_eth_static',
        blockType: 'command',
        text: { en: 'use Ethernet with IP [IP] and gateway [GW]', es: 'usar Ethernet con IP [IP] y puerta de enlace [GW]' },
        arguments: {
            IP: { type: 'string', defaultValue: '192.168.0.50' },
            GW: { type: 'string', defaultValue: '192.168.0.1' },
        },
        code: 'JWPLC_EthernetEasy.useStaticIP(/*{IP}*/, /*{GW}*/);',
        sections: ETHERNET_SECTIONS,
    },
    {
        category: 'jwplc_ethernet',
        opcode: 'jwplc_eth_ready',
        blockType: 'boolean',
        text: { en: 'Ethernet connected?', es: '¿Ethernet conectada?' },
        arguments: {},
        code: 'JWPLC_EthernetEasy.ready()',
        sections: ETHERNET_SECTIONS,
    },
    {
        category: 'jwplc_ethernet',
        opcode: 'jwplc_eth_ip',
        blockType: 'string',
        text: { en: 'Ethernet IP', es: 'IP de Ethernet' },
        arguments: {},
        code: 'JWPLC_EthernetEasy.ip()',
        sections: ETHERNET_SECTIONS,
    },

    // ---- Modbus ----------------------------------------------------------
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_reg_set',
        blockType: 'command',
        text: { en: 'set register [N] to [VALUE]', es: 'poner registro [N] a [VALUE]' },
        arguments: {
            N: { type: 'number', defaultValue: 1 },
            VALUE: { type: 'number', defaultValue: 0 },
        },
        code: 'JWPLC_Registers.set(/*{N}*/, /*{VALUE}*/);',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_reg_get',
        blockType: 'number',
        text: { en: 'value of register [N]', es: 'valor del registro [N]' },
        arguments: { N: { type: 'number', defaultValue: 1 } },
        code: 'JWPLC_Registers.get(/*{N}*/)',
        sections: LIB_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_mbrtu_slave',
        blockType: 'command',
        text: { en: 'start Modbus RTU slave with ID [ID] at [BAUD] baud', es: 'iniciar Modbus RTU esclavo con ID [ID] a [BAUD] baudios' },
        arguments: {
            ID: { type: 'number', defaultValue: 1 },
            BAUD: { type: 'fieldMenu', defaultValue: '115200', menu: BAUD_MENU },
        },
        code: 'JWPLC_ModbusEasy.beginSlave(/*{ID}*/, /*{BAUD}*/);',
        sections: MODBUS_RTU_START_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_mbrtu_master',
        blockType: 'command',
        text: { en: 'start Modbus RTU master at [BAUD] baud', es: 'iniciar Modbus RTU maestro a [BAUD] baudios' },
        arguments: { BAUD: { type: 'fieldMenu', defaultValue: '115200', menu: BAUD_MENU } },
        code: 'JWPLC_ModbusEasy.beginMaster(/*{BAUD}*/);',
        sections: MODBUS_RTU_START_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_mbrtu_read',
        blockType: 'number',
        text: { en: 'read register [N] of slave [ID]', es: 'leer registro [N] del esclavo [ID]' },
        arguments: {
            N: { type: 'number', defaultValue: 1 },
            ID: { type: 'number', defaultValue: 1 },
        },
        code: 'JWPLC_ModbusEasy.readRegister(/*{ID}*/, /*{N}*/)',
        sections: MODBUS_RTU_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_mbrtu_write',
        blockType: 'command',
        text: { en: 'write [VALUE] to register [N] of slave [ID]', es: 'escribir [VALUE] en el registro [N] del esclavo [ID]' },
        arguments: {
            VALUE: { type: 'number', defaultValue: 0 },
            N: { type: 'number', defaultValue: 1 },
            ID: { type: 'number', defaultValue: 1 },
        },
        code: 'JWPLC_ModbusEasy.writeRegister(/*{ID}*/, /*{N}*/, /*{VALUE}*/);',
        sections: MODBUS_RTU_SECTIONS,
    },
    {
        category: 'jwplc_modbus',
        opcode: 'jwplc_mbtcp_server',
        blockType: 'command',
        text: { en: 'start Modbus TCP server', es: 'iniciar servidor Modbus TCP' },
        arguments: {},
        code: 'JWPLC_ModbusTcpEasy.beginServer();',
        sections: MODBUS_TCP_SECTIONS,
    },

    // ---- Pantalla del HMI Designer --------------------------------------
    {
        // Botón de la paleta: abre el HMI Designer (protocolo jwplc-hmi://),
        // vinculado a la pantalla de «usar la pantalla…» si existe.
        // Su función está en runtime.js (JWPLC_BUTTONS).
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_open_designer',
        blockType: 'button',
        text: { en: 'Open HMI Designer', es: 'Abrir el HMI Designer' },
        arguments: {},
    },
    {
        // Botón de la paleta (no es un bloque de código): abre el diálogo de
        // archivos y pone la carpeta del diseño en «usar la pantalla…».
        // Su función está en runtime.js (JWPLC_BUTTONS).
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_pick',
        blockType: 'button',
        text: { en: 'Choose HMI Designer screen…', es: 'Elegir pantalla del HMI Designer…' },
        arguments: {},
    },
    {
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_use',
        blockType: 'command',
        text: { en: 'use the HMI Designer screen [NAME]', es: 'usar la pantalla del HMI Designer [NAME]' },
        arguments: { NAME: { type: 'string', defaultValue: 'MiPantalla' } },
        code: 'JWPLC_HMI.begin(/*{NAME}*/);',
        sections: HMI_SECTIONS,
    },
    {
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_set',
        blockType: 'command',
        text: { en: 'set screen variable [VAR] to [VALUE]', es: 'poner variable de pantalla [VAR] a [VALUE]' },
        arguments: {
            VAR: { type: 'string', defaultValue: 'temperatura' },
            VALUE: { type: 'string', defaultValue: '0' },
        },
        code: 'JWPLC_HmiSet(' + HMI_VAR + ', /*{VALUE}*/);',
        sections: HMI_SECTIONS,
    },
    {
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_get',
        blockType: 'number',
        text: { en: 'screen variable [VAR]', es: 'variable de pantalla [VAR]' },
        arguments: { VAR: { type: 'string', defaultValue: 'consigna' } },
        code: 'JWPLC_HmiGet(' + HMI_VAR + ')',
        sections: HMI_SECTIONS,
    },
    {
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_get_bool',
        blockType: 'boolean',
        text: { en: 'screen variable [VAR] is on?', es: '¿variable de pantalla [VAR] activa?' },
        arguments: { VAR: { type: 'string', defaultValue: 'modoAuto' } },
        code: '(JWPLC_HmiGet(' + HMI_VAR + ') != 0)',
        sections: HMI_SECTIONS,
    },
    {
        // Variable editable del MENU (EDIT_NUM / EDIT_BOOL) del diseño.
        category: 'jwplc_hmi',
        opcode: 'jwplc_hmi_when_menu_changed',
        blockType: 'hat',
        text: { en: 'when the operator changes [VAR] in the MENU', es: 'cuando el operador cambie [VAR] en el MENU' },
        arguments: { VAR: { type: 'string', defaultValue: 'consigna' } },
        code: '',
        sections: {
            include: HMI_SECTIONS.include,
            loop: 'if (JWPLC_HMI.changed(&' + HMI_VAR + ')) {\n  /*{$BRANCH}*/\n}',
        },
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
    {
        // "unir" estándar emite String(a) + String(b): con variables float
        // mostraría "3.00". JWPLC_Join usa el mismo formato que la pantalla.
        opcode: 'operator_join', blockType: 'string', branchCount: 0,
        arguments: { STRING1: { type: 'string', defaultValue: '' }, STRING2: { type: 'string', defaultValue: '' } },
        code: 'JWPLC_Join(/*{STRING1}*/, /*{STRING2}*/)',
        sections: LIB_SECTIONS,
    },
];

module.exports = { CATEGORIES, BLOCKS, CONTROL_BLOCKS, OPERATOR_BLOCKS };

#!/usr/bin/env node
/**
 * Genera la extensión Device "JWPLC Basic" para mBlock 5 sin pasar por el
 * Extension Builder web:
 *
 *   dist/jwplc_basic/            carpeta instalable en %USERPROFILE%\mblock\exts
 *   dist/jwplc_basic_v<ver>.mext paquete importable desde mBlock
 *
 * El index.js resultante sigue el mismo formato UMD que exporta el Extension
 * Builder (class con getInfo()/getHandler()), sin worker: los handlers son
 * vacíos y el trabajo real lo hace el driver de upload (src/runtime.js).
 *
 * Uso: node build-extension.cjs
 */
'use strict';

const fs = require('fs');
const path = require('path');
const zlib = require('zlib');
const { execFileSync } = require('child_process');

const EXT_ID = 'jwplc_basic';
const EXT_VERSION = '0.1.0';
const HERE = __dirname;
const DIST = path.join(HERE, 'dist');
const OUT = path.join(DIST, EXT_ID);
const ICON = 'jwplc_basic.svg';

const LANGS = ['zh', 'de', 'es', 'fr', 'id', 'ja', 'ja-jph', 'ko', 'pl', 'uk', 'zh-hant',
    'nl', 'it', 'hr', 'ru', 'pt', 'fi', 'tr', 'tk', 'en'];

// ---------------------------------------------------------------------------
// Plantilla Arduino C
//
// Sigue el modelo Arduino/PLC sin envoltorios:
// - lo que cuelga de "cuando JWPLC Basic arranca" va a setup() (una vez);
// - el cuerpo de "por siempre" va a loop() (ciclo de scan), sin while(1).
//
// Motivo: el core JWPLC sólo arranca jwplcSystemTask (E/S, RTC, Ethernet,
// display) cuando setup() retorna, así que setup() no debe quedarse en un
// bucle. La plantilla estándar de mBlock genera while(1) dentro de setup().
// ---------------------------------------------------------------------------
const TEMPLATE = [
    '// generado por mBlock 5 para JWPLC Basic (extensión jwplc_basic ' + EXT_VERSION + ')',
    '',
    '#include <Arduino.h>',
    '//( include //)',
    '//( lib //)',
    '',
    '//({',
    "    this.$ALL_VARIABLES.length==0?'':this.$ALL_VARIABLES.map(v=>\"float \"+v+\" = 0;\").join('\\n')",
    '}//)',
    '',
    '//( declare //)',
    '',
    'void setup() {',
    '  //( setup //)',
    '  //( code //)',
    '}',
    '',
    'void loop() {',
    '  //( loop //)',
    '}',
    '',
].join('\n');

const OUTPUT_MENU = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => ({ label: 'Q0.' + i, value: 'Q0_' + i }));
const INPUT_MENU = [0, 1, 2, 3, 4, 5, 6, 7].map((i) => ({ label: 'I0.' + i, value: 'I0_' + i }));
const STATE_MENU = {
    en: [{ label: 'ON', value: 'HIGH' }, { label: 'OFF', value: 'LOW' }],
    es: [{ label: 'ENCENDIDA', value: 'HIGH' }, { label: 'APAGADA', value: 'LOW' }],
};

// Bloques JWPLC. `text` usa [ARG] para los argumentos (formato mBlock).
// No se emite pinMode(): el core JWPLC configura internamente la dirección
// de I0_x/Q0_x, que es justamente una de sus ventajas frente a un ESP32 pelado.
// Las llamadas emitidas existen en el core: digitalWrite/digitalRead se
// redirigen a jwplc_* para los pines virtuales I0_x/Q0_x (Arduino.h,
// jwplc_peripherals.h) y JWPLC_readInputs/readOutputs/writeOutputs son API
// pública del core.
const BLOCKS = [
    {
        opcode: 'jwplc_when_start',
        blockType: 'hat',
        text: { en: 'when JWPLC Basic starts', es: 'cuando JWPLC Basic arranca' },
        arguments: {},
        code: '/*{$BRANCH}*/',
    },
    {
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
        opcode: 'jwplc_toggle_output',
        blockType: 'command',
        text: { en: 'toggle output [Q]', es: 'conmutar salida [Q]' },
        arguments: { Q: { type: 'fieldMenu', defaultValue: 'Q0_0', menu: OUTPUT_MENU } },
        code: 'digitalWrite(/*{Q}*/, !digitalRead(/*{Q}*/));',
    },
    {
        opcode: 'jwplc_read_input',
        blockType: 'boolean',
        text: { en: 'input [I] is on?', es: '¿entrada [I] activa?' },
        arguments: { I: { type: 'fieldMenu', defaultValue: 'I0_0', menu: INPUT_MENU } },
        code: '(digitalRead(/*{I}*/) == HIGH)',
    },
    {
        opcode: 'jwplc_read_output',
        blockType: 'boolean',
        text: { en: 'output [Q] is on?', es: '¿salida [Q] activa?' },
        arguments: { Q: { type: 'fieldMenu', defaultValue: 'Q0_0', menu: OUTPUT_MENU } },
        code: '(digitalRead(/*{Q}*/) == HIGH)',
    },
    {
        opcode: 'jwplc_write_outputs_byte',
        blockType: 'command',
        text: { en: 'set outputs Q0 to byte [VALUE]', es: 'escribir salidas Q0 con byte [VALUE]' },
        arguments: { VALUE: { type: 'number', defaultValue: 0 } },
        code: 'JWPLC_writeOutputs((uint8_t)(/*{VALUE}*/));',
    },
    {
        opcode: 'jwplc_all_outputs_off',
        blockType: 'command',
        text: { en: 'turn all outputs off', es: 'apagar todas las salidas' },
        arguments: {},
        code: 'JWPLC_writeOutputs(0);',
    },
    {
        opcode: 'jwplc_inputs_byte',
        blockType: 'number',
        text: { en: 'inputs I0 (byte)', es: 'entradas I0 (byte)' },
        arguments: {},
        code: 'JWPLC_readInputs()',
    },
    {
        opcode: 'jwplc_outputs_byte',
        blockType: 'number',
        text: { en: 'outputs Q0 (byte)', es: 'salidas Q0 (byte)' },
        arguments: {},
        code: 'JWPLC_readOutputs()',
    },
    {
        opcode: 'jwplc_millis',
        blockType: 'number',
        text: { en: 'milliseconds since start', es: 'milisegundos desde el arranque' },
        arguments: {},
        code: 'millis()',
    },
    {
        opcode: 'jwplc_serial_print',
        blockType: 'command',
        text: { en: 'serial print [VALUE]', es: 'imprimir por serie [VALUE]' },
        arguments: { VALUE: { type: 'number', defaultValue: 0 } },
        code: 'Serial.println(/*{VALUE}*/);',
        sections: { setup: 'Serial.begin(115200);' },
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

const PLATFORM = ['mblockpc'];
const CATEGORY = 'jwplc_io';

function menuKey(opcode, arg) {
    return (opcode + '_' + arg).toUpperCase();
}

function arduinoCodes(b) {
    return { arduinoc: b.sections ? { code: b.code, sections: b.sections } : { code: b.code } };
}

function buildInfoData() {
    const menus = {};
    const translations = {};
    for (const lang of LANGS) translations[lang] = { [EXT_ID]: 'JWPLC Basic', extensionName: 'JWPLC Basic', [CATEGORY]: 'JWPLC E/S' };
    translations.en[CATEGORY] = 'JWPLC I/O';

    const t = (key, byLang) => {
        for (const lang of LANGS) translations[lang][key] = byLang[lang] || byLang.en;
    };

    const blocks = BLOCKS.map((b) => {
        const args = {};
        for (const [name, a] of Object.entries(b.arguments)) {
            if (a.type === 'fieldMenu') {
                const key = menuKey(b.opcode, name);
                const byLang = Array.isArray(a.menu) ? { en: a.menu } : a.menu;
                menus[key] = byLang.en.map((item, i) => ({ text: key + '_' + i, value: item.value }));
                byLang.en.forEach((item, i) => {
                    const perLang = {};
                    for (const [lang, list] of Object.entries(byLang)) perLang[lang] = list[i].label;
                    t(key + '_' + i, perLang);
                });
                args[name] = { type: 'fieldMenu', defaultValue: a.defaultValue, menu: key };
            } else {
                args[name] = { type: a.type, defaultValue: a.defaultValue };
            }
        }
        t(b.opcode, b.text);
        return {
            opcode: b.opcode,
            blockType: b.blockType,
            checkboxInFlyout: false,
            hidden: false,
            gap: 12,
            arguments: args,
            branchCount: 0,
            platform: PLATFORM,
            codes: arduinoCodes(b),
        };
    });

    const controlBlocks = CONTROL_BLOCKS.map((b) => ({
        opcode: b.opcode,
        blockType: b.blockType,
        checkboxInFlyout: false,
        hidden: false,
        gap: 12,
        arguments: b.arguments,
        branchCount: b.branchCount,
        platform: PLATFORM,
        codes: arduinoCodes(b),
    }));

    const categories = [
        { name: CATEGORY, colors: ['#1E5FAF', '#1A549B', '#164887'], menuIconURI: '', blockIcon: null, blocks, menus },
        { name: 'control', colors: ['#FFAB19', '#FCA000', '#E08E00'], menuIconURI: '', blockIcon: null, blocks: controlBlocks, menus: {} },
    ];
    return { categories, translations };
}

function buildIndexJs() {
    const runtime = fs.readFileSync(path.join(HERE, 'src', 'runtime.js'), 'utf8');
    const { categories, translations } = buildInfoData();
    const generators = [{
        lang: 'arduinoc',
        template: TEMPLATE,
        splitor: { frame: { left: '//(', right: '//)' }, expression: { left: '/*{', right: '}*/' } },
    }];
    const classLines = `
const extTranslationMap = ${JSON.stringify(translations, null, 2)};

const extCategories = ${JSON.stringify(categories, null, 2)};

const extGenerators = ${JSON.stringify(generators, null, 2)};

// Reducer de includes (igual que el de mBlock): deduplica y antepone #include.
extGenerators[0].reducers = [{
    name: 'include',
    reduce: (codes) => {
        const seen = [];
        for (const code of codes) {
            const str = typeof code === 'function' ? code() : String(code);
            if (seen.indexOf(str) === -1) seen.push(str);
        }
        return seen.length === 0 ? undefined : seen.map((c) => '#include ' + c).join('\\n') + '\\n';
    },
}];

class ExtJwplcBasic {
    constructor() {
        this.funcs = {};
    }

    getInfo() {
        return {
            id: ${JSON.stringify(EXT_ID)},
            targets: {
                device: true,
                name: ${JSON.stringify(EXT_ID)},
                icon: window.MbApi.getExtResPath(${JSON.stringify(EXT_ID + '/imgs/' + ICON)}, ${JSON.stringify(EXT_ID)}),
                enableCode: true,
                enableUpload: ['serialport'],
                enableOnline: [],
                shouldConnect: ['serialport'],
                defaultOnline: false,
                options: {
                    connect: {
                        // mBlock compara este valor con el productId del puerto:
                        // 0x7523 = CH340 de la JWPLC Basic. Otros adaptadores:
                        // "Mostrar todos los dispositivos conectables".
                        serialport: { baudRate: '115200', vendorId: '0x7523' },
                    },
                    upload: {
                        middlewares: [{ name: 'jwplc_capture_port', func: jwplcCapturePortMiddleware }],
                        driver: { func: jwplcUploadDriver },
                    },
                    firmware: { driver: {} },
                },
                settings: [],
                categoriesOrder: [${JSON.stringify(CATEGORY)}, 'events', 'control', 'operators', 'data', 'myBlocks'],
            },
            codeTypes: ['arduinoc'],
            version: ${JSON.stringify(EXT_VERSION)},
            platform: ${JSON.stringify(PLATFORM)},
            categories: extCategories,
            generators: extGenerators,
            translationMap: extTranslationMap,
            snippets: { arduinoc: {} },
            excludeBlocks: [
                'event_whenflagclicked', 'event_whenkeypressed', 'event_whengreaterthan',
                'event_whenthisspriteclicked', 'event_whenbackdropswitchesto',
                'control_start_as_clone', 'control_create_clone_of', 'control_delete_this_clone',
                'control_stop', 'event_whenbroadcastreceived', 'event_broadcast', 'event_broadcastandwait',
            ],
            generatorStartBlocks: ['jwplc_when_start'],
            feature: [],
            mustLoginBlocks: [],
            disabledOffline: [],
            disabledOnline: [],
        };
    }

    getHandler() {
        return ExtHandler;
    }
}
`;
    return [
        '// Generado por tools/mblock-poc/extension/build-extension.cjs. No editar a mano.',
        '(function (global, factory) {',
        '  if (typeof define === "function" && define.amd) {',
        '    define(["exports"], factory);',
        '  } else if (typeof exports !== "undefined") {',
        '    factory(exports);',
        '  } else {',
        '    var mod = { exports: {} };',
        '    factory(mod.exports);',
        '    global.unknown = mod.exports;',
        '  }',
        '})(this, function (_exports) {',
        '"use strict";',
        'Object.defineProperty(_exports, "__esModule", { value: true });',
        runtime,
        classLines,
        '_exports.default = ExtJwplcBasic;',
        '});',
        '',
    ].join('\n');
}

function buildManifest() {
    const name = {};
    for (const lang of LANGS) name[lang] = 'JWPLC Basic';
    return {
        id: EXT_ID,
        iconURL: EXT_ID + '/imgs/' + ICON,
        coverURL: './imgs/' + ICON,
        name,
        description: { en: 'JW Control JWPLC Basic (ESP32) - experimental', es: 'JW Control JWPLC Basic (ESP32) - experimental' },
        main: EXT_ID + '/index.js',
        codeTypes: ['arduinoc'],
        device: true,
        sortId: 999,
        version: EXT_VERSION,
        extApiVersion: [],
        platform: PLATFORM,
        homepage: 'https://github.com/JW-Control/platform-jwplc',
        feature: [],
    };
}

// Zip mínimo (deflate) con rutas '/': Compress-Archive de Windows PowerShell
// escribe '\' como separador y algunos descompresores lo toman como nombre.
function writeZip(srcDir, zipFile) {
    const crcTable = [];
    for (let n = 0; n < 256; n++) {
        let c = n;
        for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1;
        crcTable[n] = c >>> 0;
    }
    const crc32 = (buf) => {
        let c = 0xFFFFFFFF;
        for (const byte of buf) c = crcTable[(c ^ byte) & 0xFF] ^ (c >>> 8);
        return (c ^ 0xFFFFFFFF) >>> 0;
    };

    const files = [];
    const walk = (dir, rel) => {
        for (const e of fs.readdirSync(dir, { withFileTypes: true })) {
            const r = rel ? rel + '/' + e.name : e.name;
            if (e.isDirectory()) walk(path.join(dir, e.name), r);
            else files.push(r);
        }
    };
    walk(srcDir, '');

    const parts = [];
    const central = [];
    let offset = 0;
    for (const name of files) {
        const data = fs.readFileSync(path.join(srcDir, ...name.split('/')));
        const comp = zlib.deflateRawSync(data);
        const nameBuf = Buffer.from(name, 'utf8');
        const crc = crc32(data);

        const local = Buffer.alloc(30);
        local.writeUInt32LE(0x04034b50, 0);
        local.writeUInt16LE(20, 4);
        local.writeUInt16LE(0x0800, 6);   // nombres UTF-8
        local.writeUInt16LE(8, 8);        // deflate
        local.writeUInt16LE(0x21, 12);    // fecha DOS 1980-01-01
        local.writeUInt32LE(crc, 14);
        local.writeUInt32LE(comp.length, 18);
        local.writeUInt32LE(data.length, 22);
        local.writeUInt16LE(nameBuf.length, 26);
        parts.push(local, nameBuf, comp);

        const entry = Buffer.alloc(46);
        entry.writeUInt32LE(0x02014b50, 0);
        entry.writeUInt16LE(20, 4);
        entry.writeUInt16LE(20, 6);
        entry.writeUInt16LE(0x0800, 8);
        entry.writeUInt16LE(8, 10);
        entry.writeUInt16LE(0x21, 14);
        entry.writeUInt32LE(crc, 16);
        entry.writeUInt32LE(comp.length, 20);
        entry.writeUInt32LE(data.length, 24);
        entry.writeUInt16LE(nameBuf.length, 28);
        entry.writeUInt32LE(offset, 42);
        central.push(entry, nameBuf);

        offset += 30 + nameBuf.length + comp.length;
    }
    const cd = Buffer.concat(central);
    const end = Buffer.alloc(22);
    end.writeUInt32LE(0x06054b50, 0);
    end.writeUInt16LE(files.length, 8);
    end.writeUInt16LE(files.length, 10);
    end.writeUInt32LE(cd.length, 12);
    end.writeUInt32LE(offset, 16);
    fs.writeFileSync(zipFile, Buffer.concat([...parts, cd, end]));
}

function main() {
    fs.rmSync(OUT, { recursive: true, force: true });
    fs.mkdirSync(path.join(OUT, 'imgs'), { recursive: true });
    fs.mkdirSync(path.join(OUT, 'i18n'), { recursive: true });

    fs.writeFileSync(path.join(OUT, 'manifest.json'), JSON.stringify(buildManifest(), null, 4));
    fs.writeFileSync(path.join(OUT, 'index.js'), buildIndexJs());
    fs.copyFileSync(path.join(HERE, 'assets', ICON), path.join(OUT, 'imgs', ICON));
    const { translations } = buildInfoData();
    for (const lang of LANGS) {
        fs.writeFileSync(path.join(OUT, 'i18n', lang + '.json'), JSON.stringify(translations[lang], null, 4));
    }

    // Validación sintáctica del index.js generado.
    execFileSync(process.execPath, ['--check', path.join(OUT, 'index.js')]);

    // .mext = zip con el contenido de la carpeta en la raíz.
    const mext = path.join(DIST, `${EXT_ID}_v${EXT_VERSION}.mext`);
    fs.rmSync(mext, { force: true });
    writeZip(OUT, mext);

    console.log('EXT_ID=' + EXT_ID);
    console.log('EXT_VERSION=' + EXT_VERSION);
    console.log('EXT_DIR=' + OUT);
    console.log('EXT_MEXT=' + mext);
}

main();

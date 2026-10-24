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
const EXT_VERSION = '0.5.0';
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

const { CATEGORIES, BLOCKS, CONTROL_BLOCKS, OPERATOR_BLOCKS } = require('./src/blocks.cjs');

const PLATFORM = ['mblockpc'];

function menuKey(opcode, arg) {
    return (opcode + '_' + arg).toUpperCase();
}

function arduinoCodes(b) {
    return { arduinoc: b.sections ? { code: b.code, sections: b.sections } : { code: b.code } };
}

function buildInfoData() {
    const translations = {};
    for (const lang of LANGS) translations[lang] = { [EXT_ID]: 'JWPLC Basic', extensionName: 'JWPLC Basic' };

    const t = (key, byLang) => {
        for (const lang of LANGS) translations[lang][key] = byLang[lang] || byLang.en;
    };

    const categories = CATEGORIES.map((c) => {
        if (c.text) t(c.name, c.text);
        return { name: c.name, colors: c.colors, menuIconURI: '', blockIcon: null, blocks: [], menus: {} };
    });
    const byName = Object.fromEntries(categories.map((c) => [c.name, c]));

    for (const b of BLOCKS) {
        const cat = byName[b.category];
        if (!cat) throw new Error('categoría desconocida ' + b.category + ' en ' + b.opcode);
        const args = {};
        for (const [name, a] of Object.entries(b.arguments)) {
            if (a.type === 'fieldMenu') {
                const key = menuKey(b.opcode, name);
                const byLang = Array.isArray(a.menu) ? { en: a.menu } : a.menu;
                cat.menus[key] = byLang.en.map((item, i) => ({ text: key + '_' + i, value: item.value }));
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
        cat.blocks.push({
            opcode: b.opcode,
            blockType: b.blockType,
            checkboxInFlyout: false,
            hidden: false,
            gap: 12,
            arguments: args,
            branchCount: 0,
            platform: PLATFORM,
            codes: arduinoCodes(b),
        });
    }

    // Sobrescrituras de bloques estándar (mismo opcode, código Arduino propio).
    const builtin = (b) => ({
        opcode: b.opcode,
        blockType: b.blockType,
        checkboxInFlyout: false,
        hidden: false,
        gap: 12,
        arguments: b.arguments,
        branchCount: b.branchCount,
        platform: PLATFORM,
        codes: arduinoCodes(b),
    });
    categories.push({
        name: 'control', colors: ['#FFAB19', '#FCA000', '#E08E00'], menuIconURI: '', blockIcon: null, menus: {},
        blocks: CONTROL_BLOCKS.map(builtin),
    });
    categories.push({
        name: 'operators', colors: ['#59C059', '#46B746', '#3EA33E'], menuIconURI: '', blockIcon: null, menus: {},
        blocks: OPERATOR_BLOCKS.map(builtin),
    });
    // mBlock sólo muestra las categorías que declara el Device: sin estas dos,
    // la paleta no tendría Variables ni Mis bloques. Vacías = bloques estándar.
    categories.push({ name: 'data', colors: ['#FF8C1A', '#FD7E00', '#E17000'], menuIconURI: '', blockIcon: null, menus: {}, blocks: [] });
    categories.push({ name: 'myBlocks', colors: ['#FF6680', '#FF4262', '#FF1F45'], menuIconURI: '', blockIcon: null, menus: {}, blocks: [] });
    return { categories, translations };
}

function buildIndexJs() {
    const runtime = fs.readFileSync(path.join(HERE, 'src', 'runtime.js'), 'utf8');
    // Archivos que viajan con cada sketch (los escribe el bridge junto al .ino).
    const sketchFiles = {};
    for (const name of fs.readdirSync(path.join(HERE, 'src', 'arduino'))) {
        sketchFiles[name] = fs.readFileSync(path.join(HERE, 'src', 'arduino', name), 'utf8');
    }
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
                categoriesOrder: ${JSON.stringify(CATEGORIES.map((c) => c.name).concat(['control', 'operators', 'data', 'myBlocks']))},
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
            generatorStartBlocks: ${JSON.stringify(BLOCKS.filter((b) => b.blockType === 'hat').map((b) => b.opcode))},
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
        'const JWPLC_SKETCH_FILES = ' + JSON.stringify(sketchFiles, null, 2) + ';',
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
    // Copia legible de las librerías del sketch (la extensión usa la versión embebida).
    fs.cpSync(path.join(HERE, 'src', 'arduino'), path.join(OUT, 'arduino'), { recursive: true });
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

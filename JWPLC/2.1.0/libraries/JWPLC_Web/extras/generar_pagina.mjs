#!/usr/bin/env node
// Genera src/JWPLC_Web_Page.h a partir de extras/panel.html (comprimida con gzip).
// Uso: node extras/generar_pagina.mjs   (desde la carpeta de la librería o cualquier otra)
import { readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { gzipSync } from "node:zlib";

const here = dirname(fileURLToPath(import.meta.url));
const html = readFileSync(join(here, "panel.html"));
// mtime 0 en la cabecera gzip: la misma página da siempre los mismos bytes.
const gz = gzipSync(html, { level: 9 });
gz[4] = gz[5] = gz[6] = gz[7] = 0;
const lines = [];
for (let i = 0; i < gz.length; i += 20) lines.push("    " + [...gz.subarray(i, i + 20)].map((b) => "0x" + b.toString(16).padStart(2, "0")).join(","));
const out = `// Generado por extras/generar_pagina.mjs a partir de extras/panel.html: no editar a mano.
// Página del panel (${html.length} bytes, ${gz.length} con gzip). Se sirve con Content-Encoding: gzip.
#pragma once

#include <stdint.h>

static const uint8_t JWPLC_WEB_PAGE_GZ[] = {
${lines.join(",\n")}
};
`;
writeFileSync(join(here, "..", "src", "JWPLC_Web_Page.h"), out);
console.log(`panel.html: ${html.length} B -> ${gz.length} B con gzip`);

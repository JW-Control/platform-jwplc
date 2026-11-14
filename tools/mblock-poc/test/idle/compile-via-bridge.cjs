// Compila un .ino por el bridge (POST /compile) con las librerías de la extensión,
// como el driver de mBlock; con un puerto, también lo sube (POST /upload).
// Uso: node compile-via-bridge.cjs programa.ino [COMn]   (JWPLC_BRIDGE_PORT/_HOME: otro bridge)
const fs = require('fs');
const os = require('os');
const path = require('path');
const ARDUINO = path.join(__dirname, '..', '..', 'extension', 'src', 'arduino');
const token = fs.readFileSync(path.join(process.env.JWPLC_BRIDGE_HOME || path.join(os.homedir(), '.jwplc', 'mblock-bridge'), 'token.txt'), 'utf8').trim();
const files = {};
for (const name of fs.readdirSync(ARDUINO)) files[name] = fs.readFileSync(path.join(ARDUINO, name), 'utf8');
const code = fs.readFileSync(process.argv[2], 'utf8');
const port = process.argv[3];
(async () => {
    const t0 = Date.now();
    const res = await fetch('http://127.0.0.1:' + (process.env.JWPLC_BRIDGE_PORT || 47820) + '/' + (port ? 'upload' : 'compile'), {
        method: 'POST',
        headers: { 'Content-Type': 'application/json', 'X-JWPLC-Token': token },
        body: JSON.stringify(port ? { code, files, port } : { code, files }),
    });
    const lines = (await res.text()).trim().split('\n').map((l) => JSON.parse(l));
    const logs = lines.filter((l) => l.type === 'log').map((l) => l.line);
    const errors = logs.filter((l) => /error|warning: .*JWPLC_mBlock/i.test(l));
    console.log('HTTP=' + res.status + ' ms=' + (Date.now() - t0));
    for (const l of lines.filter((x) => x.type !== 'log' && x.type !== 'progress')) console.log(JSON.stringify(l));
    console.log((errors.length ? errors : logs.slice(-4)).join('\n'));
    const result = lines.find((l) => l.type === 'result');
    process.exit(result && result.ok ? 0 : 1);
})();

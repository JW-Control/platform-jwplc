// Página de JWPLC_WebScreen (uso interno). Se sirve tal cual en "/".
//
// Protocolo con el equipo:
//   GET /f?v=<versión>&p=<colores conocidos>  -> cuadro binario (little endian):
//       "JWS1", u32 versión, u16 ancho, u16 alto, u16 desde, u16 n, n x u16 RGB565,
//       filas { u16 y, pares (u8 largo-1, u8 índice) hasta completar el ancho },
//       u16 0xFFFF al final.
//       &w=1  espera larga: el equipo responde en cuanto cambia algo (máx. 400 ms).
//       &e=<tecla><1|0>  tecla pulsada / suelta dentro de la misma petición.
//   GET /k?b=<0..5>&d=<1|0>                    -> tecla virtual (compatibilidad).
#pragma once

static const char JWPLC_WEBSCREEN_PAGE[] = R"JWHTML(<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>JWPLC Basic en vivo</title>
<style>
:root{--bg:#0d1117;--panel:#1b2230;--edge:#2c3648;--txt:#e6edf3;--dim:#8b98a9;--key:#2a3344;--keyon:#1f6feb;--ok:#2ea043;--bad:#da3633}
*{box-sizing:border-box}
html,body{margin:0;height:100%;background:var(--bg);color:var(--txt);font-family:system-ui,Segoe UI,Roboto,sans-serif}
main{min-height:100%;display:flex;flex-direction:column;align-items:center;justify-content:center;gap:18px;padding:16px}
header{display:flex;align-items:center;gap:12px;width:min(100%,1000px)}
header h1{font-size:20px;margin:0;font-weight:600;letter-spacing:.3px}
header .brand{color:var(--dim);font-weight:400}
#st{margin-left:auto;font-size:14px;color:var(--dim);display:flex;align-items:center;gap:8px}
#dot{width:10px;height:10px;border-radius:50%;background:var(--bad)}
#dot.on{background:var(--ok)}
.dev{background:linear-gradient(#232c3c,#161c27);border:1px solid var(--edge);border-radius:18px;padding:18px;width:min(100%,1000px);box-shadow:0 12px 40px #0008}
.scr{position:relative;background:#000;border-radius:6px;overflow:hidden;border:3px solid #05070a}
canvas{display:block;width:100%;height:auto;image-rendering:pixelated;image-rendering:crisp-edges}
#off{position:absolute;inset:0;display:flex;align-items:center;justify-content:center;background:#000c;font-size:22px}
#off[hidden]{display:none}
.keys{display:grid;grid-template-columns:repeat(3,1fr);gap:12px;margin-top:18px}
button{font:inherit;font-size:22px;font-weight:600;color:var(--txt);background:var(--key);border:1px solid var(--edge);border-radius:12px;padding:14px 0;cursor:pointer;touch-action:none;user-select:none;-webkit-user-select:none}
button.on{background:var(--keyon);border-color:var(--keyon)}
button:focus-visible{outline:3px solid var(--keyon);outline-offset:2px}
footer{font-size:13px;color:var(--dim);text-align:center}
kbd{border:1px solid var(--edge);border-radius:4px;padding:0 5px;background:var(--panel)}
body.solo .keys,body.solo footer{display:none}
.scr:fullscreen{display:flex;align-items:center;justify-content:center;border:0;border-radius:0}
.scr:fullscreen canvas{width:auto;height:100vh;max-width:100vw;object-fit:contain}
</style></head><body><main>
<header><h1>JWPLC Basic <span class="brand">· pantalla en vivo</span></h1><div id="st"><span id="dot"></span><span id="txt">conectando…</span></div></header>
<div class="dev">
<div class="scr"><canvas id="c" width="320" height="170"></canvas><div id="off">Sin conexión con el equipo…</div></div>
<div class="keys">
<button data-k="0" aria-label="Izquierda">◄</button><button data-k="1" aria-label="Arriba">▲</button><button data-k="2" aria-label="Derecha">►</button>
<button data-k="3">ESC</button><button data-k="4">OK</button><button data-k="5" aria-label="Abajo">▼</button>
</div></div>
<footer>Teclado: <kbd>←</kbd> <kbd>↑</kbd> <kbd>→</kbd> <kbd>↓</kbd> · <kbd>Enter</kbd> = OK · <kbd>Esc</kbd> = ESC · <kbd>F</kbd> o doble clic = pantalla completa · <kbd>T</kbd> = ocultar teclas</footer>
</main>
<script>
const c=document.getElementById('c'),g=c.getContext('2d'),off=document.getElementById('off'),dot=document.getElementById('dot'),txt=document.getElementById('txt');
let W=320,H=170,img=g.createImageData(W,H),idx=new Uint8Array(W*H),pal=[],ver=0,frames=0,last=performance.now(),fails=0;
function rgb(v){return[((v>>11)&31)*255/31|0,((v>>5)&63)*255/63|0,(v&31)*255/31|0]}
function resize(w,h){W=w;H=h;c.width=w;c.height=h;img=g.createImageData(w,h);idx=new Uint8Array(w*h);ver=0}
function paint(rows){const d=img.data;for(const y of rows){let o=y*W;for(let x=0;x<W;x++,o++){const p=pal[idx[o]]||[0,0,0],q=o*4;d[q]=p[0];d[q+1]=p[1];d[q+2]=p[2];d[q+3]=255}}g.putImageData(img,0,0)}
function apply(b){const v=new DataView(b);if(b.byteLength<16||v.getUint32(0)!==0x4A575331)throw 0;let o=4;const nv=v.getUint32(o,true);o+=4;const w=v.getUint16(o,true),h=v.getUint16(o+2,true);o+=4;
if(w!==W||h!==H){resize(w,h);return}
const from=v.getUint16(o,true),n=v.getUint16(o+2,true);o+=4;for(let i=0;i<n;i++){pal[from+i]=rgb(v.getUint16(o,true));o+=2}
const rows=[];for(;;){const y=v.getUint16(o,true);o+=2;if(y===0xFFFF)break;let x=0,r=y*W;while(x<W){const len=v.getUint8(o)+1,ix=v.getUint8(o+1);o+=2;idx.fill(ix,r+x,r+x+len);x+=len}rows.push(y)}
if(rows.length){paint(rows);frames++}ver=nv}
// Una petición a la vez. Sin teclas pendientes, espera larga (w=1): el equipo
// contesta en cuanto cambia la pantalla. Una tecla cancela esa espera y viaja
// en la petición siguiente (e=), sin hacer cola detrás de un cuadro.
const evq=[];let ctl=null,waiting=false,busy=false;
async function loop(){if(busy)return;busy=true;let aborted=false;
const ev=evq.shift();ctl=new AbortController();waiting=ev===undefined;
try{const r=await fetch('/f?v='+ver+'&p='+pal.length+(ev!==undefined?'&e='+ev:(ver?'&w=1':'')),{cache:'no-store',signal:ctl.signal});if(!r.ok)throw 0;apply(await r.arrayBuffer());fails=0;off.hidden=true;dot.className='on'}
catch(e){if(e&&e.name==='AbortError')aborted=true;else{if(ev!==undefined)evq.unshift(ev);if(++fails>2){off.hidden=false;dot.className='';txt.textContent='sin conexión';ver=0;pal=[]}}}
waiting=false;busy=false;
const now=performance.now();if(now-last>1000){if(!fails)txt.textContent='en vivo'+(frames?' · '+Math.round(frames*1000/(now-last))+' cuadros/s':'');frames=0;last=now}
setTimeout(loop,fails&&!aborted?800:0)}
function send(k,d){evq.push(k*10+(d?1:0));if(waiting&&ctl)ctl.abort()}
const down=new Set();
if(/solo/.test(location.search))document.body.classList.add('solo');
function full(){const s=document.querySelector('.scr');document.fullscreenElement?document.exitFullscreen():s.requestFullscreen&&s.requestFullscreen()}
c.addEventListener('dblclick',full);
function key(k,d){if(d?down.has(k):!down.has(k))return;d?down.add(k):down.delete(k);document.querySelector('[data-k="'+k+'"]').classList.toggle('on',d);send(k,d)}
for(const b of document.querySelectorAll('button[data-k]')){const k=+b.dataset.k;b.addEventListener('pointerdown',e=>{b.setPointerCapture(e.pointerId);key(k,true)});for(const ev of['pointerup','pointercancel','lostpointercapture'])b.addEventListener(ev,()=>key(k,false))}
const map={ArrowLeft:0,ArrowUp:1,ArrowRight:2,Escape:3,Backspace:3,Enter:4,' ':4,ArrowDown:5};
addEventListener('keydown',e=>{if(e.key==='f'||e.key==='F'){full();return}if(e.key==='t'||e.key==='T'){document.body.classList.toggle('solo');return}const k=map[e.key];if(k===undefined)return;e.preventDefault();if(!e.repeat)key(k,true)});
addEventListener('keyup',e=>{const k=map[e.key];if(k!==undefined)key(k,false)});
addEventListener('blur',()=>{for(const k of[...down])key(k,false)});
setInterval(()=>{for(const k of down)send(k,true)},1000);
loop();
</script></body></html>
)JWHTML";

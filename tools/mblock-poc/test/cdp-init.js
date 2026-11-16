// Expone el contexto interno de mBlock 5.4.3 como window.__jwA (vmCtx, blocklyCtx, extCtx...).
// 98243 es el id del módulo webpack en mBlock 5.4.3; cambia con otra versión.
// Uso: node cdp.cjs cdp-init.js
(()=>{ if(!window.__jwReq) window.webpackChunkmblock_next.push([[Symbol("jw")],{},r=>{window.__jwReq=r;}]);
 window.__jwA=window.__jwReq(98243).default; return 'ok'; })()

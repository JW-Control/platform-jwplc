// Gate 4: añade al workspace "escribir" con "unir" y con una división, para revisar tipos.
// Ejecutar después de cdp-program-phase1.js. Uso: node cdp.cjs cdp-program-types.js
(async()=>{
 const A=__jwA;const b=A.blocklyCtx;const ws=b.workspace;const B=b.blockly;
 const num=(n)=>`<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
 const txt=(s)=>`<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
 const J='jwplc_basic.';
 const xml=`<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="${J}jwplc_every_seconds" x="800" y="20"><value name="SECS">${num(0.5)}</value><next>
  <block type="${J}jwplc_screen_write"><field name="LINE">3</field><value name="TEXT">${txt('')}<block type="operator_join"><value name="STRING1">${txt('t = ')}</value><value name="STRING2">${txt('')}<block type="${J}jwplc_millis"></block></value></block></value><next>
  <block type="${J}jwplc_screen_write"><field name="LINE">4</field><value name="TEXT">${txt('')}<block type="operator_divide"><value name="NUM1">${num(10)}</value><value name="NUM2">${num(3)}</value></block></value>
 </block></next></block></next></block></xml>`;
 B.Xml.domToWorkspace(B.Xml.textToDom(xml),ws);
 await new Promise(r=>setTimeout(r,2500));
 const code=window.monaco.editor.getModels().map(x=>x.getValue()).find(v=>v.includes('JWPLC Basic'));
 return code.split('\n').filter(l=>/_jw_line\((3|4),/.test(l)).join('\n');
})()

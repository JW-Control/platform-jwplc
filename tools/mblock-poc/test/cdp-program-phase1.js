// Gate 4: programa de prueba con todos los bloques de la fase 1 (eventos, pantalla, botonera, sonido).
// Requiere el Device jwplc_basic añadido (cdp-add-device.js). Uso: node cdp.cjs cdp-program-phase1.js
(async()=>{
 const A=__jwA;const b=A.blocklyCtx;const ws=b.workspace;const B=b.blockly;
 ws.clear();
 ws.createVariable('contador','', 'jwVarCont');
 const num=(n)=>`<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
 const txt=(s)=>`<shadow type="text"><field name="TEXT">${s}</field></shadow>`;
 const v=`<block type="data_variable"><field name="VARIABLE" id="jwVarCont" variabletype="">contador</field></block>`;
 const J='jwplc_basic.';
 const xml=`<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="${J}jwplc_when_start" x="20" y="20"><next>
  <block type="${J}jwplc_set_volume"><value name="PCT">${num(30)}</value><next>
  <block type="${J}jwplc_screen_write"><field name="LINE">1</field><value name="TEXT">${txt('Hola JWPLC')}</value><next>
  <block type="${J}jwplc_play_note"><field name="NOTE">523</field><value name="SECS">${num(0.2)}</value>
  </block></next></block></next></block></next></block>
 <block type="${J}jwplc_every_seconds" x="20" y="250"><value name="SECS">${num(1)}</value><next>
  <block type="data_changevariableby"><field name="VARIABLE" id="jwVarCont" variabletype="">contador</field><value name="VALUE">${num(1)}</value><next>
  <block type="${J}jwplc_screen_write"><field name="LINE">2</field><value name="TEXT">${txt('')}${v}</value><next>
  <block type="${J}jwplc_toggle_output"><field name="Q">Q0_7</field></block></next></block></next></block></next></block>
 <block type="${J}jwplc_when_button" x="420" y="20"><field name="B">BTN_OK</field><next>
  <block type="${J}jwplc_beep"><next>
  <block type="${J}jwplc_screen_write"><field name="LINE">3</field><value name="TEXT">${txt('OK pulsado')}</value></block></next></block></next></block>
 <block type="${J}jwplc_when_button" x="420" y="200"><field name="B">BTN_ESC</field><next>
  <block type="${J}jwplc_screen_clear"><next><block type="${J}jwplc_stop_sound"></block></next></block></next></block>
 <block type="${J}jwplc_when_input_on" x="420" y="350"><field name="I">I0_0</field><next>
  <block type="${J}jwplc_play_tone"><value name="HZ">${num(880)}</value><value name="SECS">${num(0.1)}</value></block></next></block>
 <block type="${J}jwplc_when_start" x="20" y="500"><next>
  <block type="control_forever"><statement name="SUBSTACK">
   <block type="control_if_else">
    <value name="CONDITION"><block type="${J}jwplc_button_down"><field name="B">BTN_UP</field></block></value>
    <statement name="SUBSTACK"><block type="${J}jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">HIGH</field></block></statement>
    <statement name="SUBSTACK2"><block type="${J}jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">LOW</field></block></statement>
    <next><block type="${J}jwplc_screen_write"><field name="LINE">4</field><value name="TEXT">${txt('')}<block type="${J}jwplc_read_input"><field name="I">I0_1</field></block></value></block></next>
   </block></statement></block></next></block>
 </xml>`;
 B.Xml.domToWorkspace(B.Xml.textToDom(xml),ws);
 await new Promise(r=>setTimeout(r,2500));
 if(!document.querySelector('.monaco-editor')){const t=document.querySelector('button.tigger');if(t)t.click();await new Promise(r=>setTimeout(r,2500));}
 return 'BLOCKS='+ws.getAllBlocks().length+'\n'+window.monaco.editor.getModels().map(x=>x.getValue()).filter(v=>v.includes('JWPLC Basic')).join('\n=====\n');
})()

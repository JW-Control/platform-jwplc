// Carga en el workspace un programa de bloques JWPLC de demostración y devuelve el código Arduino generado.
// Requiere el Device jwplc_basic añadido y cdp-init.js ejecutado. Uso: node cdp.cjs cdp-program-demo.js
(async()=>{
 const A=__jwA;const b=A.blocklyCtx;const ws=b.workspace;const B=b.blockly;
 ws.clear();
 const xml=`<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="jwplc_basic.jwplc_when_start" x="40" y="40">
  <next><block type="jwplc_basic.jwplc_all_outputs_off">
  <next><block type="control_forever"><statement name="SUBSTACK">
   <block type="control_if_else">
     <value name="CONDITION"><block type="jwplc_basic.jwplc_read_input"><field name="I">I0_0</field></block></value>
     <statement name="SUBSTACK"><block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">HIGH</field></block></statement>
     <statement name="SUBSTACK2"><block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">LOW</field></block></statement>
     <next><block type="jwplc_basic.jwplc_toggle_output"><field name="Q">Q0_7</field>
      <next><block type="control_repeat"><value name="TIMES"><shadow type="math_whole_number"><field name="NUM">3</field></shadow></value><statement name="SUBSTACK"><block type="jwplc_basic.jwplc_serial_print"><value name="VALUE"><shadow type="math_number"><field name="NUM">7</field></shadow></value></block></statement><next><block type="control_wait"><value name="DURATION"><shadow type="math_positive_number"><field name="NUM">0.5</field></shadow></value></block></next></block></next>
     </block></next>
   </block>
  </statement></block></next></block></next>
 </block></xml>`;
 B.Xml.domToWorkspace(B.Xml.textToDom(xml),ws);
 await new Promise(r=>setTimeout(r,2500));
 if(!document.querySelector('.monaco-editor')){document.querySelector('button.tigger').click();await new Promise(r=>setTimeout(r,2500));}
 return window.monaco.editor.getModels().map(x=>x.getValue()).filter(v=>v.includes('JWPLC')).join('\n=====\n');
})()

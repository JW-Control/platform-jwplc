// Gate 3I: prueba aislada de Q0.0 (encender 1 s, apagar 1 s, imprime 1/0 por serie).
// Requiere el Device jwplc_basic añadido y cdp-init.js ejecutado. Uso: node cdp.cjs cdp-program-q00-isolated.js
(async()=>{
 const A=__jwA;const b=A.blocklyCtx;const ws=b.workspace;const B=b.blockly;
 ws.clear();
 const w=(s)=>`<block type="control_wait"><value name="DURATION"><shadow type="math_positive_number"><field name="NUM">${s}</field></shadow></value>`;
 const xml=`<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="jwplc_basic.jwplc_when_start" x="40" y="40">
  <next><block type="jwplc_basic.jwplc_all_outputs_off">
  <next><block type="control_forever"><statement name="SUBSTACK">
   <block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">HIGH</field>
    <next><block type="jwplc_basic.jwplc_serial_print"><value name="VALUE"><shadow type="math_number"><field name="NUM">1</field></shadow></value>
     <next>${w(1)}
      <next><block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">LOW</field>
       <next><block type="jwplc_basic.jwplc_serial_print"><value name="VALUE"><shadow type="math_number"><field name="NUM">0</field></shadow></value>
        <next>${w(1)}</block></next>
       </block></next>
      </block></next>
     </block></next>
    </block></next>
   </block>
  </statement></block></next></block></next>
 </block></xml>`;
 B.Xml.domToWorkspace(B.Xml.textToDom(xml),ws);
 await new Promise(r=>setTimeout(r,2500));
 if(!document.querySelector('.monaco-editor')){document.querySelector('button.tigger').click();await new Promise(r=>setTimeout(r,2500));}
 return window.monaco.editor.getModels().map(x=>x.getValue()).filter(v=>v.includes('JWPLC Basic')).join('\n=====\n');
})()

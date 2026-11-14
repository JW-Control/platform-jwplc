// Gate 3I: ejemplo del propietario (Arduino IDE) reproducido en bloques: Q0.0 sigue a I0.0 y
// Q0.7 conmuta una vez por segundo con millis() % 1000, sin delay(). Requiere jwplc_basic y cdp-init.js.
// Uso: node cdp.cjs cdp-program-millis-scan.js
(async()=>{
 const A=__jwA;const b=A.blocklyCtx;const ws=b.workspace;const B=b.blockly;
 ws.clear();
 const v=ws.createVariable('inside','', 'jwInsideVar');
 const num=(n)=>`<shadow type="math_number"><field name="NUM">${n}</field></shadow>`;
 const varBlk=`<block type="data_variable"><field name="VARIABLE" id="jwInsideVar" variabletype="">inside</field></block>`;
 const setVar=(n,next='')=>`<block type="data_setvariableto"><field name="VARIABLE" id="jwInsideVar" variabletype="">inside</field><value name="VALUE"><shadow type="text"><field name="TEXT">${n}</field></shadow></value>${next}</block>`;
 const mod=`<block type="operator_mod"><value name="NUM1">${num('')}<block type="jwplc_basic.jwplc_millis"></block></value><value name="NUM2">${num(1000)}</value></block>`;
 const lt500=`<block type="operator_lt"><value name="OPERAND1"><shadow type="text"><field name="TEXT"></field></shadow>${mod}</value><value name="OPERAND2"><shadow type="text"><field name="TEXT">500</field></shadow></value></block>`;
 const insideFalse=`<block type="operator_equals"><value name="OPERAND1"><shadow type="text"><field name="TEXT"></field></shadow>${varBlk}</value><value name="OPERAND2"><shadow type="text"><field name="TEXT">0</field></shadow></value></block>`;
 const xml=`<xml xmlns="http://www.w3.org/1999/xhtml">
 <block type="jwplc_basic.jwplc_when_start" x="40" y="40">
  <next><block type="jwplc_basic.jwplc_all_outputs_off">
  <next>${setVar(0,`<next><block type="control_forever"><statement name="SUBSTACK">
   <block type="control_if_else">
     <value name="CONDITION"><block type="jwplc_basic.jwplc_read_input"><field name="I">I0_0</field></block></value>
     <statement name="SUBSTACK"><block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">HIGH</field></block></statement>
     <statement name="SUBSTACK2"><block type="jwplc_basic.jwplc_write_output"><field name="Q">Q0_0</field><field name="STATE">LOW</field></block></statement>
     <next><block type="control_if">
       <value name="CONDITION"><block type="operator_and"><value name="OPERAND1">${lt500}</value><value name="OPERAND2">${insideFalse}</value></block></value>
       <statement name="SUBSTACK"><block type="jwplc_basic.jwplc_toggle_output"><field name="Q">Q0_7</field><next>${setVar(1)}</next></block></statement>
       <next><block type="control_if">
         <value name="CONDITION"><block type="operator_not"><value name="OPERAND">${lt500}</value></block></value>
         <statement name="SUBSTACK">${setVar(0)}</statement>
       </block></next>
     </block></next>
   </block>
  </statement></block></next>`)}</block></next>
 </block></xml>`;
 B.Xml.domToWorkspace(B.Xml.textToDom(xml),ws);
 await new Promise(r=>setTimeout(r,2500));
 if(!document.querySelector('.monaco-editor')){document.querySelector('button.tigger').click();await new Promise(r=>setTimeout(r,2500));}
 return 'BLOCKS='+ws.getAllBlocks().length+'\n'+window.monaco.editor.getModels().map(x=>x.getValue()).filter(v=>v.includes('JWPLC Basic')).join('\n=====\n');
})()

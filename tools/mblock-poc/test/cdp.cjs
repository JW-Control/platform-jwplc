// cliente CDP mínimo: node cdp.cjs <archivo.js | -e "expr">  (evalúa en la página mBlock, awaitPromise)
const fs=require('fs');
const port=process.env.CDP_PORT||9229;
(async()=>{
 const list=await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
 const page=list.find(p=>p.type==='page'&&/mblock/.test(p.url));
 const ws=new WebSocket(page.webSocketDebuggerUrl);
 await new Promise(r=>ws.onopen=r);
 const expr=process.argv[2]==='-e'?process.argv[3]:fs.readFileSync(process.argv[2],'utf8');
 ws.onmessage=(m)=>{const d=JSON.parse(m.data);if(d.id===1){
   if(d.result.exceptionDetails)console.log('EXCEPTION',JSON.stringify(d.result.exceptionDetails).slice(0,2000));
   else console.log(typeof d.result.result.value==='string'?d.result.result.value:JSON.stringify(d.result.result.value??d.result.result,null,1));
   ws.close();process.exit(0);} };
 ws.send(JSON.stringify({id:1,method:'Runtime.evaluate',params:{expression:expr,awaitPromise:true,returnByValue:true,timeout:Number(process.env.CDP_TIMEOUT||600000)}}));
})();

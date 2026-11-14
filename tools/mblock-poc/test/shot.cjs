// captura de la página mBlock: node shot.cjs salida.png
const fs=require('fs');
(async()=>{
 const list=await (await fetch('http://127.0.0.1:9229/json/list')).json();
 const page=list.find(p=>p.type==='page'&&/mblock/.test(p.url));
 const ws=new WebSocket(page.webSocketDebuggerUrl);await new Promise(r=>ws.onopen=r);
 ws.onmessage=(m)=>{const d=JSON.parse(m.data);if(d.id===1){fs.writeFileSync(process.argv[2],Buffer.from(d.result.data,'base64'));console.log('saved',process.argv[2]);process.exit(0);}};
 ws.send(JSON.stringify({id:1,method:'Page.captureScreenshot',params:{format:'png'}}));
})();

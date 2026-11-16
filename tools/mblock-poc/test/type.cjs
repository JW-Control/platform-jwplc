// enfoca el input con placeholder dado y escribe texto con Input.insertText
const [,,placeholder,text]=process.argv;
(async()=>{
 const list=await (await fetch('http://127.0.0.1:9229/json/list')).json();
 const page=list.find(p=>p.type==='page'&&/mblock/.test(p.url));
 const ws=new WebSocket(page.webSocketDebuggerUrl);await new Promise(r=>ws.onopen=r);
 let id=0;const pend={};ws.onmessage=m=>{const d=JSON.parse(m.data);if(pend[d.id]){pend[d.id](d);delete pend[d.id];}};
 const call=(method,params)=>new Promise(r=>{const i=++id;pend[i]=r;ws.send(JSON.stringify({id:i,method,params}));});
 const r=await call('Runtime.evaluate',{expression:`(()=>{const e=[...document.querySelectorAll('input')].find(i=>i.placeholder===${JSON.stringify(placeholder)}&&i.getBoundingClientRect().width>0);if(!e)return false;e.focus();e.select();return true})()`,returnByValue:true});
 console.log('focused',r.result.result.value);
 await call('Input.insertText',{text});
 await call('Input.dispatchKeyEvent',{type:'keyDown',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
 await call('Input.dispatchKeyEvent',{type:'keyUp',key:'Enter',code:'Enter',windowsVirtualKeyCode:13});
 process.exit(0);
})();

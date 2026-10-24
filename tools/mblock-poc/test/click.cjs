// clic real (Input.dispatchMouseEvent) sobre el elemento visible más interno cuyo texto == arg
const [,,text,nth='0']=process.argv;
(async()=>{
 const list=await (await fetch('http://127.0.0.1:9229/json/list')).json();
 const page=list.find(p=>p.type==='page'&&/mblock/.test(p.url));
 const ws=new WebSocket(page.webSocketDebuggerUrl);await new Promise(r=>ws.onopen=r);
 let id=0;const pend={};ws.onmessage=m=>{const d=JSON.parse(m.data);if(pend[d.id]){pend[d.id](d);delete pend[d.id];}};
 const call=(method,params)=>new Promise(r=>{const i=++id;pend[i]=r;ws.send(JSON.stringify({id:i,method,params}));});
 const expr=`(()=>{const t=${JSON.stringify(text)};const els=[...document.querySelectorAll('body *')].filter(e=>{if(e.children.length&&[...e.children].some(c=>(c.innerText||'').trim()===t))return false;return (e.innerText||e.textContent||'').trim()===t&&e.getBoundingClientRect().width>0;});const e=els[${Number(nth)}];if(!e)return null;const r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,n:els.length,tag:e.tagName,cls:e.className}})()`;
 const r=await call('Runtime.evaluate',{expression:expr,returnByValue:true});
 const v=r.result.result.value;console.log(JSON.stringify(v));
 if(!v){process.exit(2);}
 for(const type of ['mouseMoved','mousePressed','mouseReleased'])await call('Input.dispatchMouseEvent',{type,x:v.x,y:v.y,button:'left',clickCount:1});
 process.exit(0);
})();

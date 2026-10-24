// Compila desde el renderer de mBlock (misma ruta que el driver: token vía MeApi + fetch al bridge).
// Uso: node cdp.cjs cdp-compile-in-mblock.js
(async()=>{
 const code=window.monaco.editor.getModels().map(x=>x.getValue()).find(v=>v.includes('JWPLC Basic'));
 const home=await MeApi.sys.getPath('home');
 const tf=MeApi.fe.getPath().join(home,'.jwplc','mblock-bridge','token.txt');
 const token=String(await MeApi.fe.read2Str(tf)).trim();
 const t0=Date.now();
 const res=await fetch('http://127.0.0.1:47820/compile',{method:'POST',headers:{'Content-Type':'application/json','X-JWPLC-Token':token},body:JSON.stringify({code})});
 const txt=await res.text();
 const lines=txt.trim().split('\n').map(l=>JSON.parse(l));
 return 'HTTP='+res.status+' ms='+(Date.now()-t0)+'\n'+lines.filter(l=>l.type!=='log').map(l=>JSON.stringify(l)).join('\n')+'\nLAST_LOGS:\n'+lines.filter(l=>l.type==='log').slice(-4).map(l=>l.line).join('\n');
})()

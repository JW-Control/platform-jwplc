// lector mínimo de asar: node asar.cjs <archivo.asar> list | extract <outdir> [filtroRegex]
const fs=require('fs'),path=require('path');
const [,,file,cmd,out,filt]=process.argv;
const fd=fs.openSync(file,'r');const b=Buffer.alloc(16);fs.readSync(fd,b,0,16,0);
const hsize=b.readUInt32LE(4);const jlen=b.readUInt32LE(12);
const hb=Buffer.alloc(jlen);fs.readSync(fd,hb,0,jlen,16);const header=JSON.parse(hb.toString());
const base=8+hsize;const re=filt?new RegExp(filt):null;
function walk(node,p){for(const [k,v] of Object.entries(node.files||{})){const q=p?p+'/'+k:k;
 if(v.files)walk(v,q);else{ if(re&&!re.test(q))continue;
  if(cmd==='list')console.log((v.unpacked?'U ':'  ')+String(v.size).padStart(10)+' '+q);
  else if(!v.unpacked){const o=path.join(out,q);fs.mkdirSync(path.dirname(o),{recursive:true});const buf=Buffer.alloc(v.size);fs.readSync(fd,buf,0,v.size,base+Number(v.offset));fs.writeFileSync(o,buf);}}}}
walk(header,'');

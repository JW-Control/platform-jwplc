#!/usr/bin/env python3
"""Alpha13 G10 — integrador por hitos; fase actual: preparación unificada.

Compone TODOS los overlays ya probados sobre la MISMA fuente y reporta
brechas. No repite gates individuales ni confunde composición con build
Arduino o adopción. Solo escrituras dentro de tools/alpha13/results/.
El avance a compilación integrada requiere resolver bloqueos y autorizar
separadamente cualquier cambio de firmware y cualquier gate físico.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
PREVIOUS_HEAD = "c25a1201e7ead17475b06c3dd0f7fa3554c94757"
PREVIOUS_RUN = "g10_p1g_c2_s2c2b1_20261013_135948_9b7cb66e"
PREVIOUS_STATUS = "UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED"
MODULES = {
    "repro": "a13_g10_p1a_a13_006_repro.py",
    "s1": "a13_g10_p1g_c2_first_real_spi_slice.py",
    "s2a": "a13_g10_p1g_c2_s2a_socket_begin_lease.py",
    "b1": "a13_g10_p1g_c2_s2b1_client_server_lease.py",
    "b2a": "a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
    "b2b": "a13_g10_p1g_c2_s2b2b_outbound_connect.py",
    "c1": "a13_g10_p1g_c2_s2c1_tx_cancel.py",
    "c2a": "a13_g10_p1g_c2_s2c2a_legacy_connect.py",
    "udp": "a13_g10_p1g_c2_s2c2b1_udp_lifecycle.py",
    "frozen": "a13_g10_p1g_a_mutex_callpaths.py",
}
# Ningún PASS previo resuelve todavía estas fronteras de integración.
BLOCKERS = {
    "B01": "SPI_BUSY: EthernetClient.connected() y socketStatus() pueden simular CLOSED o desconexión",
    "B02": "TCP write/pollWrite/read/available, UDP parse/read/write y rutas directas restantes no tienen lease completo",
    "B03": "Dns.cpp real, EthernetUDP dependiente y DHCP no están integrados en un mismo flujo",
    "B04": "stop() / connect() con timeout y SPI ocupado pueden dejar handles sin reintento automático garantizado",
    "B05": "Identidad real de server_port[]/listeners, socket recycling y reset físico no validada a nivel end-to-end",
    "B06": "Sin build Arduino CLI/IDE baseline-vs-overlay con autoload y cinco archives congelados",
    "B07": "Sin medición física de fairness y regresión W5500, Display, SD, FRAM, RTC, TCA, RS-485, Modbus RTU/TCP",
    "B08": "Overlay único no equivale a compilación completa: revisar doble lock, firmas públicas y compatibilidad del core",
}
def emit(k,v):
    print("G10_INTEGRATED_"+str(k)+"="+str(v),flush=True)
def require(ok,why):
    if not ok:raise RuntimeError(why)
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True,text=True,errors="replace",timeout=35)
    require(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def digest(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()
def load(rel,name):
    p=ROOT/"tools/alpha13/gates"/rel
    spec=importlib.util.spec_from_file_location("g10_integrated_"+name,p)
    require(spec is not None and spec.loader is not None,"IMPORT_ERROR_"+name)
    m=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=m
    spec.loader.exec_module(m)
    return m
def preflight(ci):
    require(Path.cwd().resolve()==ROOT,"WRONG_CWD")
    require(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
            "WRONG_REPO")
    head=git("rev-parse","HEAD")
    require(subprocess.run(["git","merge-base","--is-ancestor",PREVIOUS_HEAD,head],
                           cwd=ROOT,capture_output=True).returncode==0,
            "S2C2B1_HEAD_NOT_ANCESTOR")
    require(not git("diff","--name-only") and
            not git("diff","--cached","--name-only"),
            "TRACKED_WORKTREE_DIRTY_STOP_REVIEW_NO_CLEAN")
    if not ci:
        require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        require(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
                "REMOTE_HEAD_MISMATCH")
        path=ROOT/"tools/alpha13/results"/PREVIOUS_RUN/"MANIFEST.json"
        require(path.exists(),"LOCAL_S2C2B1_MANIFEST_MISSING")
        m=json.loads(path.read_text(encoding="utf-8-sig"))
        require(m.get("run_id")==PREVIOUS_RUN and
                m.get("head")==PREVIOUS_HEAD and
                m.get("status")==PREVIOUS_STATUS,
                "LOCAL_S2C2B1_NOT_QUALIFIED")
    f=load(MODULES["frozen"],"frozen")
    require(f.check_frozen()==5,"PRECOMPILED_HASH_DRIFT")
    f.check(f.sources())
    return head

def compose():
    mods={k:load(v,k) for k,v in MODULES.items() if k!="frozen"}
    repro=mods["repro"]
    s1,s2a,b1=mods["s1"],mods["s2a"],mods["b1"]
    b2a,b2b,c1,c2a,udp=(mods[k] for k in ("b2a","b2b","c1","c2a","udp"))
    # Cada capa se aplica a la salida de la anterior: evita overlays
    # independientes incompatibles que antes daban falsos PASS aislados.
    raw={k:(ROOT/p).read_text(encoding="utf-8-sig")
         for k,p in b1.FILES.items()}
    first=b1.patch(repro,raw)
    s1_inputs={k:(first["CLIENT"] if k=="RAW_CLIENT" else
                  (ROOT/p).read_text(encoding="utf-8-sig"))
               for k,p in s1.TARGETS.items()}
    s1_overlay=s1.patch_functions(repro,s1_inputs)
    first["CLIENT"]=s1_overlay["RAW_CLIENT"]
    first["SOCKET"]=s1_overlay["SOCKET"]
    for k,p in s1.TARGETS.items():
        if k not in ("RAW_CLIENT","SOCKET"):
            first[k]=s1_overlay[k]
    first["CLIENT"],_=b2a.patched_methods(repro,first["CLIENT"])
    field="jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};"
    require(first["HEADER"].count(field)==1,"CLIENT_TOKEN_FIELD_NOT_UNIQUE")
    first["HEADER"]=first["HEADER"].replace(field,field+"""
    bool _jwplcDeferredConnect = false;
    IPAddress _jwplcDeferredIp;
    uint16_t _jwplcDeferredPort = 0;""",1)
    first["CLIENT"],_=b2b.patch(repro,first["CLIENT"])
    first["CLIENT"],_=c1.patch_tx(repro,first["CLIENT"])
    first["CLIENT"],_=c2a.patch_legacy(repro,first["CLIENT"])
    socket_overlay,_=s2a.patch(repro,first["SOCKET"])
    first["SOCKET"]=socket_overlay
    udp_original=(ROOT/udp.UDP_PATH).read_text(encoding="utf-8-sig")
    udp_overlay,_=udp.patches(repro,udp_original)
    first["UDP"]='#include "JWPLC_W5500_SafeAccess.h"\n'+udp_overlay
    udp_anchor="bool _sendPending = false; // JWPLC cooperative UDP SEND state"
    require(first["HEADER"].count(udp_anchor)==1,"UDP_HEADER_FIELD_DRIFT")
    first["HEADER"]=first["HEADER"].replace(udp_anchor,udp_anchor+
          "\n\tbool _jwplcDeferredSendStart = false;"
          "\n\tjwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",1)
    # Includes duplicados son producto de combinar transformadores.
    # El overlay integrado debe contener solo uno; no modificar lógica.
    inc='#include "JWPLC_W5500_SafeAccess.h"\n'
    for k,v in first.items():
        if inc in v:
            require(v.count(inc)<=3,"UNEXPECTED_INCLUDE_MULTIPLICITY_"+k)
            first[k]=inc+v.replace(inc,"")
    paths={}
    for k,path in b1.FILES.items():
        require(k in first,"MISSING_LAYER_"+k)
        paths[path]=first[k]
    for k,path in s1.TARGETS.items():
        paths[path]=first["CLIENT"] if k=="RAW_CLIENT" else first[k]
    paths[udp.UDP_PATH]=first["UDP"]
    return paths

def write_evidence(out,head,files):
    overlay=out/"overlay"
    hashes={}
    for name,source in sorted(files.items()):
        p=overlay/name
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(source,encoding="utf-8")
        hashes[name]={"sha256":digest(p),"bytes":p.stat().st_size,
                      "source_sha256":digest(ROOT/name)}
    out.joinpath("BLOCKERS.md").write_text(
        "# G10-I — bloqueos de adopción\n\n"+
        "\n".join(f"- **{k}**: {v}" for k,v in BLOCKERS.items())+"\n",
        encoding="utf-8")
    return hashes
def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ci-standalone",action="store_true",
                        help="CI: solo verificación de branch ancestor, no acredita gate local")
    parser.add_argument("--require-ready",action="store_true",
                        help="Falla mientras existan bloqueos de integración")
    opts=parser.parse_args()
    runid=("g10_i_"+datetime.now().strftime("%Y%m%d_%H%M%S")
           +"_"+uuid.uuid4().hex[:8])
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-INTEGRATED-PREP-1","run_id":runid,
       "phase":"G10-I-PREP","status":"REVIEW_STOP",
       "product_modified":False,"hardware_touched":False,
       "arduino_build":False,"archives_frozen":False,
       "candidate_adopted":False}
    exit_code=2
    try:
        head=preflight(opts.ci_standalone)
        m["archives_frozen"]=True
        files=compose()
        shas=write_evidence(out,head,files)
        m.update({"head":head,"overlay_files":shas,
                  "blockers":BLOCKERS,
                  "status":"COMPOSED_REVIEW_BLOCKED"})
        emit("SOURCE_OVERLAY","COMPOSED")
        emit("OVERLAY_FILES",len(files))
        emit("FROZEN_ARCHIVES",5)
        emit("BLOCKERS",len(BLOCKERS))
        emit("ARDUINO_BUILD","NOT_RUN")
        emit("PRODUCT_CHANGED","NO")
        emit("STATUS",m["status"])
        exit_code=3 if opts.require_ready else 0
    except Exception as exc:
        m["reason"]=repr(exc)
        emit("STATUS","REVIEW_STOP")
        emit("REASON",repr(exc))
    finally:
        m["exit_code"]=exit_code
        (out/"MANIFEST.json").write_text(json.dumps(m,indent=2,ensure_ascii=False)+"\n",
                                         encoding="utf-8")
        (out/"SUMMARY.log").write_text(
          "PHASE=G10-I-PREP\nSTATUS="+m["status"]+
          "\nCLASSIFICATION="+("PRECONDITION" if m["status"]=="REVIEW_STOP" else "DESIGN")+
          "\nARDUINO_BUILD=NOT_RUN\nPRODUCT_CHANGED=NO\n",
          encoding="utf-8")
        emit("RESULTS",out)
    return exit_code

if __name__=="__main__":
    sys.exit(main())

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
import shutil
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

def patch_client_modbus_b01_b04(repro, first):
    """Parcheo source-backed de consulta triestado y cierre reintentable.

    No altera la API Arduino legacy; los nuevos símbolos son adiciones
    internas al overlay. Cada guard de deriva debe fallar cerrado.
    """
    hdr=first["HEADER"]
    signature="uint8_t getSocketNumber() const { return _sockindex; }"
    require(hdr.count(signature)==1,"G10_CLIENT_PROBE_HEADER_DRIFT")
    hdr=hdr.replace(signature,signature+
        "\n\t// Estado tri-valuado interno; no convertir BUS_BUSY a CLOSED.\n"
        "\tjwplc_w5500_candidate::StatusReadOutcome jwplcProbeSocketStatus(uint8_t& result);",1)
    first["HEADER"]=hdr
    cpp=first["CLIENT"]
    require("EthernetClient::jwplcProbeSocketStatus" not in cpp,
            "CLIENT_PROBE_ALREADY_PRESENT")
    cpp+="""

// G10-I B01: Status bajo ownership y la misma exclusión W5500.
jwplc_w5500_candidate::StatusReadOutcome
EthernetClient::jwplcProbeSocketStatus(uint8_t& result)
{
    if (_sockindex >= MAX_SOCK_NUM)
        return jwplc_w5500_candidate::StatusReadOutcome::Stale;
    return jwplc_w5500_candidate::readStatusIfCurrent(
        _jwplcSocketToken,
        [](uint8_t index, void*) -> uint8_t {
            return Ethernet.socketStatus(index);
        },
        nullptr, 20, &result);
}
"""
    first["CLIENT"]=cpp
    mh_path="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h"
    mh=(ROOT/mh_path).read_text(encoding="utf-8-sig")
    field="    bool _sendStarted;"
    require(mh.count(field)==1,"MODBUS_HEADER_SEND_FIELD_DRIFT")
    mh=mh.replace(field,field+
        "\n    // G10-I: retener el objeto/socket hasta CLOSE o STALE real.\n"
        "    bool _jwplcClosePending = false;",1)
    proto="    void closeSession();"
    require(mh.count(proto)==1,"MODBUS_HEADER_CLOSE_DECL_DRIFT")
    mh=mh.replace(proto,proto+
        "\n    bool serviceDeferredClose();",1)
    first["MB_CLIENT_HEADER"]=mh
    mb=first["MB_CLIENT"]
    def replace_body(signature,old_fragment,new_fragment):
        nonlocal mb
        old=repro.extract(mb,signature)
        require(old.count(old_fragment)==1,
                "MODBUS_BODY_SOURCE_DRIFT_"+signature)
        new=old.replace(old_fragment,new_fragment,1)
        require(new!=old and mb.count(old)==1,
                "MODBUS_BODY_NOT_UNIQUE_"+signature)
        mb=mb.replace(old,new,1)
    replace_body("bool JWPLC_ModbusTCPClientClass::busy() const",
        "{",
        "{\n    // Mantener task() activo si el cierre necesita reintento.\n"
        "    if (_jwplcClosePending) return true;")
    replace_body("bool JWPLC_ModbusTCPClientClass::begin(",
        "    if (busy())",
        "    if (_jwplcClosePending) {\n"
        "        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;\n"
        "        return false;\n"
        "    }\n    if (busy())")
    replace_body("bool JWPLC_ModbusTCPClientClass::startRequest(",
        "    if (busy())",
        "    if (_jwplcClosePending) {\n"
        "        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;\n"
        "        return false;\n"
        "    }\n    if (busy())")
    replace_body("void JWPLC_ModbusTCPClientClass::task()",
        "    if (!busy())",
        "    // Incluso despues de fail()/end() se debe poder drenar CLOSE.\n"
        "    if (_jwplcClosePending && !serviceDeferredClose()) return;\n"
        "    if (!busy())")
    mb_sig="void JWPLC_ModbusTCPClientClass::closeSession()"
    before=repro.extract(mb,mb_sig)
    require(before.count("_tcpClient = EthernetClient();")==1 and
            "cancelConnectAsync();" in before,"CLOSESESSION_SOURCE_DRIFT")
    after=mb_sig+"""
{
    _asyncTx.reset();
    // No abandonar el token si otro task posee SPI.
    _jwplcClosePending = _jwplcClosePending || (bool)_tcpClient;
    if (_jwplcClosePending)
        (void)serviceDeferredClose();
    else
        _tcpClient = EthernetClient();

    _sessionConnected = false;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();
}
"""
    require(mb.count(before)==1,"CLOSESESSION_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    mb+="""

// Servicio cooperativo, sin espera ocupada ni cierre por indice sin owner.
bool JWPLC_ModbusTCPClientClass::serviceDeferredClose()
{
    if (!_jwplcClosePending) return true;
    if (!acquireBus(20)) return false; // retain _tcpClient and token
    _tcpClient.cancelStopAsync(); // guard anidado y lease en EthernetClient
    const bool released = !(bool)_tcpClient;
    releaseBus();
    if (released) {
        _tcpClient = EthernetClient();
        _jwplcClosePending = false;
    }
    return released;
}
"""
    # Lectura previa al uso: los consumidores internos nunca deben
    # interpretar BusBusy como desconexion. La validacion se hace dentro
    # de su scope de Modbus para evitar carreras entre probe y consumo.
    status_sig="void JWPLC_ModbusTCPClientClass::serviceConnection()"
    before=repro.extract(mb,status_sig)
    probe="        const bool stillConnected = _tcpClient.connected() != 0;"
    require(before.count(probe)==1,"MODBUS_CONNECT_STATUS_DRIFT")
    after=before.replace(probe,"""        uint8_t observedStatus = 0;
        const auto probe = _tcpClient.jwplcProbeSocketStatus(observedStatus);
        if (probe == jwplc_w5500_candidate::StatusReadOutcome::BusBusy) {
            releaseBus();
            return; // no falsa desconexion con SPI ocupado
        }
        const bool stillConnected =
            probe == jwplc_w5500_candidate::StatusReadOutcome::Read &&
            _tcpClient.connected() != 0;""",1)
    require(mb.count(before)==1,"MODBUS_SERVICE_CONNECT_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    resp_sig="void JWPLC_ModbusTCPClientClass::serviceResponse()"
    before=repro.extract(mb,resp_sig)
    needle="    int availableBytes = _tcpClient.available();"
    require(before.count(needle)==1,"MODBUS_RESPONSE_AVAILABLE_DRIFT")
    after=before.replace(needle,"""    uint8_t observedStatus = 0;
    const auto probe = _tcpClient.jwplcProbeSocketStatus(observedStatus);
    if (probe == jwplc_w5500_candidate::StatusReadOutcome::BusBusy) {
        releaseBus();
        return; // SPI_BUSY != link failure
    }
    if (probe == jwplc_w5500_candidate::StatusReadOutcome::Stale) {
        releaseBus();
        fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
        return;
    }
    int availableBytes = _tcpClient.available();""",1)
    require(mb.count(before)==1,"MODBUS_SERVICE_RESPONSE_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    first["MB_CLIENT"]=mb
    return first


def patch_modbus_server_deferred_drop(repro, first):
    """B01/B04 server: mantener el cliente hasta close validado, sin GPIO4."""
    hdr_path="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
    h=(ROOT/hdr_path).read_text(encoding="utf-8-sig")
    field="    bool _clientActive;"
    require(h.count(field)==1,"SERVER_DROP_HEADER_STATE_DRIFT")
    h=h.replace(field,field+"\n    bool _jwplcDropPending = false; // G10-I overlay only",1)
    decl="    void dropClient();"
    require(h.count(decl)==1,"SERVER_DROP_DECL_DRIFT")
    h=h.replace(decl,decl+"\n    bool serviceDeferredDrop();",1)
    first["MB_SERVER_HEADER"]=h
    cpp=first["MB_SERVER"]
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::dropClient()")
    require("if (_clientActive && acquireBus(20))" in old
            and "_client = EthernetClient();" in old,
            "SERVER_DROP_SOURCE_CHANGED")
    new="""void JWPLC_ModbusTCPClass::dropClient()
{
    // No perder la identidad del socket mientras falte mutex.
    _jwplcDropPending = _jwplcDropPending || (bool)_client;
    _clientActive = false;
    resetRx();
    if (_jwplcDropPending && !serviceDeferredDrop()) {
        return; // must be serviced before accepting any new socket
    }
    if (_serverEnabled && JWPLC_Ethernet.isReady())
        _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
    else if (_serverEnabled)
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
    else
        _serverState = JWPLC_MODBUS_TCP_SERVER_STOPPED;
}"""
    require(cpp.count(old)==1,"SERVER_DROP_METHOD_NOT_UNIQUE")
    cpp=cpp.replace(old,new,1)
    cpp+="""

// G10-I server cooperative cleanup. Never close an index without lease.
bool JWPLC_ModbusTCPClass::serviceDeferredDrop()
{
    if (!_jwplcDropPending) return true;
    if (!acquireBus(20)) return false;
    // The configured INT socket can no longer belong to this client.
    // Only touch W5500 INT state if the token is CURRENT.
    uint8_t observedStatus = 0;
    const auto outcome = _client.jwplcProbeSocketStatus(observedStatus);
    if (outcome == jwplc_w5500_candidate::StatusReadOutcome::Read &&
        _rxIntConfigured && _rxIntSocket == _client.getSocketNumber())
        disableRxIntLocked();
    _client.cancelStopAsync();  // nested scope checks token before close
    const bool released = !(bool)_client;
    releaseBus();
    if (released) {
        _client = EthernetClient();
        _jwplcDropPending = false;
        resetRx();
        resetRxIntSoftware();
    }
    return released;
}
"""
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::task()")
    anchor="    if (!_serverEnabled)"
    require(old.count(anchor)==1,"SERVER_TASK_SOURCE_CHANGED")
    revised=old.replace(anchor,
       """    // El cierre pendiente se atiende incluso si Ethernet deja de READY.
    if (_jwplcDropPending && !serviceDeferredDrop()) return;
"""+anchor,1)
    reset="""        _serverListening = false;
        _clientActive = false;
        _client = EthernetClient();"""
    require(revised.count(reset)==1,"SERVER_NOT_READY_RESET_DRIFT")
    revised=revised.replace(reset,"""        if (_clientActive || (bool)_client) {
            dropClient();
            if (_jwplcDropPending) return;
        }
        _serverListening = false;
        _clientActive = false;
        _client = EthernetClient();""",1)
    require(cpp.count(old)==1,"SERVER_TASK_NOT_UNIQUE")
    cpp=cpp.replace(old,revised,1)
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::serviceServer()")
    anchor="""    const uint32_t now = millis();"""
    require(old.count(anchor)==1,"SERVER_SERVICE_NOW_DRIFT")
    revised=old.replace(anchor,anchor+"""
    if (_jwplcDropPending && !serviceDeferredDrop())
        return;
""",1)
    # If status was actually CLOSED, use the guarded deferred close path
    # rather than throwing the client handle away without invalidate.
    old_drop="""            _client = EthernetClient();
            _clientActive = false;
            resetRx();
            _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;"""
    require(revised.count(old_drop)==1,"SERVER_PEER_DISCONNECT_DRIFT")
    revised=revised.replace(old_drop,"""            dropClient(); // owns valid token or defers safely
            if (_jwplcDropPending) return;""",1)
    require(cpp.count(old)==1,"SERVER_SERVICE_NOT_UNIQUE")
    cpp=cpp.replace(old,revised,1)
    first["MB_SERVER"]=cpp
    return first

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
    first=patch_client_modbus_b01_b04(repro,first)
    first=patch_modbus_server_deferred_drop(repro,first)
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
    paths["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h"]=first["MB_CLIENT_HEADER"]
    paths["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"]=first["MB_SERVER_HEADER"]
    # La fase antigua componía referencias a SafeAccess, pero OMITÍA sus
    # dos archivos. Un build Arduino completo exige ambas unidades.
    base=ROOT/"tools/alpha13/candidates/g10_p1g_c"
    candidate_base="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
    for name in ("JWPLC_W5500_SafeAccess.h","JWPLC_W5500_SafeAccess.cpp"):
        rel=candidate_base+name
        require(rel not in paths and not (ROOT/rel).exists(),
                "SAFE_ACCESS_ALREADY_IN_PRODUCT_REVIEW_REQUIRED_"+name)
        paths[rel]=(base/name).read_text(encoding="utf-8-sig")
    return paths

def write_evidence(out,head,files):
    overlay=out/"overlay"
    hashes={}
    for name,source in sorted(files.items()):
        p=overlay/name
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(source,encoding="utf-8")
        candidate=(ROOT/name)
        if not candidate.is_file():
            candidate=(ROOT/"tools/alpha13/candidates/g10_p1g_c"/Path(name).name)
        require(candidate.is_file(),"OVERLAY_SOURCE_PROVENANCE_MISSING_"+name)
        hashes[name]={"sha256":digest(p),"bytes":p.stat().st_size,
                      "source_sha256":digest(candidate),
                      "source_path":candidate.relative_to(ROOT).as_posix()}
    out.joinpath("BLOCKERS.md").write_text(
        "# G10-I — bloqueos de adopción\n\n"+
        "\n".join(f"- **{k}**: {v}" for k,v in BLOCKERS.items())+"\n",
        encoding="utf-8")
    return hashes

def stage_platform(destination,files,hashes):
    """Copia el package en ubicación temporal externa, sin modificar repo."""
    target=Path(destination).resolve()
    require(not target.is_relative_to(ROOT),"STAGE_CANNOT_BE_INSIDE_REPOSITORY")
    require(not target.exists(),"STAGE_MUST_NOT_ALREADY_EXIST")
    require(target.parent.is_dir(),"STAGE_PARENT_MISSING")
    require(str(target)!=str(target.anchor),"INVALID_STAGE_ROOT")
    source=ROOT/"JWPLC/2.1.0"
    shutil.copytree(source,target)
    manifest={}
    for rel,content in files.items():
        prefix="JWPLC/2.1.0/"
        require(rel.startswith(prefix),"OVERLAY_ESCAPES_PACKAGE_"+rel)
        dst=target/rel[len(prefix):]
        require(dst.resolve().is_relative_to(target),"OVERLAY_ESCAPE_DETECTED")
        dst.parent.mkdir(parents=True,exist_ok=True)
        dst.write_text(content,encoding="utf-8")
        require(digest(dst)==hashes[rel]["sha256"],
                "STAGED_SOURCE_SHA_DIFFERS_"+rel)
        manifest[rel]={"sha256":digest(dst),"relative_staged_path":rel[len(prefix):]}
    # Las bibliotecas .a se copian byte-a-byte. Los cinco archives son
    # inmutables, no se regeneran ni se vinculan de nuevo en esta etapa.
    frozen=load(MODULES["frozen"],"staged_frozen")
    refs=frozen.FROZEN if hasattr(frozen,"FROZEN") else {}
    # La comprobación del freeze canónico ya se ejecutó en preflight.
    return manifest

def server_cleanup_native(out,files):
    """Source-backed Server drop/retry tested inside the single G10-I run."""
    repro=load(MODULES["repro"],"server_cleanup_extract")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","server_cleanup_compiler")
    builder=load(MODULES["b2a"],"server_cleanup_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_server_cleanup_test.cpp"
    rel="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
    src=files[rel]
    sigs=("void JWPLC_ModbusTCPClass::dropClient()",
          "bool JWPLC_ModbusTCPClass::serviceDeferredDrop()",
          "void JWPLC_ModbusTCPClass::task()")
    bodies={sig:repro.extract(src,sig) for sig in sigs}
    def run(tag, methods):
        return builder.native(out,tag,"\n\n".join(methods[sig] for sig in sigs)+"\n",compiler)
    code,output=run("server_cleanup_positive",bodies)
    wanted=("G10_I_SERVER_DEFERRED_CLOSE_BUSY_RETRY=PASS",
            "G10_I_SERVER_STALE_OWNER_NOT_CLOSED=PASS",
            "G10_I_SERVER_EMPTY_AND_LINK_DOWN=PASS",
            "G10_I_SERVER_TASK_PENDING_DRAIN=PASS",
            "G10_I_SERVER_LINK_DOWN_BUSY_DRAIN=PASS",
            "G10_I_SERVER_DISABLED_DRAIN=PASS",
            "G10_I_SERVER_CLEANUP_NATIVE=PASS_THREE_REAL_SOURCE_METHODS")
    if code!=0 or not all(x in output for x in wanted):
        emit("SERVER_CLEANUP_DIAG",
             "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:1200])
    require(code==0 and all(x in output for x in wanted),
            "SERVER_CLEANUP_REAL_SOURCE_POSITIVE_FAILED")
    negatives=[
        ("LOSE_OWNER_WHEN_BUSY",sigs[0],
         "_jwplcDropPending = _jwplcDropPending || (bool)_client;",
         "_jwplcDropPending = false;",
         "SERVER_BUSY_DISCARDED_LEASE"),
        ("SKIP_CLOSE",sigs[1],
         "_client.cancelStopAsync();",
         "/* no hardware close */",
         "SERVER_DEFERRED_RETRY_DID_NOT_CLOSE"),
        ("STALE_INT_DISABLE",sigs[1],
         "outcome == jwplc_w5500_candidate::StatusReadOutcome::Read",
         "outcome != jwplc_w5500_candidate::StatusReadOutcome::BusBusy",
         "SERVER_STALE_CLOSED_OTHER_OWNER"),
        ("TASK_SKIPS_PENDING",sigs[2],
         "if (_jwplcDropPending && !serviceDeferredDrop()) return;",
         "if (false) return;",
         "SERVER_TASK_ENTERED_WHILE_DROP_BUSY"),
        ("TASK_NOT_READY_DROPS_HANDLE",sigs[2],
         "if (_jwplcDropPending) return;",
         "/* ignore deferred cleanup, unsafe */",
         "SERVER_LINK_DOWN_BUSY_ABANDONED_CLIENT"),
    ]
    for label,sig,a,b,expected in negatives:
        original=bodies[sig]
        require(original.count(a)==1,"SERVER_NEGATIVE_ANCHOR_MISSING_"+label)
        mutant=dict(bodies);mutant[sig]=original.replace(a,b,1)
        code,out_text=run("server_cleanup_negative_"+label,mutant)
        if code==0 or expected not in out_text:
            emit("SERVER_NEGATIVE_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+out_text.strip().replace("\n"," | ")[:600])
        require(code!=0 and expected in out_text,"SERVER_NEGATIVE_NOT_REJECTED_"+label)
    return {"result":"PASS_TWO_SOURCE_METHODS","negatives":len(negatives),
            "end_to_end":False,
            "limitations":"complete task(), serviceServer(), PHY link and RX paths remain open"}


def modbus_cleanup_native(out,files):
    """Compilar tres METODOS C++ del overlay actual y negativos integrados."""
    repro=load(MODULES["repro"],"modbus_cleanup_extract")
    cxx=load("a13_g10_p1g_c1_native_prototype.py","modbus_cleanup_compiler")
    builder=load(MODULES["b2a"],"modbus_cleanup_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_modbus_cleanup_test.cpp"
    mb=files["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp"]
    signatures=(
        "bool JWPLC_ModbusTCPClientClass::busy() const",
        "void JWPLC_ModbusTCPClientClass::closeSession()",
        "bool JWPLC_ModbusTCPClientClass::serviceDeferredClose()",
    )
    bodies={key:repro.extract(mb,key) for key in signatures}
    def compile_test(tag,versions):
        joined="\n\n".join(versions[key] for key in signatures)+"\n"
        return builder.native(out,tag,joined,cxx)
    rc,log=compile_test("modbus_cleanup_positive",bodies)
    expectations=[
        "G10_I_MB_DEFERRED_CLOSE_BUSY_RETRY=PASS",
        "G10_I_MB_STALE_REUSE_PROTECTED=PASS",
        "G10_I_MB_NO_HANDLE_IDEMPOTENT=PASS",
        "G10_I_MB_CLEANUP_NATIVE=PASS_SOURCE_METHODS_NO_PRODUCT_ADOPTION",
    ]
    if rc!=0 or not all(x in log for x in expectations):
        emit("MB_CLEANUP_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:700])
    require(rc==0 and all(x in log for x in expectations),
            "MB_CLEANUP_REAL_SOURCE_POSITIVE_FAILED")
    variants=[
        ("NO_BUSY_FLAG",signatures[0],
         "if (_jwplcClosePending) return true;",
         "if (_jwplcClosePending) return false;",
         "SPI_BUSY_DROPPED_MODBUS_HANDLE"),
        ("DROP_HANDLE_ON_BUSY",signatures[2],
         "if (!acquireBus(20)) return false; // retain _tcpClient and token",
         "if (!acquireBus(20)) { _tcpClient=EthernetClient(); return false; }",
         "SPI_BUSY_DROPPED_MODBUS_HANDLE"),
        ("SKIP_REAL_CLOSE",signatures[2],
         "_tcpClient.cancelStopAsync();",
         "/* never close socket */",
         "DEFERRED_CLOSE_DID_NOT_DRAIN"),
    ]
    for label,signature,original,replacement,expected in variants:
        version=dict(bodies)
        require(version[signature].count(original)==1,
                "MB_CLEANUP_NEGATIVE_ANCHOR_MISSING_"+label)
        version[signature]=version[signature].replace(original,replacement,1)
        code,output=compile_test("modbus_cleanup_negative_"+label,version)
        if code==0 or expected not in output:
            emit("MB_NEGATIVE_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,
                "MB_CLEANUP_NEGATIVE_NOT_REJECTED_"+label)
    return {"result":"PASS_THREE_SOURCE_METHODS","negatives":len(variants),
            "limitations":"task scheduler and Modbus end-to-end not proven"}


def b03_shutdown_risk_native(out, files):
    """Reproducción B03 source-backed; PASS significa RIESGO, no adopción."""
    repro=load(MODULES["repro"],"b03_real_methods")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b03_real_compiler")
    builder=load(MODULES["b2a"],"b03_native_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_dns_dhcp_shutdown_risk_test.cpp"
    dns=(ROOT/"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp").read_text(encoding="utf-8-sig")
    dhcp=(ROOT/"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dhcp.cpp").read_text(encoding="utf-8-sig")
    udp=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"]
    signatures=(
        ("void EthernetUDP::stop()",udp),
        ("void DNSClient::finishResolveAsync(int result)",dns),
        ("void DNSClient::cancelResolveAsync()",dns),
        ("void DhcpClass::finishAsync()",dhcp),
    )
    methods=[repro.extract(source,signature) for signature,source in signatures]
    require(all("stop()" in text for text in methods),
            "B03_SOURCE_TERMINATION_CONTRACT_DRIFT")
    rc,log=builder.native(out,"b03_shutdown_risk",
                           "\n\n".join(methods)+"\n",compiler)
    required=(
        "G10_I_B03_DNS_FINISH_BUSY_ORPHAN=RISK_REPRODUCED",
        "G10_I_B03_DNS_CANCEL_BUSY_ORPHAN=RISK_REPRODUCED",
        "G10_I_B03_DHCP_FINISH_BUSY_ORPHAN=RISK_REPRODUCED",
        "G10_I_B03_NATIVE=SOURCE_BACKED_RISK_REPRODUCED_ADOPTION_BLOCKED",
    )
    if rc!=0 or not all(marker in log for marker in required):
        emit("B03_SOURCE_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:1100])
    require(rc==0 and all(marker in log for marker in required),
            "B03_SOURCE_REPRODUCTION_FAILED_OR_BEHAVIOR_CHANGED_REVIEW")
    return {"status":"SOURCE_BACKED_RISK_REPRODUCED",
            "cpp_methods":len(methods),
            "safe_for_adoption":False,
            "limitations":"host mock; no network hardware or full DNS DHCP runtime"}


def status_probe_native(out):
    """Una fase DEL MISMO G10-I; no es un nuevo gate manual."""
    cxx=load("a13_g10_p1g_c1_native_prototype.py","g10i_cxx_status")
    builder=load(MODULES["b2a"],"g10i_status_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_status_probe_test.cpp"
    code,log=builder.native(out,"status_probe","",cxx)
    expected=(
        "G10_I_STATUS_READ_CLOSED_ESTABLISHED=PASS",
        "G10_I_STATUS_SPI_BUSY_NO_FALSE_CLOSED=PASS",
        "G10_I_STATUS_REUSE_RESET_STALE=PASS",
        "G10_I_STATUS_NATIVE=PASS_TRISTATE_PRIMITIVE_ONLY_ADOPTION_BLOCKED",
    )
    if code != 0 or not all(item in log for item in expected):
        emit("STATUS_PROBE_DIAG",log.strip().replace("\n"," | ")[:800])
    require(code==0 and all(item in log for item in expected),
            "NATIVE_TRISTATE_STATUS_PROBE_FAILED")
    return {"result":"PASS_PRIMITIVE_ONLY","log":"status_probe/native.log",
            "qualified_for_adoption":False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ci-standalone",action="store_true",
                        help="CI: solo verificación de branch ancestor, no acredita gate local")
    parser.add_argument("--require-ready",action="store_true",
                        help="Falla mientras existan bloqueos de integración")
    parser.add_argument("--stage-platform",type=Path,
                        help="Copiar package a ruta TEMPORAL EXTERNA inexistente y aplicar overlay")
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
        m["status_probe"]=status_probe_native(out)
        emit("TRISTATE_STATUS_NATIVE","PASS_PRIMITIVE_ONLY")
        m["modbus_cleanup_probe"]=modbus_cleanup_native(out,files)
        emit("MODBUS_CLEANUP_NATIVE","PASS_THREE_SOURCE_METHODS_3_NEGATIVES")
        m["modbus_server_cleanup_probe"]=server_cleanup_native(out,files)
        emit("MODBUS_SERVER_CLEANUP_NATIVE","PASS_THREE_SOURCE_METHODS_5_NEGATIVES")
        m["b03_dns_dhcp_shutdown"]=b03_shutdown_risk_native(out,files)
        emit("B03_DNS_DHCP_SHUTDOWN","RISK_REPRODUCED_BLOCKS_ADOPTION")
        if opts.stage_platform is not None:
            staged=stage_platform(opts.stage_platform,files,shas)
            m["staged_sources"]=staged
            m["staged_platform"]=str(opts.stage_platform.resolve())
            emit("ARDUINO_PLATFORM_EXPORT","STAGED_NOT_COMPILED")
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

#!/usr/bin/env python3
"""Alpha13 G10-P1G-B: testigos source-backed de rutas SPI y ownership.

PASS confirma la caracterización, NO una corrección. No modifica producto,
no compila Arduino ni toca hardware. Cuenta caminos seleccionados, no prueba
cobertura semántica exhaustiva de C++.
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

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
P1G_A_RUN="g10_p1g_a_20261012_231402_0953c0ca"
P1G_A_HEAD="e53b0e4d93ca9cd921a313d5e37b246d02856d4c"
ETH="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
MB="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/"
EXTRA={
    "W5100":ETH+"utility/w5100.cpp",
    "DHCP":ETH+"Dhcp.cpp",
    "DHCP_ASYNC":ETH+"DhcpAsyncControl.cpp",
    "DNS":ETH+"Dns.cpp",
    "README": "JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md",
}
def say(k,v):
    print("G10_P1G_B_"+str(k)+"="+str(v),flush=True)
def need(cond,label):
    if not cond:
        raise RuntimeError(label)
def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):
            h.update(b)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,text=True,capture_output=True,
                     encoding="utf-8",errors="replace",timeout=25)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def load_p1g_a():
    name="a13_g10_p1g_a_mutex_callpaths"
    path=ROOT/"tools/alpha13/gates"/(name+".py")
    spec=importlib.util.spec_from_file_location(name,path)
    need(spec is not None and spec.loader is not None,"A_GATE_NOT_IMPORTABLE")
    module=importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module
def getbody(m,src,key,method):
    arr=[x for x in m.methods(src[key]) if x["name"]==method]
    need(len(arr)==1,"METHOD_MISSING_OR_OVERLOADED_"+key+"_"+method)
    return arr[0]["body"]
def has(s,*needles):
    return all(needle in s for needle in needles)
def inspect(m,src):
    # Las comprobaciones son testigos positivos/negativos de C++ REAL:
    # no simulan ownership ni prueban ausencia de interleavings externos.
    m.check({k:src[k] for k in list(m.SOURCES)})
    C=lambda key,n:getbody(m,src,key,n)
    p=C("RUNTIME","JWPLC_EthernetClass::probeHardware")
    reset=C("RUNTIME","JWPLC_EthernetClass::resetHardwareIfNeeded")
    client_connect=C("CLIENT","EthernetClient::beginConnectAsync")
    client_stop=C("CLIENT","EthernetClient::beginStopAsync")
    client_poll_stop=C("CLIENT","EthernetClient::pollStopAsync")
    modbus_close=C("CLIENT_MODBUS","JWPLC_ModbusTCPClientClass::closeSession")
    modbus_drop=C("SERVER_MODBUS","JWPLC_ModbusTCPClass::dropClient")
    mbsc=C("SERVER_MODBUS","JWPLC_ModbusTCPClass::serviceServer")
    mbconn=C("CLIENT_MODBUS","JWPLC_ModbusTCPClientClass::serviceConnection")
    mbensure=C("SERVER_MODBUS","JWPLC_ModbusTCPClass::ensureServerListening")
    l2=C("RUNTIME","JWPLC_EthernetClass::serviceL2RefreshLocked")
    runtime_service=C("RUNTIME","JWPLC_EthernetClass::service")
    mbstask=C("SERVER_MODBUS","JWPLC_ModbusTCPClass::task")
    mbctask=C("CLIENT_MODBUS","JWPLC_ModbusTCPClientClass::task")
    witnesses=[
      ("WRAPPED_MB_SERVER_RX","LOCK_FROM_CALLER",
       has(mbsc,"if (!acquireBus(20))","_server.available();","releaseBus();") and
       has(C("SERVER","EthernetServer::available"),
           "Ethernet.socketStatus(i)","Ethernet.socketRecvAvailable(i)"),
       "Modbus serviceServer toma SPI antes de Server.available"),
      ("WRAPPED_MB_SERVER_LISTENER","LOCK_FROM_CALLER",
       has(mbensure,"if (!acquireBus(20))","_server.begin();","releaseBus();") and
       has(C("SERVER","EthernetServer::begin"),
           "Ethernet.socketBegin(","Ethernet.socketListen("),
       "Modbus ensureServerListening protege Server.begin"),
      ("WRAPPED_MB_CLIENT_CONNECT","LOCK_FROM_CALLER",
       has(mbconn,"if (!acquireBus(20))","_tcpClient.beginConnectAsync(",
           "_tcpClient.pollConnectAsync();","releaseBus();") and
       has(client_connect,"Ethernet.socketBegin(","Ethernet.socketConnect("),
       "Modbus serviceConnection protege connect cooperativo"),
      ("WRAPPED_MB_CLIENT_POLL","LOCK_FROM_CALLER",
       has(mbconn,"_tcpClient.connected()","if (!acquireBus(20))") and
       has(C("CLIENT","EthernetClient::connected"),"Ethernet.socketStatus("),
       "Modbus comprueba conexion dentro de SPI"),
      ("WRAPPED_L2_UDP","INHERITED_LOCK",
       has(runtime_service,"if (!acquireBus(50))",
           "serviceL2RefreshLocked(now);","releaseBus();") and
       has(l2,"EthernetUDP udp;","udp.begin(0)","udp.stop();"),
       "L2 UDP temporal asume lock de service"),
      ("DIRECT_RAW_CLIENT_STATUS","RAW_ENTRY_NO_JWPLC_GUARD",
       has(C("CLIENT","EthernetClient::connected"),"Ethernet.socketStatus(") and
       "jwplcSPI_acquire" not in src["CLIENT"] and
       has(C("SOCKET","EthernetClass::socketStatus"),
           "SPI.beginTransaction(","W5100.readSnSR("),
       "Client.connected directo llega a SPI sin mutex JWPLC"),
      ("DIRECT_RAW_SERVER","RAW_ENTRY_NO_JWPLC_GUARD",
       has(C("SERVER","EthernetServer::begin"),
           "Ethernet.socketBegin(","Ethernet.socketListen(") and
       "jwplcSPI_acquire" not in src["SERVER"],
       "Server.begin directo llega a socketBegin sin mutex JWPLC"),
      ("DIRECT_RAW_UDP","RAW_ENTRY_NO_JWPLC_GUARD",
       has(C("UDP","EthernetUDP::begin"),"Ethernet.socketBegin(") and
       has(C("UDP","EthernetUDP::stop"),"Ethernet.socketClose(") and
       "jwplcSPI_acquire" not in src["UDP"],
       "UDP begin/stop directo llega a W5500 sin mutex JWPLC"),
      ("RAW_TCP_STOP_DELEGATION","DELEGATED_UNGUARDED",
       has(C("CLIENT","EthernetClient::stop"),
           "beginStopAsync();","pollStopAsync();") and
       has(client_stop,"Ethernet.socketDisconnect(") and
       has(client_poll_stop,"Ethernet.socketClose("),
       "Client.stop delega a cierre de socket sin mutex local"),
      ("MB_CLIENT_CONTENTION_HANDLE_DROP","KNOWN_HAZARD",
       has(modbus_close,"if ((bool)_tcpClient && acquireBus(20))",
           "_tcpClient.cancelConnectAsync();","_tcpClient = EthernetClient();"),
       "El handle se descarta tras adquirir o no el mutex"),
      ("MB_SERVER_CONTENTION_HANDLE_DROP","KNOWN_HAZARD",
       has(modbus_drop,"if (_clientActive && acquireBus(20))",
           "_client.stop();","_client = EthernetClient();"),
       "El servidor descarta handle despues de fallo potencial SPI"),
      ("RESET_BEFORE_LOCK","KNOWN_HAZARD",
       has(p,"resetHardwareIfNeeded();","if (!acquireBus(500))") and
       p.index("resetHardwareIfNeeded();") <
       p.index("if (!acquireBus(500))") and
       has(reset,"if (_resetPin == 255)","digitalWrite(_resetPin, LOW);"),
       "Reset fisico opcional actualmente anterior al mutex"),
      ("SOCKET_LEGACY_LISTENER","KNOWN_HAZARD",
       "server_port[MAX_SOCK_NUM]" in src["SERVER"] and
       "server_port[i] == _port" in src["SERVER"] and
       "EthernetServer::server_port[s] = 0" in src["SOCKET"],
       "Listener porta solo socket->puerto sin owner ni epoch"),
      ("NOT_READY_IS_NOT_EPOCH","KNOWN_HAZARD",
       has(mbstask,"if (!JWPLC_Ethernet.isReady())",
           "_client = EthernetClient();") and
       has(mbctask,"if (!JWPLC_Ethernet.isReady())",
           "_tcpClient = EthernetClient();") and
       "return _ready;" in C("RUNTIME","JWPLC_EthernetClass::isReady"),
       "NotReady descarta handles sin acreditar reset efectivo"),
      ("NON_RECURSIVE_SPI_MUTEX","STRUCTURAL_CONSTRAINT",
       "xSemaphoreCreateMutex()" in src["SPI_MUTEX"] and
       "xSemaphoreCreateRecursiveMutex()" not in src["SPI_MUTEX"],
       "No duplicar acquire en raw APIs llamadas dentro del wrapper"),
      ("GPIO4_NOT_ENABLED_BY_DEFAULT","STRUCTURAL_CONSTRAINT",
       "#define JWPLC_ETH_RESET_PIN 255" in src["RUNTIME_H"] and
       "-DJWPLC_ETH_RESET_PIN" not in src["BOARDS"],
       "Esquema GPIO4; package default 255"),
      ("W5100_STATIC_INIT_GUARD","STRUCTURAL_CONSTRAINT",
       has(src["W5100"],"static bool initialized = false;",
           "if (initialized) return 1;"),
       "W5100.init no certifica cada reset efectivo"),
      ("RAW_APIS_DOCUMENTED","COMPATIBILITY_CONSTRAINT",
       has(src["README"],"EthernetClient client;","EthernetServer server(5000);",
           "EthernetUDP udp;"),
       "Mantener Client/Server/UDP publicos y sus APIs"),
      ("NO_SPI_IN_USES_BOOL","SOFTWARE_ONLY",
       "virtual operator bool() { return _sockindex < MAX_SOCK_NUM; }"
           in src["BACKEND_H"],
       "EthernetClient.operator bool es software, no transferencia SPI"),
    ]
    for name,cls,ok,desc in witnesses:
        need(ok,"SOURCE_WITNESS_DRIFT_"+name)
    need(len(witnesses)==19,"EXPECTED_NINETEEN_WITNESSES")
    return [{"id":n,"class":c,"source_verified":v,"observation":d}
            for n,c,v,d in witnesses]

def negatives(m,src):
    # Negativos alteran COPIAS en memoria, nunca archivos de producto.
    mutations=[
      ("REMOVE_MB_SERVER_LOCK","SERVER_MODBUS",
       "if (!acquireBus(20))","if (false)", "WRAPPED_MB_SERVER_RX"),
      ("REMOVE_MB_CLIENT_LOCK","CLIENT_MODBUS",
       "if (!acquireBus(20))","if (false)", "WRAPPED_MB_CLIENT_CONNECT"),
      ("DROP_CLIENT_HAZARD_DISAPPEARED","CLIENT_MODBUS",
       "if ((bool)_tcpClient && acquireBus(20))",
       "if ((bool)_tcpClient)", "MB_CLIENT_CONTENTION_HANDLE_DROP"),
      ("RESET_REORDER","RUNTIME",
       "    resetHardwareIfNeeded();\n\n    if (!acquireBus(500))",
       "    if (!acquireBus(500))\n\n    resetHardwareIfNeeded();",
       "RESET_BEFORE_LOCK"),
      ("LISTENER_OWNERSHIP_DRIFT","SERVER",
       "server_port[i] == _port","server_port[i] == 0", "SOCKET_LEGACY_LISTENER"),
    ]
    for label,key,old,new,wanted in mutations:
        need(src[key].count(old)>=1,"NEGATIVE_ANCHOR_MISSING_"+label)
        alt=dict(src)
        # La tabla legacy aparece en available(), accept() y operator bool().
        # Este negativo debe eliminar TODAS las lecturas, no solo la primera.
        alt[key]=src[key].replace(old,new) if label=="LISTENER_OWNERSHIP_DRIFT" else src[key].replace(old,new,1)
        try:
            rows=inspect(m,alt)
        except RuntimeError as err:
            # La prueba debe fallar por testigo o baseline productivo, no excepcion arbitraria.
            need("DRIFT" in str(err) or "CHANGED" in str(err),
                 "NEGATIVE_WRONG_FAILURE_"+label+"_"+str(err))
        else:
            raise RuntimeError("NEGATIVE_ACCEPTED_"+label)
        say("SELFTEST_"+label,"PASS_REJECTED")
    say("SELFTEST","PASS_FIVE_NEGATIVES")
    return 0

def validate(ci):
    need(Path.cwd().resolve()==ROOT and
         Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    h=git("rev-parse","HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",P1G_A_HEAD,h],
         cwd=ROOT,capture_output=True).returncode==0,"G10_P1G_A_NOT_ANCESTOR")
    need(not git("diff","--name-only") and not git("diff","--cached","--name-only"),
         "GIT_TRACKED_DIRTY")
    if not ci:
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        need(h==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_HEAD_DIFFERENT")
        p=ROOT/"tools/alpha13/results"/P1G_A_RUN/"MANIFEST.json"
        need(p.is_file(),"P1G_A_LOCAL_MANIFEST_MISSING")
        obj=json.loads(p.read_text(encoding="utf-8-sig"))
        need(obj.get("schema")=="A13-G10-P1G-A-CALLPATHS-1" and
             obj.get("run_id")==P1G_A_RUN and
             obj.get("head")==P1G_A_HEAD and
             obj.get("status")=="CALLPATH_BASELINE_CAPTURED_ADOPTION_BLOCKED",
             "P1G_A_LOCAL_MANIFEST_NOT_QUALIFIED")
    return h

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--ci-standalone",action="store_true")
    p.add_argument("--selftest",action="store_true")
    args=p.parse_args()
    m=load_p1g_a()
    src=m.sources()
    src.update({k:(ROOT/rel).read_text(encoding="utf-8-sig")
                for k,rel in EXTRA.items()})
    if args.selftest:
        inspect(m,src)
        return negatives(m,src)
    runid="g10_p1g_b_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    result={"schema":"A13-G10-P1G-B-SOURCE-WITNESSES-1",
            "run_id":runid,"status":"REVIEW_STOP","product_modified":False,
            "git_modified":False,"hardware_access":False,
            "compile":False,"candidate_adoption":False}
    try:
        head=validate(args.ci_standalone)
        archives=m.check_frozen()
        witnesses=inspect(m,src)
        (out/"WITNESSES.json").write_text(json.dumps(witnesses,indent=2,ensure_ascii=False)+"\n",encoding="utf-8")
        result.update({"status":"CALLGRAPH_WITNESSES_CAPTURED_ADOPTION_BLOCKED",
                       "head":head,"archives_frozen":archives,
                       "witness_count":len(witnesses),
                       "sources":{rel:sha(ROOT/rel)
                                  for rel in [*m.SOURCES.values(),*EXTRA.values()]},
                       "known_hazards":[x["id"] for x in witnesses
                                        if x["class"]=="KNOWN_HAZARD"],
                       "witness_sha256":sha(out/"WITNESSES.json"),
                       "candidate_status":"NOT_IMPLEMENTED"})
        say("HEAD",head)
        for w in witnesses:
            say("WITNESS_"+w["id"],w["class"])
        say("WITNESSES",len(witnesses))
        say("KNOWN_HAZARDS",len(result["known_hazards"]))
        say("ARCHIVES_FROZEN",archives)
        say("STATUS",result["status"])
        say("RESULTS",out)
        return 0
    except Exception as error:
        result["reason"]=repr(error)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(error))
        say("RESULTS",out)
        return 2
    finally:
        (out/"MANIFEST.json").write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        say("FATAL",repr(e))
        sys.exit(2)

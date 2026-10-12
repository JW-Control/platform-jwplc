#!/usr/bin/env python3
"""G10-P1D: lifecycle native de A13-006 basado en fuentes candidatas exactas.

Usa cuerpos reales closeSession, task, begin, end, dropClient, server task
extraidos de P1B (NO una reescritura semantica). Prueba reintentos y
situacion de reset/reuso de indice con sockets/bus simulados.
Resultado de riesgo debe BLOQUEAR adopcion: no es fix PASS.
No toca productos, Git, archivos .a ni hardware/COM.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
P0_ID="g10_p0_20261012_114921_34102c6b"
P1A_ID="g10_p1a_20261012_120529_a174356e"
P1B_ID="g10_p1b_20261012_124640_8458554e"
P1C_ID="g10_p1c_20261012_140152_a7211c08"
TCP=Path("JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src")
CLIENT=TCP/"JWPLC_ModbusTCP_Client.cpp"
SERVER=TCP/"JWPLC_ModbusTCP.cpp"
EXPECTED=[
 "CLIENT_PENDING_RETAINED_DURING_SPI_DENIAL",
 "CLIENT_PENDING_BLOCKS_NEW_BEGIN",
 "CLIENT_TASK_DRAINS_AFTER_SPI_RELEASE",
 "CLIENT_END_DRAINS_WHILE_IDLE",
 "SERVER_PENDING_RETAINED_DURING_SPI_DENIAL",
 "SERVER_TASK_DRAINS_AFTER_SPI_RELEASE",
 "CLIENT_RESET_REUSED_SOCKET_CLOSED_WRONG_OWNER=REPRODUCED",
 "SERVER_RESET_REUSED_SOCKET_CLOSED_WRONG_OWNER=REPRODUCED",
 "A13_006_P1D_RESULT=RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED",
]
M={}
OUT=None

def say(k,v):print("G10_P1D_"+k+"="+str(v),flush=True)
def need(ok,reason):
    if not ok:raise RuntimeError(reason)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for b in iter(lambda:f.read(1024*1024),b""):h.update(b)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True)
    need(p.returncode==0,"GIT_ERROR_"+repr(args))
    return p.stdout.decode("utf-8","replace").strip()
def module(path,name):
    spec=importlib.util.spec_from_file_location(name,ROOT/path)
    need(spec and spec.loader,"MODULE_NOT_FOUND_"+name)
    x=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=x
    spec.loader.exec_module(x)
    return x
def load_manifest(id,schema,status):
    p=ROOT/"tools/alpha13/results"/id/"MANIFEST.json"
    need(p.is_file(),"LOCAL_MANIFEST_MISSING_"+id)
    m=json.loads(p.read_text(encoding="utf-8-sig"))
    need(m.get("schema")==schema and m.get("run_id")==id and
         m.get("status")==status,"LOCAL_MANIFEST_UNQUALIFIED_"+id)
    return m
def candidate_sources(local):
    repro=module("tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py",
                 "g10_p1a_repro_for_p1d")
    candidate_generator=module("tools/alpha13/gates/a13_g10_p1b_a13_006_candidate.py",
                               "g10_p1b_candidate_for_p1d")
    orig={p.as_posix():(ROOT/p).read_bytes().decode("utf-8-sig")
          for p in candidate_generator.TARGETS}
    candidate=candidate_generator.make(orig,repro)
    if local:
        p0=load_manifest(P0_ID,"A13-G10-P0-READONLY-1",
                         "BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN")
        p1a=load_manifest(P1A_ID,"A13-G10-P1A-REPRO-1",
                          "SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN")
        p1b=load_manifest(P1B_ID,"A13-G10-P1B-CANDIDATE-1",
                          "CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN")
        p1c=load_manifest(P1C_ID,"A13-G10-P1C-ARDUINO-CANDIDATE-1",
                          "ARDUINO_BASELINE_AND_CANDIDATE_BUILDS_PASS_PRODUCT_UNCHANGED")
        need(p1c.get("candidate_run")==P1B_ID and
             p1c.get("phases",{}).get("ARDUINO_BUILDS")=="PASS" and
             p1c.get("phases",{}).get("FINAL_AUDIT")=="PASS",
             "P1C_BUILD_NOT_PROVEN")
        need(p0.get("head") and p1a.get("source_cpp_sha256") and
             p1b.get("candidate_paths"),"LOCAL_CHAIN_INCOMPLETE")
        for name in candidate:
            p=ROOT/"tools/alpha13/results"/P1B_ID/"candidate_sources"/name
            entry=p1b["candidate_paths"][name]
            need(p.is_file() and sha(p)==entry["candidate_sha256"] and
                 sha(p)==hashlib.sha256(candidate[name].encode()).hexdigest() and
                 p1c.get("candidate_sha256",{}).get(name)==sha(p),
                 "P1B_P1C_CANDIDATE_HASH_MISMATCH_"+name)
    return candidate,repro
def src_code(candidate,repro):
    client=candidate[CLIENT.as_posix()]
    server=candidate[SERVER.as_posix()]
    functions=[
        repro.extract(client,"void JWPLC_ModbusTCPClientClass::closeSession()"),
        repro.extract(client,"bool JWPLC_ModbusTCPClientClass::begin("),
        repro.extract(client,"void JWPLC_ModbusTCPClientClass::end()"),
        repro.extract(client,"void JWPLC_ModbusTCPClientClass::task()"),
        repro.extract(server,"void JWPLC_ModbusTCPClass::dropClient()"),
        repro.extract(server,"void JWPLC_ModbusTCPClass::task()"),
    ]
    signatures=[
        "JWPLC_ModbusTCPClientClass::closeSession()",
        "JWPLC_ModbusTCPClientClass::begin(",
        "JWPLC_ModbusTCPClientClass::end()",
        "JWPLC_ModbusTCPClientClass::task()",
        "JWPLC_ModbusTCPClass::dropClient()",
        "JWPLC_ModbusTCPClass::task()",
    ]
    need(all(sig in body for sig,body in zip(signatures,functions)),
         "EXTRACTED_METHODS_INCOMPLETE")
    source=CPP_HEAD+"\n\n".join(functions)+"\n"+CPP_TESTS
    need("__" not in source or "__unlikely" not in source,
         "SOURCE_ASSEMBLY_PLACEHOLDER")
    return source,signatures
CPP_HEAD=r"""
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool x,const char *msg){
    if(!x) throw std::runtime_error(msg);
}
struct Sock {char owner='-';int closed=0;};
static Sock slots[8];
static int wrongOwnerCloses=0;
static void clearSockets(){
    for(auto &s:slots) s=Sock{};
    wrongOwnerCloses=0;
}
static void assign(int id,char owner){slots[id].owner=owner;}
struct EthernetClient {
    int sock=-1;
    char expected='-';
    int timeout=1000;
    EthernetClient()=default;
    EthernetClient(int id,char owner):sock(id),expected(owner){}
    explicit operator bool() const {return sock>=0;}
    void cancelConnectAsync(){
        if(sock>=0){
            if(slots[sock].owner!=expected) ++wrongOwnerCloses;
            slots[sock].owner='-';++slots[sock].closed;
            sock=-1;
        }
    }
    void setConnectionTimeout(int t){timeout=t;}
    void stop(){cancelConnectAsync();}
};
struct AsyncTx {int n=0;void reset(){++n;}};
struct IPAddress{
    uint32_t v;
    IPAddress(uint32_t a=0):v(a){}
    IPAddress(int a,int b,int c,int d):
      v((uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<<8)|uint32_t(d)){}
    bool operator==(const IPAddress &b)const{return v==b.v;}
};
enum {
 JWPLC_MODBUS_TCP_CLIENT_IDLE=0,
 JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET,
 JWPLC_MODBUS_TCP_CLIENT_CONNECTING,
 JWPLC_MODBUS_TCP_CLIENT_SENDING,
 JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE,
 JWPLC_MODBUS_TCP_CLIENT_DONE,
 JWPLC_MODBUS_TCP_CLIENT_ERROR,
};
enum {
 JWPLC_MODBUS_TCP_CLIENT_OK=0,
 JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED,
 JWPLC_MODBUS_TCP_CLIENT_ETHERNET_NOT_READY,
 JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT,
 JWPLC_MODBUS_TCP_CLIENT_INVALID_ARGUMENT,
 JWPLC_MODBUS_TCP_CLIENT_BUSY,
 JWPLC_MODBUS_TCP_CLIENT_TIMEOUT,
};
enum {
 JWPLC_MODBUS_TCP_SERVER_LISTENING=1,
 JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET=2,
 JWPLC_MODBUS_TCP_SERVER_STOPPED=3,
 JWPLC_MODBUS_TCP_ETHERNET_NOT_READY=7,
};
struct Network {bool ready=true; bool isReady()const{return ready;}} JWPLC_Ethernet;
static uint32_t tick=0;
static uint32_t millis(){return tick;}
static void resetWorld(){clearSockets();tick=0;JWPLC_Ethernet.ready=true;}
struct JWPLC_ModbusTCPClientClass {
  EthernetClient _tcpClient;
  AsyncTx _asyncTx;
  bool _sessionConnected=false,_connectStarted=false,_sendStarted=false;
  bool _closePending=false;
  bool _configured=false;
  IPAddress _serverIP;
  uint16_t _serverPort=502;
  uint8_t _unitId=1;
  int _state=JWPLC_MODBUS_TCP_CLIENT_IDLE;
  int _result=JWPLC_MODBUS_TCP_CLIENT_NOT_CONFIGURED;
  int _exceptionCode=0;
  uint32_t _timeoutMs=0,_requestStartMs=0;
  bool allowBus=false;
  int acquires=0,releases=0,rxResets=0,txResets=0;
  bool acquireBus(int){++acquires;return allowBus;}
  void releaseBus(){++releases;}
  void resetRx(){++rxResets;}
  void resetTransactionContext(){++txResets;}
  bool busy()const{
    return _state==JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET ||
           _state==JWPLC_MODBUS_TCP_CLIENT_CONNECTING ||
           _state==JWPLC_MODBUS_TCP_CLIENT_SENDING ||
           _state==JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE;
  }
  void fail(int,bool){throw std::runtime_error("UNEXPECTED_CLIENT_FAIL");}
  void serviceConnection(){throw std::runtime_error("UNEXPECTED_SERVICE_CONNECTION");}
  void serviceSend(){throw std::runtime_error("UNEXPECTED_SERVICE_SEND");}
  void serviceResponse(){throw std::runtime_error("UNEXPECTED_SERVICE_RESPONSE");}
  void closeSession();
  bool begin(IPAddress,uint8_t,uint16_t);
  void end();
  void task();
};
struct JWPLC_ModbusTCPClass {
  EthernetClient _client;
  bool _clientActive=false,_serverEnabled=true,
       _serverListening=true,_dropPending=false;
  int _serverState=JWPLC_MODBUS_TCP_SERVER_LISTENING;
  bool allowBus=false;
  int acquires=0,releases=0,rxIntDisables=0,rxResets=0,
      ensureCalls=0,serviceCalls=0,rxIntResets=0;
  bool acquireBus(int){++acquires;return allowBus;}
  void releaseBus(){++releases;}
  void disableRxIntLocked(){++rxIntDisables;}
  void resetRx(){++rxResets;}
  void resetRxIntSoftware(){++rxIntResets;}
  void setError(int){}
  void ensureServerListening(){++ensureCalls;}
  void serviceServer(){++serviceCalls;}
  void dropClient();
  void task();
};
"""
CPP_TESTS=r"""
int main(){
 try {
  // Cliente: fallar SPI, bloquear begin nuevo sin perder handle,
  // y limpiar desde task() aun cuando busy()==false.
  resetWorld();
  JWPLC_ModbusTCPClientClass c;
  c._tcpClient=EthernetClient(2,'C');assign(2,'C');
  c._sessionConnected=true;c.allowBus=false;c.closeSession();
  check(c._closePending && c._tcpClient.sock==2 && slots[2].owner=='C',
        "CLIENT_CLOSE_DENIED_DID_NOT_RETAIN");
  std::cout<<"CLIENT_PENDING_RETAINED_DURING_SPI_DENIAL\n";
  check(!c.begin(IPAddress(10,0,0,5),1,502) &&
        c._result==JWPLC_MODBUS_TCP_CLIENT_BUSY &&
        c._tcpClient.sock==2,"CLIENT_BEGIN_REOPENED_DURING_PENDING");
  std::cout<<"CLIENT_PENDING_BLOCKS_NEW_BEGIN\n";
  c.task();
  check(c._closePending && c._tcpClient.sock==2 &&
        slots[2].closed==0,"CLIENT_TASK_SPIN_ON_SPI_DENIED");
  c.allowBus=true;c.task();
  check(!c._closePending && !c._tcpClient &&
        slots[2].closed==1 && c.releases==1,
        "CLIENT_TASK_DID_NOT_DRAIN");
  std::cout<<"CLIENT_TASK_DRAINS_AFTER_SPI_RELEASE\n";

  resetWorld();
  JWPLC_ModbusTCPClientClass c2;
  c2._tcpClient=EthernetClient(2,'C');assign(2,'C');
  c2.allowBus=false;c2.end();
  check(c2._closePending && !c2._configured && !c2.busy(),
        "CLIENT_END_NOT_MARKED_PENDING");
  c2.allowBus=true;c2.task();
  check(!c2._closePending && slots[2].closed==1,
        "CLIENT_END_IDLE_DID_NOT_DRAIN");
  std::cout<<"CLIENT_END_DRAINS_WHILE_IDLE\n";

  // Servidor: cierre SPI denegado conserva socket y task() drena cuando
  // regresa SPI, sin llamar servicio antes del drenaje.
  resetWorld();
  JWPLC_ModbusTCPClass s;
  s._client=EthernetClient(3,'S');assign(3,'S');
  s._clientActive=true;s.allowBus=false;s.dropClient();
  check(s._dropPending && s._client.sock==3 && slots[3].owner=='S',
        "SERVER_DENIED_DID_NOT_RETAIN");
  std::cout<<"SERVER_PENDING_RETAINED_DURING_SPI_DENIAL\n";
  s.task();
  check(s._dropPending && s.serviceCalls==0,
        "SERVER_ACCEPTED_DURING_PENDING");
  s.allowBus=true;s.task();
  check(!s._dropPending && !s._clientActive && slots[3].closed==1 &&
        s.rxIntDisables==1 && s.releases==1,
        "SERVER_TASK_DID_NOT_DRAIN");
  std::cout<<"SERVER_TASK_DRAINS_AFTER_SPI_RELEASE\n";

  // Contraejemplo de seguridad del candidato P1B (NO es un PASS de fix):
  // reset W5500 invalida la generación del socket y otro subsistema puede
  // asignar el mismo índice al regresar Ethernet READY. El cierre pendiente
  // antiguo no sabe distinguir la nueva propiedad.
  resetWorld();
  JWPLC_ModbusTCPClientClass reusedClient;
  reusedClient._tcpClient=EthernetClient(2,'C');assign(2,'C');
  reusedClient.allowBus=false;reusedClient.closeSession();
  JWPLC_Ethernet.ready=false;
  assign(2,'X'); // nueva generación, socket asignado a otro propietario
  reusedClient.task(); // no cierra mientras Ethernet no esta READY
  check(wrongOwnerCloses==0 && slots[2].owner=='X',
        "CLIENT_CLOSED_DURING_NOT_READY");
  JWPLC_Ethernet.ready=true;reusedClient.allowBus=true;
  reusedClient.task();
  check(wrongOwnerCloses==1 && slots[2].closed==1,
        "CLIENT_REUSE_HAZARD_NOT_REPRODUCED");
  std::cout<<"CLIENT_RESET_REUSED_SOCKET_CLOSED_WRONG_OWNER=REPRODUCED\n";

  resetWorld();
  JWPLC_ModbusTCPClass reusedServer;
  reusedServer._client=EthernetClient(3,'S');assign(3,'S');
  reusedServer._clientActive=true;reusedServer.allowBus=false;
  reusedServer.dropClient();
  JWPLC_Ethernet.ready=false;
  assign(3,'X');
  reusedServer.task();
  check(wrongOwnerCloses==0 && slots[3].owner=='X',
        "SERVER_CLOSED_DURING_NOT_READY");
  JWPLC_Ethernet.ready=true;reusedServer.allowBus=true;
  reusedServer.task();
  check(wrongOwnerCloses==1 && slots[3].closed==1,
        "SERVER_REUSE_HAZARD_NOT_REPRODUCED");
  std::cout<<"SERVER_RESET_REUSED_SOCKET_CLOSED_WRONG_OWNER=REPRODUCED\n";
  std::cout<<"A13_006_P1D_RESULT=RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED\n";
  return 0;
 } catch(const std::exception &e) {
  std::cerr<<"P1D_NATIVE_TEST_FAILED="<<e.what()<<"\n";
  return 3;
 }
}
"""
def main():
    global OUT
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-only",action="store_true")
    p.add_argument("--native",action="store_true")
    p.add_argument("--ci-standalone",action="store_true",
                   help="CI uses git ancestry and current sources, no Windows manifests")
    args=p.parse_args()
    need(args.source_only!=args.native,"SELECT_EXACTLY_ONE_MODE")
    runid="g10_p1d_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    OUT=ROOT/"tools/alpha13/results"/runid
    OUT.mkdir(parents=True,exist_ok=False)
    M.update({"schema":"A13-G10-P1D-LIFECYCLE-1","run_id":runid,
              "status":"REVIEW_STOP","product_modified":False,
              "git_modified":False,"hardware_access":False,
              "firmware_uploaded":False,"git_commit":False,
              "candidate_adoption":False})
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
             "REPO_ROOT_REQUIRED")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "LOCAL_REMOTE_TRACKING_DIFFER")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"PRODUCT_GIT_NOT_CLEAN")
        candidate,repro=candidate_sources(not args.ci_standalone)
        source,methods=src_code(candidate,repro)
        cpp=OUT/"a13_006_p1d_lifecycle.cpp"
        cpp.write_text(source,encoding="utf-8")
        M["candidate_source_sha256"]={
            name:hashlib.sha256(data.encode()).hexdigest()
            for name,data in candidate.items()}
        M["native_cpp_sha256"]=sha(cpp)
        M["extracted_methods"]=methods
        say("SOURCE_CONTRACT","PASS_SIX_REAL_CANDIDATE_METHODS")
        if args.native:
            compiler,kind=repro.compiler()
            need(compiler,"NATIVE_CPP_COMPILER_NOT_FOUND")
            exe=OUT/("p1d.exe" if os.name=="nt" else "p1d")
            argv=([compiler,"/nologo","/EHsc","/std:c++17",str(cpp),
                   "/Fe:"+str(exe)] if kind=="msvc" else
                  [compiler,"-std=c++17","-O0","-Wall","-Wextra",
                   "-o",str(exe),str(cpp)])
            compile=subprocess.run(argv,cwd=OUT,capture_output=True,text=True,
                                   errors="replace",timeout=90)
            (OUT/"compiler.log").write_text(
                "COMMAND="+repr(argv)+"\nEXIT="+str(compile.returncode)+
                "\n"+compile.stdout+"\n"+compile.stderr,encoding="utf-8")
            need(compile.returncode==0 and exe.is_file(),
                 "HOST_CXX_COMPILE_FAILED_SEE_LOG")
            run=subprocess.run([str(exe)],cwd=OUT,capture_output=True,
                               text=True,errors="replace",timeout=15)
            (OUT/"native.log").write_text(
                "EXIT="+str(run.returncode)+"\n"+run.stdout+"\n"+run.stderr,
                encoding="utf-8")
            need(run.returncode==0,"HOST_NATIVE_LIFECYCLE_FAILED_SEE_LOG")
            for marker in EXPECTED:
                need(marker in run.stdout,"NATIVE_EXPECTED_MARKER_MISSING_"+marker)
            say("NATIVE","PASS_SOURCE_BACKED_LIFECYCLE_AND_RESET_COUNTEREXAMPLES")
            M["native_markers"]=EXPECTED
            M["status"]="RESET_REUSE_RISK_REPRODUCED_ADOPTION_BLOCKED"
        else:
            M["status"]="SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN_ADOPTION_BLOCKED"
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),
             "PRODUCT_MUTATED_DURING_TEST")
        M["head"]=head
        say("STATUS",M["status"])
        say("RESULTS",OUT)
        return 0
    except Exception as error:
        M["reason"]=repr(error)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(error))
        say("RESULTS",OUT)
        return 2
    finally:
        (OUT/"MANIFEST.json").write_text(
            json.dumps(M,ensure_ascii=False,indent=2)+"\n",
            encoding="utf-8")

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as error:
        say("FATAL",repr(error))
        sys.exit(2)

#!/usr/bin/env python3
"""G10-P1B A13-006: generar 4 fuentes candidatas TCP aisladas y probar contrato nativo.

Modo --source-only: fuentes candidatas + guardas, sin compilador ni hardware.
Modo --native: ademas compila con C++ host cuerpos reales de candidato bajo mocks SPI.
No modifica archivos de producto, staging, Git ni hardware. NO adopcion.
"""
from __future__ import annotations
import argparse
from datetime import datetime
import difflib
import hashlib
import importlib.util
import json
import os
from pathlib import Path, PureWindowsPath, PurePosixPath
import shutil
import subprocess
import sys
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
G10_P0_HEAD="bd6787711c1480f9abf2fe79ca16352e03ad4e02"
P0_ID="g10_p0_20261012_114921_34102c6b"
P1A_ID="g10_p1a_20261012_120529_a174356e"
BASE=Path("JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src")
CLIENT_CPP=BASE/"JWPLC_ModbusTCP_Client.cpp"
CLIENT_H=BASE/"JWPLC_ModbusTCP_Client.h"
SERVER_CPP=BASE/"JWPLC_ModbusTCP.cpp"
SERVER_H=BASE/"JWPLC_ModbusTCP.h"
TARGETS=[CLIENT_CPP,CLIENT_H,SERVER_CPP,SERVER_H]
METHODS={
"client_close":"void JWPLC_ModbusTCPClientClass::closeSession()",
"client_task":"void JWPLC_ModbusTCPClientClass::task()",
"client_begin":"bool JWPLC_ModbusTCPClientClass::begin(",
"client_req":"bool JWPLC_ModbusTCPClientClass::startRequest(",
"client_multi":"bool JWPLC_ModbusTCPClientClass::startWriteMultipleRequest(",
"server_drop":"void JWPLC_ModbusTCPClass::dropClient()",
"server_task":"void JWPLC_ModbusTCPClass::task()",
"server_begin":"bool JWPLC_ModbusTCPClass::beginServer(",
"server_connected":"bool JWPLC_ModbusTCPClass::clientConnected() const",
}
EXPECTED={
  "CLIENT_DENIED_HANDLE_RETAINED",
  "CLIENT_RECOVERY_CLOSED_ONCE",
  "CLIENT_IMMEDIATE_CLOSE",
  "CLIENT_NO_HANDLE",
  "SERVER_DENIED_HANDLE_RETAINED",
  "SERVER_RECOVERY_CLOSED_ONCE",
  "SERVER_IMMEDIATE_CLOSE",
  "SERVER_NO_HANDLE",
}
OUT=None
M={}

def say(k,v):print("G10_P1B_"+k+"="+str(v),flush=True)
def need(ok,why):
    if not ok:raise RuntimeError(why)
def sha(p):
    h=hashlib.sha256()
    with Path(p).open("rb") as f:
        for piece in iter(lambda:f.read(1024*1024),b""):h.update(piece)
    return h.hexdigest()
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True)
    need(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.decode("utf-8","replace").strip()

def repo_key(path):
    """Clave portable de ruta relativa usada por manifests P0/P1A y Git.

    Path.__str__ en Windows produce barras inversas. Los manifests previos
    registraron rutas POSIX. Mantener una sola representación en ambos SO.
    """
    need(not path.is_absolute() and ".." not in path.parts,
         "UNSAFE_RELATIVE_REPO_PATH")
    return path.as_posix()

def verify_local_provenance(p0,p1a):
    """Validación realmente usada en modo local (también por selftest CI)."""
    for path in [CLIENT_CPP,SERVER_CPP]:
        name=repo_key(path)
        actual=sha(ROOT/path)
        need(p0.get("sources",{}).get(name,{}).get("sha256")==actual and
             p1a.get("source_cpp_sha256",{}).get(name)==actual,
             "SOURCE_PROVENANCE_CHANGED_"+name)

def selftest_paths():
    for path in [CLIENT_CPP,CLIENT_H,SERVER_CPP,SERVER_H]:
        expected=path.as_posix()
        windows=PureWindowsPath(expected)
        posix=PurePosixPath(expected)
        need(repo_key(windows)==expected and repo_key(posix)==expected,
             "CROSS_PLATFORM_MANIFEST_KEY_MISMATCH_"+expected)
    p0={"sources":{repo_key(path):{"sha256":sha(ROOT/path)}
                   for path in [CLIENT_CPP,SERVER_CPP]}}
    p1a={"source_cpp_sha256":{repo_key(path):sha(ROOT/path)
                            for path in [CLIENT_CPP,SERVER_CPP]}}
    verify_local_provenance(p0,p1a)
    say("SELFTEST_MANIFEST_PATHS","PASS_WINDOWS_AND_POSIX")
    # Rechazar mismatches; la prueba positiva no debe relajar provenance.
    p0["sources"][repo_key(CLIENT_CPP)]["sha256"]="0"*64
    try:
        verify_local_provenance(p0,p1a)
    except RuntimeError as exc:
        need("SOURCE_PROVENANCE_CHANGED_" in str(exc),
             "NEGATIVE_PROVENANCE_WRONG_FAILURE")
    else:
        raise RuntimeError("BAD_MANIFEST_SHA_ACCEPTED")
    say("SELFTEST_NEGATIVE_PROVENANCE","PASS_REJECTED")
    return 0
def load_repro():
    path=ROOT/"tools/alpha13/gates/a13_g10_p1a_a13_006_repro.py"
    spec=importlib.util.spec_from_file_location("g10_p1a_ref",path)
    need(spec is not None and spec.loader is not None,"P1A_PY_IMPORT_BAD")
    mod=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=mod
    spec.loader.exec_module(mod)
    return mod
def exact(s,old,new,label):
    need(s.count(old)==1,"ANCHOR_MISSING_OR_DUPLICATE_"+label)
    return s.replace(old,new,1)
def function(src,signature,fn,repro):
    before=repro.extract(src,signature)
    after=fn(before)
    need(before!=after,"FUNCTION_NO_CHANGE_"+signature)
    return exact(src,before,after,"FUNCTION_"+signature)
def splice(fn,old,new,label):
    return exact(fn,old,new,label)
def eol(s):
    need(not ("\r\n" in s and s.count("\r\n")!=s.count("\n")),
         "MIXED_EOL_REVIEW")
    return "\r\n" if "\r\n" in s else "\n"

def make(orig,repro):
    c=orig[repo_key(CLIENT_CPP)]
    ch=orig[repo_key(CLIENT_H)]
    s=orig[repo_key(SERVER_CPP)]
    sh=orig[repo_key(SERVER_H)]
    ch=exact(ch,"    bool _sendStarted;\n".replace("\n",eol(ch)),
             "    bool _sendStarted;\n    bool _closePending;\n".replace("\n",eol(ch)),
             "CLIENT_PRIVATE_PENDING_FIELD")
    sh=exact(sh,"    bool _clientActive;\n".replace("\n",eol(sh)),
             "    bool _clientActive;\n    bool _dropPending;\n".replace("\n",eol(sh)),
             "SERVER_PRIVATE_PENDING_FIELD")

    c=exact(c,"      _sendStarted(false),\n".replace("\n",eol(c)),
            "      _sendStarted(false),\n      _closePending(false),\n".replace("\n",eol(c)),
            "CLIENT_CONSTRUCTOR")
    s=exact(s,"      _clientActive(false),\n".replace("\n",eol(s)),
            "      _clientActive(false),\n      _dropPending(false),\n".replace("\n",eol(s)),
            "SERVER_CONSTRUCTOR")

    close_old=repro.extract(c,METHODS["client_close"])
    close_new="""void JWPLC_ModbusTCPClientClass::closeSession()
{
    _asyncTx.reset();

    if ((bool)_tcpClient)
    {
        if (!acquireBus(20))
        {
            // Mantener el handle: no se cerro el W5500 sin SPI.
            _closePending = true;
            _sessionConnected = false;
            _connectStarted = false;
            _sendStarted = false;
            resetRx();
            return;
        }

        _tcpClient.cancelConnectAsync();
        releaseBus();
    }

    _tcpClient = EthernetClient();
    _closePending = false;
    _sessionConnected = false;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();
}""".replace("\n",eol(c))
    c=exact(c,close_old,close_new,"CLIENT_CLOSE_FULL_BODY")

    def client_gate(fn):
        anchor="    if (busy())\n".replace("\n",eol(c))
        replacement="""    if (_closePending)
    {
        closeSession();
        if (_closePending)
        {
            _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;
            return false;
        }
    }

    if (busy())
""".replace("\n",eol(c))
        return splice(fn,anchor,replacement,"CLIENT_GATE_BUSY")
    for x in ["client_begin","client_req","client_multi"]:
        c=function(c,METHODS[x],client_gate,repro)

    def begin_contract(fn):
        return splice(fn,
            "    closeSession();\n\n    _serverIP".replace("\n",eol(c)),
            """    closeSession();
    if (_closePending)
    {
        // No reconfigurar mientras el socket anterior siga vivo.
        return false;
    }

    _serverIP""".replace("\n",eol(c)),
            "CLIENT_BEGIN_CLOSE_RESULT")
    c=function(c,METHODS["client_begin"],begin_contract,repro)

    def client_task(fn):
        return splice(fn,
            """{
    if (!busy())
""".replace("\n",eol(c)),
            """{
    // Drenar incluso con _state=ERROR/IDLE (busy() == false).
    if (_closePending)
    {
        if (!JWPLC_Ethernet.isReady())
        {
            return;
        }
        closeSession();
        if (_closePending)
        {
            return;
        }
    }

    if (!busy())
""".replace("\n",eol(c)),"CLIENT_TASK_CLEANUP_BEFORE_BUSY")
    c=function(c,METHODS["client_task"],client_task,repro)

    drop_old=repro.extract(s,METHODS["server_drop"])
    drop_new="""void JWPLC_ModbusTCPClass::dropClient()
{
    if (_clientActive)
    {
        if (!acquireBus(20))
        {
            // No perder el socket ni aceptar otro cliente mientras SPI esta ocupado.
            _dropPending = true;
            return;
        }

        disableRxIntLocked();
        _client.setConnectionTimeout(0);
        _client.stop();
        releaseBus();
    }

    _client = EthernetClient();
    _clientActive = false;
    _dropPending = false;
    resetRx();

    if (_serverEnabled && JWPLC_Ethernet.isReady())
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
    }
    else if (_serverEnabled)
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
    }
    else
    {
        _serverState = JWPLC_MODBUS_TCP_SERVER_STOPPED;
    }
}""".replace("\n",eol(s))
    s=exact(s,drop_old,drop_new,"SERVER_DROP_FULL_BODY")

    def server_task(fn):
        return splice(fn,
            """    if (!_serverEnabled)
    {
        return;
    }

    if (!JWPLC_Ethernet.isReady())
""".replace("\n",eol(s)),
            """    if (!_serverEnabled)
    {
        return;
    }

    if (_dropPending)
    {
        // No se reutiliza el socket durante el cierre pendiente.
        if (!JWPLC_Ethernet.isReady())
        {
            return;
        }
        dropClient();
        if (_dropPending)
        {
            return;
        }
    }

    if (!JWPLC_Ethernet.isReady())
""".replace("\n",eol(s)),"SERVER_TASK_PRIORITIZED_DROP")
    s=function(s,METHODS["server_task"],server_task,repro)

    def server_begin(fn):
        return splice(fn,"    _clientActive = false;\n    _client = EthernetClient();".replace("\n",eol(s)),
            "    _clientActive = false;\n    _dropPending = false;\n    _client = EthernetClient();".replace("\n",eol(s)),
            "SERVER_INIT_PENDING")
    s=function(s,METHODS["server_begin"],server_begin,repro)
    def server_connected(fn):
        return splice(fn,"    return _clientActive;","    return _clientActive && !_dropPending;",
                      "SERVER_CONNECTED_PENDING_GUARD")
    s=function(s,METHODS["server_connected"],server_connected,repro)

    for path,src in [(CLIENT_CPP,c),(SERVER_CPP,s)]:
        need(src.count("void JWPLC_")>=3,"SOURCE_LARGE_CONTENT_MISSING_"+str(path))
    for before,after,path in [(orig[repo_key(CLIENT_CPP)],c,CLIENT_CPP),
                              (orig[repo_key(SERVER_CPP)],s,SERVER_CPP),
                              (orig[repo_key(CLIENT_H)],ch,CLIENT_H),
                              (orig[repo_key(SERVER_H)],sh,SERVER_H)]:
        need(eol(before)==eol(after),"SOURCE_EOL_CHANGED_"+str(path))
        need(len(list(difflib.unified_diff(
             before.splitlines(),after.splitlines(),n=0)))<140,
             "SOURCE_DIFF_TOO_WIDE_"+str(path))
    return {repo_key(CLIENT_CPP):c,repo_key(CLIENT_H):ch,
            repo_key(SERVER_CPP):s,repo_key(SERVER_H):sh}

def build_native(c,repro):
    native=repro.CPP_TEMPLATE
    needle="bool _sessionConnected=false,_connectStarted=false,_sendStarted=false;"
    need(native.count(needle)==1,"NATIVE_CLIENT_STUB_MISSING")
    native=native.replace(needle,needle+"\n    bool _closePending=false;",1)
    needle2="bool _clientActive=false,_serverEnabled=true;"
    need(native.count(needle2)==1,"NATIVE_SERVER_STUB_MISSING")
    native=native.replace(needle2,needle2+"\n    bool _dropPending=false;",1)
    native=native.replace("__SOURCE_CLIENT__",
                          repro.extract(c[repo_key(CLIENT_CPP)],METHODS["client_close"]))
    native=native.replace("__SOURCE_SERVER__",
                          repro.extract(c[repo_key(SERVER_CPP)],METHODS["server_drop"]))
    native=native.split("int main(){")[0]
    native+=r'''
int main() {
 try {
  {
   resetSockets();sockets[2]=true;
   JWPLC_ModbusTCPClientClass x;
   x._tcpClient=EthernetClient(2);
   x._sessionConnected=true;
   x.allowBus=false;
   x.closeSession();
   require(x._closePending && x._tcpClient.sock==2 && sockets[2]
       && closes[2]==0 && !x._sessionConnected,
       "CLIENT_DENIED_LOST_HANDLE");
   std::cout<<"CLIENT_DENIED_HANDLE_RETAINED\n";
   x.closeSession(); // persistent contention, no socket reuse
   require(x._closePending && x._tcpClient.sock==2 && sockets[2]
       && closes[2]==0, "CLIENT_REPEATED_DENIAL_LOST_HANDLE");
   x.allowBus=true;
   x.closeSession();
   require(!x._closePending && !x._tcpClient && !sockets[2]
       && closes[2]==1,"CLIENT_RECOVERY_LEAK_OR_DOUBLE_CLOSE");
   std::cout<<"CLIENT_RECOVERY_CLOSED_ONCE\n";
  }
  {
   resetSockets();sockets[2]=true;
   JWPLC_ModbusTCPClientClass x;
   x._tcpClient=EthernetClient(2);x.allowBus=true;
   x.closeSession();
   require(!x._tcpClient && !sockets[2] && closes[2]==1,
           "CLIENT_NORMAL_CLOSURE_FAILED");
   std::cout<<"CLIENT_IMMEDIATE_CLOSE\n";
  }
  {
   resetSockets();JWPLC_ModbusTCPClientClass x;
   x.closeSession();
   require(x.tries==0 && !x._closePending && !x._tcpClient,
           "CLIENT_EMPTY_TOOK_SPI");
   std::cout<<"CLIENT_NO_HANDLE\n";
  }
  {
   resetSockets();sockets[3]=true;
   JWPLC_ModbusTCPClass x;
   x._client=EthernetClient(3);x._clientActive=true;
   x.allowBus=false;
   x.dropClient();
   require(x._dropPending && x._clientActive && x._client.sock==3
          && sockets[3] && closes[3]==0 && x.rxIntDisables==0,
          "SERVER_DENIED_LOST_HANDLE");
   std::cout<<"SERVER_DENIED_HANDLE_RETAINED\n";
   x.dropClient();
   require(x._dropPending && x._clientActive && x._client.sock==3
          && closes[3]==0,"SERVER_REPEATED_DENIAL_LOST_HANDLE");
   x.allowBus=true;
   x.dropClient();
   require(!x._dropPending && !x._clientActive && !x._client
          && closes[3]==1 && !sockets[3] && x.rxIntDisables==1,
          "SERVER_RECOVERY_LEAK_OR_DOUBLE_CLOSE");
   std::cout<<"SERVER_RECOVERY_CLOSED_ONCE\n";
  }
  {
   resetSockets();sockets[3]=true;
   JWPLC_ModbusTCPClass x;
   x._client=EthernetClient(3);x._clientActive=true;x.allowBus=true;
   x.dropClient();
   require(!x._clientActive && !x._dropPending && closes[3]==1
           && !sockets[3],"SERVER_NORMAL_CLOSE_FAILED");
   std::cout<<"SERVER_IMMEDIATE_CLOSE\n";
  }
  {
   resetSockets();JWPLC_ModbusTCPClass x;
   x.dropClient();
   require(x.tries==0 && !x._dropPending && !x._clientActive,
           "SERVER_EMPTY_TOOK_SPI");
   std::cout<<"SERVER_NO_HANDLE\n";
  }
  std::cout<<"A13_006_CANDIDATE=PASS_LOCK_FAIL_RETRY_SOCKET_HANDLE\n";
  return 0;
 } catch(const std::exception&e) {
  std::cerr<<"A13_006_CANDIDATE_FAIL="<<e.what()<<"\n";
  return 4;
 }
}
'''
    for x in EXPECTED:
        need(x in native,"NATIVE_EXPECTED_MARKER_MISSING_"+x)
    return native

def compile_host(text,output,repro):
    compiler,kind=repro.compiler()
    need(compiler,"HOST_COMPILER_REQUIRED_FOR_NATIVE")
    source=output/"candidate_native.cpp"
    source.write_text(text,encoding="utf-8")
    exe=output/("candidate_native.exe" if os.name=="nt" else "candidate_native")
    args=([compiler,"/nologo","/EHsc","/std:c++17",str(source),
           "/Fe:"+str(exe)] if kind=="msvc" else
          [compiler,"-std=c++17","-O0","-Wall","-Wextra","-o",str(exe),str(source)])
    cp=subprocess.run(args,cwd=output,capture_output=True,text=True,
                      errors="replace",timeout=90)
    (output/"compiler.log").write_text(repr(args)+"\n"+cp.stdout+"\n"+cp.stderr,
                                      encoding="utf-8")
    need(cp.returncode==0 and exe.is_file(),"NATIVE_COMPILE_FAILED_SEE_LOG")
    run=subprocess.run([str(exe)],cwd=output,capture_output=True,text=True,
                       errors="replace",timeout=12)
    (output/"native_result.log").write_text(run.stdout+"\n"+run.stderr,
                                           encoding="utf-8")
    need(run.returncode==0,"NATIVE_RUN_FAILED_SEE_LOG")
    for key in EXPECTED:
        need(key in run.stdout,"NATIVE_SCENARIO_MISSING_"+key)
    need("A13_006_CANDIDATE=PASS_LOCK_FAIL_RETRY_SOCKET_HANDLE" in run.stdout,
         "NATIVE_FINAL_PASS_MISSING")
    say("NATIVE","PASS_EIGHT_MARKERS_SOURCE_BACKED")
    return {"compiler":str(compiler),"native_code":run.returncode,
            "native_source_sha256":sha(source),
            "scenarios":sorted(EXPECTED)}

def main():
    global OUT,M
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--source-only",action="store_true")
    p.add_argument("--native",action="store_true")
    p.add_argument("--ci-standalone",action="store_true",
                   help="in CI accept HEAD ancestry without user-local manifests")
    p.add_argument("--selftest-paths",action="store_true",
                   help="exercise actual local manifest lookup with Windows/POSIX paths")
    args=p.parse_args()
    if args.selftest_paths:
        need(not args.source_only and not args.native and
             not args.ci_standalone,"SELFTEST_MODE_EXCLUSIVE")
        return selftest_paths()
    need(args.source_only != args.native,"SELECT_EXACTLY_ONE_MODE")
    rid="g10_p1b_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    OUT=ROOT/"tools/alpha13/results"/rid
    OUT.mkdir(parents=True,exist_ok=False)
    M={"schema":"A13-G10-P1B-CANDIDATE-1","run_id":rid,
       "status":"REVIEW_STOP","mode":"native" if args.native else "source-only",
       "product_mutated":False,"git_mutated":False,"hardware_access":False,
       "candidate_paths":{}}
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
             "REPO_ROOT_REQUIRED")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "HEAD_DIFFERS_FROM_REMOTE_TRACKING")
        need(subprocess.run(["git","merge-base","--is-ancestor",
                            G10_P0_HEAD,head],cwd=ROOT).returncode==0,
             "P0_HEAD_NOT_IN_HISTORY")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"WORKTREE_DIRTY")
        old={}
        for path in TARGETS:
            f=ROOT/path
            need(f.is_file(),"FILE_MISSING_"+str(path))
            old[str(path)]=f.read_bytes().decode("utf-8-sig")
        if not args.ci_standalone:
            local=ROOT/"tools/alpha13/results"/P0_ID/"MANIFEST.json"
            repro_local=ROOT/"tools/alpha13/results"/P1A_ID/"MANIFEST.json"
            need(local.is_file() and repro_local.is_file(),
                 "P0_OR_P1A_LOCAL_EVIDENCE_MISSING")
            p0=json.loads(local.read_text(encoding="utf-8-sig"))
            p1a=json.loads(repro_local.read_text(encoding="utf-8-sig"))
            need(p0.get("schema")=="A13-G10-P0-READONLY-1" and
                 p0.get("head")==G10_P0_HEAD and
                 p0.get("status")=="BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN" and
                 p1a.get("schema")=="A13-G10-P1A-REPRO-1" and
                 p1a.get("status")=="SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN",
                 "INVALID_LOCAL_EVIDENCE")
            verify_local_provenance(p0,p1a)
        repro=load_repro()
        candidate=make(old,repro)
        output=OUT/"candidate_sources"
        for name,s in candidate.items():
            f=output/name
            f.parent.mkdir(parents=True,exist_ok=True)
            f.write_bytes(s.encode("utf-8"))
            need(sha(f)!=sha(ROOT/name),"CANDIDATE_IDENTICAL_"+name)
            M["candidate_paths"][name]={"old_sha256":sha(ROOT/name),
                                         "candidate_sha256":sha(f),
                                         "path":str(f)}
        say("SOURCE_CONTRACT","PASS_FOUR_PRIVATE_CANDIDATES")
        native=build_native(candidate,repro)
        (OUT/"candidate_native.cpp").write_text(native,encoding="utf-8")
        if args.native:
            M["native"]=compile_host(native,OUT,repro)
            M["status"]="CANDIDATE_NATIVE_PASS_PRODUCT_UNCHANGED"
        else:
            M["status"]="CANDIDATE_SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN"
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),"GIT_MUTATED")
        for name,d in M["candidate_paths"].items():
            need(sha(ROOT/name)==d["old_sha256"],"PRODUCT_MUTATED_"+name)
        M["head"]=head
        M["provenance"]=("CI_GIT_CURRENT_SOURCE" if args.ci_standalone
                         else "LOCAL_G10_P0_AND_P1A")
        say("STATUS",M["status"])
        say("RESULTS",OUT)
        return 0
    except Exception as ex:
        M["reason"]=repr(ex)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(ex))
        say("RESULTS",OUT)
        return 2
    finally:
        (OUT/"MANIFEST.json").write_text(
            json.dumps(M,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as ex:
        say("FATAL",repr(ex))
        sys.exit(2)

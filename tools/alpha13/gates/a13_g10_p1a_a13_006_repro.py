#!/usr/bin/env python3
"""G10-P1A: reproduce A13-006 from LIVE source in a native C++ test shim.

Extracts actual closeSession() and dropClient() method bodies. A reproduction
PASS demonstrates the existing defect under SPI lock failure, NOT its fix.
Does not change product, Git, hardware, sketch, archives or flash.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
G10_P0_HEAD="bd6787711c1480f9abf2fe79ca16352e03ad4e02"
CLIENT="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp"
SERVER="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
P0_ID="g10_p0_20261012_114921_34102c6b"
EXPECTED=[
    "A13_006_CLIENT_LOCK_DENIED_SOCKET_ORPHAN=REPRODUCED",
    "A13_006_CLIENT_LOCK_GRANTED_CLOSURE=PASS",
    "A13_006_CLIENT_NO_SOCKET_NO_LOCK=PASS",
    "A13_006_SERVER_LOCK_DENIED_SOCKET_ORPHAN=REPRODUCED",
    "A13_006_SERVER_LOCK_GRANTED_CLOSURE=PASS",
    "A13_006_SERVER_NO_CLIENT_NO_LOCK=PASS",
    "A13_006_RESULT=BASELINE_DEFECT_REPRODUCED_NOT_FIXED",
]
CPP_TEMPLATE=r'''
#include <iostream>
#include <stdexcept>
#include <string>
static bool sockets[8]={};
static int closes[8]={};
static void require(bool ok,const char* why) {
    if(!ok) throw std::runtime_error(why);
}
struct EthernetClient {
    int sock=-1;
    int timeoutMs=1000;
    EthernetClient()=default;
    explicit EthernetClient(int id):sock(id){}
    explicit operator bool() const {return sock>=0;}
    void cancelConnectAsync() {
        if(sock>=0){sockets[sock]=false;++closes[sock];sock=-1;}
    }
    void setConnectionTimeout(int ms) {timeoutMs=ms;}
    void stop() {cancelConnectAsync();}
};
struct AsyncTx {void reset(){++resets;}int resets=0;};
class JWPLC_ModbusTCPClientClass {
public:
    EthernetClient _tcpClient;
    AsyncTx _asyncTx;
    bool _sessionConnected=false,_connectStarted=false,_sendStarted=false;
    bool allowBus=false;
    int tries=0,releases=0,rxResets=0;
    bool acquireBus(int) {++tries;return allowBus;}
    void releaseBus(){++releases;}
    void resetRx(){++rxResets;}
    void closeSession();
};
enum {JWPLC_MODBUS_TCP_SERVER_LISTENING=1,
      JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET=2,
      JWPLC_MODBUS_TCP_SERVER_STOPPED=3};
struct EthernetRuntime {bool ready=true; bool isReady() const{return ready;}} JWPLC_Ethernet;
class JWPLC_ModbusTCPClass {
public:
    bool _clientActive=false,_serverEnabled=true;
    int _serverState=0;
    EthernetClient _client;
    bool allowBus=false;
    int tries=0,releases=0,rxResets=0,rxIntDisables=0;
    bool acquireBus(int) {++tries;return allowBus;}
    void releaseBus(){++releases;}
    void disableRxIntLocked(){++rxIntDisables;}
    void resetRx(){++rxResets;}
    void dropClient();
};
__SOURCE_CLIENT__
__SOURCE_SERVER__
static void resetSockets(){
    for(int i=0;i<8;i++){sockets[i]=false;closes[i]=0;}
}
int main(){
  try {
    {
      resetSockets();
      sockets[2]=true;
      JWPLC_ModbusTCPClientClass s;
      s._tcpClient=EthernetClient(2);
      s._sessionConnected=true;
      s.allowBus=false;
      s.closeSession();
      require(s.tries==1 && !s._tcpClient &&
              sockets[2] && closes[2]==0 && !s._sessionConnected,
              "CLIENT_DENIED_NOT_ORPHANED_AS_EXPECTED");
      std::cout<<"A13_006_CLIENT_LOCK_DENIED_SOCKET_ORPHAN=REPRODUCED\n";
    }
    {
      resetSockets();
      sockets[2]=true;
      JWPLC_ModbusTCPClientClass s;
      s._tcpClient=EthernetClient(2);
      s._sessionConnected=true;
      s.allowBus=true;
      s.closeSession();
      require(s.tries==1 && s.releases==1 && !s._tcpClient &&
              !sockets[2] && closes[2]==1,"CLIENT_GRANTED_CLOSURE");
      std::cout<<"A13_006_CLIENT_LOCK_GRANTED_CLOSURE=PASS\n";
    }
    {
      resetSockets();
      JWPLC_ModbusTCPClientClass s;
      s.closeSession();
      require(s.tries==0 && s.releases==0 &&
              !s._tcpClient,"CLIENT_EMPTY_CLIENT");
      std::cout<<"A13_006_CLIENT_NO_SOCKET_NO_LOCK=PASS\n";
    }
    {
      resetSockets();
      sockets[3]=true;
      JWPLC_ModbusTCPClass s;
      s._client=EthernetClient(3);
      s._clientActive=true;
      s.allowBus=false;
      s.dropClient();
      require(s.tries==1 && !s._clientActive && !s._client &&
              sockets[3] && closes[3]==0 && s.rxIntDisables==0,
              "SERVER_DENIED_NOT_ORPHANED_AS_EXPECTED");
      std::cout<<"A13_006_SERVER_LOCK_DENIED_SOCKET_ORPHAN=REPRODUCED\n";
    }
    {
      resetSockets();
      sockets[3]=true;
      JWPLC_ModbusTCPClass s;
      s._client=EthernetClient(3);
      s._clientActive=true;
      s.allowBus=true;
      s.dropClient();
      require(s.tries==1 && s.releases==1 && s.rxIntDisables==1 &&
              !s._clientActive && !s._client &&
              !sockets[3] && closes[3]==1,"SERVER_GRANTED_CLOSURE");
      std::cout<<"A13_006_SERVER_LOCK_GRANTED_CLOSURE=PASS\n";
    }
    {
      resetSockets();
      JWPLC_ModbusTCPClass s;
      s.dropClient();
      require(s.tries==0 && s.releases==0 && !s._clientActive,
              "SERVER_EMPTY_CLIENT");
      std::cout<<"A13_006_SERVER_NO_CLIENT_NO_LOCK=PASS\n";
    }
    std::cout<<"A13_006_RESULT=BASELINE_DEFECT_REPRODUCED_NOT_FIXED\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr<<"A13_006_FAILURE="<<e.what()<<"\n";
    return 4;
  }
}
'''

def say(key,value):
    print("G10_P1A_"+str(key)+"="+str(value),flush=True)

def need(ok,reason):
    if not ok: raise RuntimeError(reason)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""): h.update(chunk)
    return h.hexdigest()

def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],
                     cwd=ROOT,capture_output=True)
    need(p.returncode==0,"GIT_COMMAND_FAILED_"+repr(args))
    return p.stdout.decode("utf-8","replace").strip()

def extract(source,signature):
    start=source.find(signature)
    need(start>=0 and source.find(signature,start+1)<0,
         "SIGNATURE_MISSING_OR_DUPLICATED_"+signature)
    opening=source.find("{",start+len(signature))
    need(opening>=0,"OPEN_BRACE_MISSING")
    depth=0
    for i in range(opening,len(source)):
        if source[i]=="{":depth+=1
        elif source[i]=="}":
            depth-=1
            if depth==0: return source[start:i+1]
    raise RuntimeError("UNBALANCED_FUNCTION_"+signature)

def prepare_source():
    c=(ROOT/CLIENT).read_text(encoding="utf-8-sig")
    s=(ROOT/SERVER).read_text(encoding="utf-8-sig")
    cfun=extract(c,"void JWPLC_ModbusTCPClientClass::closeSession()")
    sfun=extract(s,"void JWPLC_ModbusTCPClass::dropClient()")
    need("_tcpClient = EthernetClient();" in cfun and
         "if ((bool)_tcpClient && acquireBus(20))" in cfun and
         "_tcpClient.cancelConnectAsync();" in cfun,"CLIENT_PATTERN_CHANGED")
    need("_client = EthernetClient();" in sfun and
         "if (_clientActive && acquireBus(20))" in sfun and
         "_client.stop();" in sfun,"SERVER_PATTERN_CHANGED")
    result=CPP_TEMPLATE.replace("__SOURCE_CLIENT__",cfun)
    result=result.replace("__SOURCE_SERVER__",sfun)
    need("void JWPLC_ModbusTCPClientClass::closeSession()" in result and
         "void JWPLC_ModbusTCPClass::dropClient()" in result,
         "METHOD_EXTRACT_FAILED")
    return result

def compiler():
    for name in ["clang++","g++","c++","cl"]:
        binary=shutil.which(name)
        if binary:return binary,("msvc" if name=="cl" else "gcc")
    return None,None

def execute_native(source,folder):
    command,kind=compiler()
    need(command is not None,
         "HOST_CPP_COMPILER_MISSING_NO_NATIVE_PROOF_DO_NOT_ASSUME_PASS")
    file=folder/"a13_006_source_backed.cpp"
    file.write_text(source,encoding="utf-8")
    exe=folder/("a13_006_repro.exe" if os.name=="nt" else "a13_006_repro")
    if kind=="msvc":
        compile_args=[command,"/nologo","/EHsc","/std:c++17",str(file),
                      "/Fe:"+str(exe)]
    else:
        compile_args=[command,"-std=c++17","-O0","-Wall","-Wextra","-o",
                      str(exe),str(file)]
    result=subprocess.run(compile_args,cwd=folder,capture_output=True,
                          text=True,errors="replace",timeout=90)
    (folder/"compiler.log").write_text(
        "COMMAND="+repr(compile_args)+"\nEXIT="+str(result.returncode)+
        "\n"+result.stdout+"\n"+result.stderr,encoding="utf-8")
    need(result.returncode==0 and exe.is_file(),
         "HOST_CPP_COMPILATION_FAILED_CHECK_COMPILER_LOG")
    run=subprocess.run([str(exe)],cwd=folder,capture_output=True,
                       text=True,errors="replace",timeout=10)
    output=run.stdout+"\n"+run.stderr
    (folder/"native_result.log").write_text(output,encoding="utf-8")
    need(run.returncode==0,"HOST_NATIVE_REPRO_FAILED")
    for marker in EXPECTED:
        need(marker in output,"NATIVE_MARKER_MISSING_"+marker)
    say("HOST_COMPILER",Path(command).name)
    say("NATIVE", "PASS_SOURCE_BACKED_SIX_SCENARIOS")
    return {"compiler":str(command),"source_sha256":sha(file),
            "markers":EXPECTED,"code":run.returncode}

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native",action="store_true",
                   help="run real closeSession/dropClient bodies with host C++ mocks")
    p.add_argument("--source-only",action="store_true",
                   help="validate source and provenance without a host compiler")
    p.add_argument("--ci-standalone",action="store_true",
                   help="CI only: prove HEAD ancestry and sources without local P0 evidence")
    args=p.parse_args()
    need(args.native!=args.source_only,"CHOOSE_EXACTLY_ONE_MODE")
    runid="g10_p1a_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    output=ROOT/"tools/alpha13/results"/runid
    output.mkdir(parents=True,exist_ok=False)
    report={"schema":"A13-G10-P1A-REPRO-1","run_id":runid,
            "status":"REVIEW_STOP","product_mutation":False,
            "git_mutation":False,"hardware_access":False,
            "source_cpp_sha256":{}}
    try:
        need(Path.cwd().resolve()==ROOT and
             Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
             "RUN_AT_ROOT")
        need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        head=git("rev-parse","HEAD")
        need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
             "HEAD_NOT_REMOTE_TRACKING")
        need(subprocess.run(["git","merge-base","--is-ancestor",G10_P0_HEAD,head],
                            cwd=ROOT,capture_output=True).returncode==0,
             "G10_P0_NOT_ANCESTOR")
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),
             "TRACKED_WORKTREE_DIRTY")
        if not args.ci_standalone:
            p0=ROOT/"tools/alpha13/results"/P0_ID/"MANIFEST.json"
            need(p0.is_file(),"G10_P0_LOCAL_MANIFEST_NOT_FOUND")
            previous=json.loads(p0.read_text(encoding="utf-8-sig"))
            need(previous.get("schema")=="A13-G10-P0-READONLY-1" and
                 previous.get("run_id")==P0_ID and
                 previous.get("status")=="BASELINE_CAPTURED_REVIEW_FINDINGS_OPEN" and
                 previous.get("head")==G10_P0_HEAD,
                 "P0_WRONG_PROVENANCE")
        else:
            previous=None
        for name in [CLIENT,SERVER]:
            current=sha(ROOT/name)
            if previous is not None:
                need(previous.get("sources",{}).get(name,{}).get("sha256")==current,
                     "G10_P0_SOURCE_SHA_CHANGED_"+name)
            report["source_cpp_sha256"][name]=current
        report["provenance"]=("CI_GIT_HISTORY_AND_CURRENT_SOURCES_ONLY"
                              if args.ci_standalone else "LOCAL_P0_MANIFEST_AND_SHA")
        report["head"]=head
        source=prepare_source()
        (output/"extracted_cpp.cpp").write_text(source,encoding="utf-8")
        say("SOURCE_EXTRACTION","PASS_EXACT_CURRENT_FUNCTION_BODIES")
        if args.native:
            report["native_proof"]=execute_native(source,output)
            report["status"]="NATIVE_BASELINE_DEFECT_REPRODUCED_NOT_FIXED"
        else:
            report["status"]="SOURCE_CONTRACT_PASS_NATIVE_NOT_RUN"
        need(not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),
             "PRODUCT_WORKTREE_MUTATED")
        say("STATUS",report["status"])
        say("RESULTS",output)
        return 0
    except Exception as exc:
        report["reason"]=repr(exc)
        say("STATUS","REVIEW_STOP")
        say("REASON",repr(exc))
        say("RESULTS",output)
        return 2
    finally:
        (output/"MANIFEST.json").write_text(
            json.dumps(report,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as exc:
        say("FATAL",repr(exc))
        sys.exit(2)

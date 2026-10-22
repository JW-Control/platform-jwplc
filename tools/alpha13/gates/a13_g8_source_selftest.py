#!/usr/bin/env python3
"""A13 G8: comprobar delta canónico y ejecutar C++ nativo extraído de fuentes reales.

NO altera producto: los fixtures ensamblan solamente métodos productivos
del candidato bajo mocks UART/RX. No demuestra todavía prueba RS-485 física.
"""
from __future__ import annotations
import argparse
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[3]
RTU=ROOT/"JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp"
RS=ROOT/"JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp"
CAND_RTU=ROOT/"tools/alpha13/candidates/g8/JWPLC_ModbusRTU.cpp"
CAND_RS=ROOT/"tools/alpha13/candidates/g8/JWPLC_RS485.cpp"
CORE_SER=ROOT/"JWPLC/2.1.0/cores/esp32/HardwareSerial.cpp"
CORE_H=ROOT/"JWPLC/2.1.0/cores/esp32/HardwareSerial.h"


def need(ok,why):
    if not ok: raise RuntimeError(why)


def subst(src,before,after):
    need(src.count(before)==1,"WRONG_DELTA_ANCHOR_COUNT")
    return src.replace(before,after)


def make_expected_rtu(source):
    source=subst(source,
        "            _stats.rxBytes++;\n            _stats.rxBytes++;\n            _rxBuffer[_rxLength++] = (uint8_t)value;",
        "            _rxBuffer[_rxLength++] = (uint8_t)value;")
    pos1=source.index("void JWPLC_ModbusRTUClass::pollServer()")
    pos2=source.index("void JWPLC_ModbusRTUClass::pollMaster()")
    need(pos1<pos2,"RTU_METHOD_ORDER")
    pre=source[:pos1]; slave=source[pos1:pos2]; master=source[pos2:]
    anchor=("            if (value < 0)\n"
            "            {\n"
            "                break;\n"
            "            }\n\n"
            "            if (_rxLength >= JWPLC_MODBUS_RTU_MAX_FRAME)")
    repl=("            if (value < 0)\n"
          "            {\n"
          "                break;\n"
          "            }\n\n"
          "            // Contar cada byte consumido, incluso el que desborda el buffer.\n"
          "            _stats.rxBytes++;\n"
          "            if (_rxLength >= JWPLC_MODBUS_RTU_MAX_FRAME)")
    return pre+subst(slave,anchor,repl)+subst(master,anchor,repl)


def make_expected_rs(source):
    return subst(source,
        "    _serial->begin(_baud, _config, _rxPin, _txPin);\n"
        "    _ready = true;\n"
        "    clearError();",
        "    _serial->begin(_baud, _config, _rxPin, _txPin);\n"
        "    // HardwareSerial::begin() retorna void. Su operator bool() comprueba\n"
        "    // uartIsDriverInstalled(); no declarar listo un UART que no inició.\n"
        "    if (!static_cast<bool>(*_serial))\n"
        "    {\n"
        "        _ready = false;\n"
        "        setError(JWPLC_RS485_UNKNOWN_ERROR);\n"
        "        return false;\n"
        "    }\n\n"
        "    _ready = true;\n"
        "    clearError();")


def contract():
    source_rtu=RTU.read_text(encoding="utf-8")
    source_rs=RS.read_text(encoding="utf-8")
    candidate_rtu=CAND_RTU.read_text(encoding="utf-8")
    candidate_rs=CAND_RS.read_text(encoding="utf-8")
    if source_rtu==candidate_rtu and source_rs==candidate_rs:
        print("G8_SOURCE_STATE=ADOPTED_EXACT_CANDIDATE",flush=True)
    else:
        need(candidate_rtu==make_expected_rtu(source_rtu),
             "RTU_CANDIDATE_NOT_EXACT_DELTA")
        need(candidate_rs==make_expected_rs(source_rs),
             "RS_CANDIDATE_NOT_EXACT_DELTA")
        print("G8_SOURCE_STATE=BASELINE_EXACT_DELTA",flush=True)
    core=CORE_SER.read_text(encoding="utf-8")
    header=CORE_H.read_text(encoding="utf-8")
    need("HardwareSerial::operator bool() const" in core and
         "return uartIsDriverInstalled(_uart);" in core and
         "operator bool() const;" in header and
         "_uart = uartBegin(" in core,
         "CORE_UART_DRIVER_VALIDATION_NOT_PROVEN")
    print("G8_SOURCE_CONTRACT=PASS",flush=True)
    print("G8_HARDWARESERIAL_BOOL_CORE_CONTRACT=PASS",flush=True)
    return candidate_rtu,candidate_rs


def rtu_rx_piece(source,method,end_marker):
    begin=source.index("void JWPLC_ModbusRTUClass::"+method+"()")
    end=source.index(end_marker,begin)
    body=source[begin:end]
    piece_start=body.index("    if (_bulkRxEnabled)")
    return body[piece_start:]


def rs_method(source,signature,next_sig):
    pos=source.index(signature)
    end=source.index(next_sig,pos+len(signature))
    return source[pos:end]


def rtu_fixture(source):
    server=rtu_rx_piece(source,"pollServer","    if (_rxLength == 0)")
    master=rtu_rx_piece(source,"pollMaster","    uint16_t expectedLength = 0;")
    need(server.count("_stats.rxBytes++")==1 and
         master.count("_stats.rxBytes++")==1 and
         server.count("_stats.rxBytes +=")==1 and
         master.count("_stats.rxBytes +=")==1,"COUNTER_PATH_COUNT")
    return (r"""
#include <cstdint>
#include <cstddef>
#include <vector>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#define JWPLC_MODBUS_RTU_MAX_FRAME 256
#define JWPLC_MODBUS_BUFFER_OVERFLOW 9
static uint32_t micros(){return 123456;}
struct FakeRS {
  std::vector<int> values;
  size_t pos=0;
  int available(){return (int)(values.size()-pos);}
  int read(){return pos<values.size()?values[pos++]:-1;}
  size_t readAvailable(uint8_t *p,size_t max) {
    size_t n=0;
    while(n<max && pos<values.size() && values[pos]>=0)
      p[n++]=(uint8_t)values[pos++];
    return n;
  }
} JWPLC_RS485;
struct Trial {
  bool _bulkRxEnabled=false, overflow=false;
  uint16_t _rxLength=0;
  uint8_t _rxBuffer[256]{};
  uint32_t _lastByteUs=0;
  struct {uint64_t rxBytes=0;} _stats;
  void clearRxBuffer(){_rxLength=0;}
  void setError(int){overflow=true;}
  void completeMasterTransaction(int){overflow=true;}
  void server(){
""" + server + r"""
  }
  void master(){
""" + master + r"""
  }
};
static void expect(const char *name, bool ok){
 if(!ok){std::fprintf(stderr,"FAILED_%s\n",name);std::exit(2);}
 std::printf("G8_NATIVE_CASE_%s=PASS\n",name);
}
static void scenario(const char *name,bool is_master,bool bulk,
                     std::vector<int> bytes,uint16_t initial,
                     uint64_t count,bool overflow,int remain) {
 JWPLC_RS485.values=bytes;JWPLC_RS485.pos=0;
 Trial t;t._bulkRxEnabled=bulk;t._rxLength=initial;
 if(is_master)t.master();else t.server();
 expect(name,t._stats.rxBytes==count&&t.overflow==overflow&&
        JWPLC_RS485.available()==remain);
}
int main(){
 scenario("SLAVE_NONBULK_3",false,false,{1,2,3},0,3,false,0);
 scenario("MASTER_NONBULK_3",true,false,{1,2,3},0,3,false,0);
 scenario("SLAVE_BULK_3",false,true,{1,2,3},0,3,false,0);
 scenario("MASTER_BULK_3",true,true,{1,2,3},0,3,false,0);
 scenario("SLAVE_NONBULK_ZERO",false,false,{},0,0,false,0);
 scenario("MASTER_BULK_ZERO",true,true,{},0,0,false,0);
 scenario("SLAVE_NONBULK_NEGATIVE",false,false,{-1},0,0,false,0);
 scenario("MASTER_NONBULK_NEGATIVE",true,false,{-1},0,0,false,0);
 scenario("SLAVE_NONBULK_OVERFLOW_COUNT",false,false,{1,2},255,2,true,0);
 scenario("MASTER_NONBULK_OVERFLOW_COUNT",true,false,{1,2},255,2,true,0);
 scenario("SLAVE_BULK_OVERFLOW",false,true,{1,2},255,1,true,1);
 scenario("MASTER_BULK_OVERFLOW",true,true,{1,2},255,1,true,1);
 std::puts("G8_NATIVE_RTU=PASS");
}
""")


def rs_fixture(source):
    begin=rs_method(source,
        "bool JWPLC_RS485Class::begin(uint32_t baud, uint32_t config)",
        "void JWPLC_RS485Class::end()")
    end=rs_method(source,
        "void JWPLC_RS485Class::end()",
        "bool JWPLC_RS485Class::isEnabled() const")
    need("static_cast<bool>(*_serial)" in begin,"RS_UART_OPERATOR_BOOL_MISSING")
    return (r"""
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#define JWPLC_HAS_RS485 1
#define JWPLC_RS485_AUTO_DIRECTION 1
#define JWPLC_RS485_TX_BUFFER_SIZE 128
#define JWPLC_RS485_FORCE_APB_CLOCK 1
#define UART_CLK_SRC_APB 1
#define JWPLC_RS485_NOT_STARTED 2
#define JWPLC_RS485_DISABLED 1
#define JWPLC_RS485_INVALID_SERIAL 3
#define JWPLC_RS485_UNKNOWN_ERROR 4
#define JWPLC_RS485_CLOCK_SOURCE_FAILED 5
#define JWPLC_RS485_OK 0
struct HardwareSerial {
 bool fail=false,fail_clock=false,installed=false;
 int begin_calls=0,end_calls=0;
 size_t setTxBufferSize(size_t n){return n;}
 bool setClockSource(int){return !fail_clock;}
 void begin(uint32_t,uint32_t,int8_t,int8_t){
   ++begin_calls;installed=!fail;
 }
 void end(){++end_calls;installed=false;}
 operator bool()const{return installed;}
};
class JWPLC_RS485Class{
public:
 HardwareSerial *_serial=nullptr;
 bool _ready=false;
 uint32_t _baud=0,_config=0;
 int8_t _rxPin=16,_txPin=17;
 size_t _txBufferSize=0;
 int _lastError=JWPLC_RS485_NOT_STARTED;
 uint32_t _lastActivityMs=0,_lastRxActivityMs=0,_lastTxActivityMs=0;
 explicit JWPLC_RS485Class(HardwareSerial* s):_serial(s){}
 void setError(int e){_lastError=e;}
 void clearError(){_lastError=JWPLC_RS485_OK;}
 bool begin(uint32_t baud,uint32_t config);
 void end();
};
""" + begin + end + r"""
static void expect(const char *name,bool ok){
 if(!ok){std::fprintf(stderr,"FAILED_%s\n",name);std::exit(2);}
 std::printf("G8_NATIVE_CASE_%s=PASS\n",name);
}
int main(){
 HardwareSerial serial;
 JWPLC_RS485Class r(&serial);
 expect("RS_BEGIN_OK",r.begin(115200,3)&&r._ready&&r._lastError==0);
 expect("RS_REPEAT_BEGIN_OK",r.begin(230400,4)&&r._ready&&r._baud==230400);
 r.end();
 expect("RS_END_CLEARS_READY",!r._ready&&r._lastError==JWPLC_RS485_NOT_STARTED);
 serial.fail=true;
 expect("RS_BEGIN_FAILURE_NOT_READY",!r.begin(19200,5)&&!r._ready&&
        r._lastError==JWPLC_RS485_UNKNOWN_ERROR);
 serial.fail=false;serial.fail_clock=true;
 expect("RS_CLOCK_FAILURE",!r.begin(9600,6)&&!r._ready&&
        r._lastError==JWPLC_RS485_CLOCK_SOURCE_FAILED);
 serial.fail_clock=false;
 expect("RS_RECOVERY_SUCCESS",r.begin(38400,7)&&r._ready&&
        r._lastError==JWPLC_RS485_OK);
 serial.fail=true;
 expect("RS_FAILURE_AFTER_SUCCESS",!r.begin(9600,8)&&!r._ready&&
        r._lastError==JWPLC_RS485_UNKNOWN_ERROR);
 JWPLC_RS485Class null_serial(nullptr);
 expect("RS_NULL_SERIAL",!null_serial.begin(19200,9)&&
        null_serial._lastError==JWPLC_RS485_INVALID_SERIAL);
 std::puts("G8_NATIVE_RS485=PASS");
}
""")


def run_native(src,compiler,name):
    with tempfile.TemporaryDirectory(prefix="jwplc_a13_g8_"+name+"_") as tmp:
        p=Path(tmp)
        source=p/(name+".cpp")
        exe=p/(name+".exe")
        source.write_text(src,encoding="utf-8")
        cmd=[compiler,"-std=c++17","-Wall","-Wextra","-pedantic",
             "-Werror",str(source),"-o",str(exe)]
        res=subprocess.run(cmd,capture_output=True,text=True)
        if res.returncode:
            print(res.stdout+res.stderr,flush=True)
        need(res.returncode==0,"NATIVE_COMPILE_FAILED_"+name)
        res=subprocess.run([str(exe)],capture_output=True,text=True)
        if res.returncode:
            print(res.stdout+res.stderr,flush=True)
        need(res.returncode==0,"NATIVE_RUNTIME_FAILED_"+name)
        print(res.stdout,end="",flush=True)
        return res.stdout


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native-cxx",default="")
    parser.add_argument("--require-native",action="store_true")
    args=parser.parse_args()
    rtu,rs=contract()
    if not args.native_cxx:
        need(not args.require_native,"NATIVE_COMPILER_REQUIRED")
        print("G8_NATIVE_CPP=NOT_AVAILABLE_PENDING",flush=True)
        return
    a=run_native(rtu_fixture(rtu),args.native_cxx,"rtu")
    b=run_native(rs_fixture(rs),args.native_cxx,"rs485")
    need(a.count("G8_NATIVE_CASE_")==12,"RTU_NATIVE_CASE_COUNT")
    need(b.count("G8_NATIVE_CASE_")==8,"RS_NATIVE_CASE_COUNT")
    print("G8_NATIVE_TOTAL_CASES=20",flush=True)
    print("G8_NATIVE_CPP=PASS_ACTUAL_EXTRACTED_BRANCHES",flush=True)


if __name__=="__main__":
    try:main()
    except Exception as exc:
        print("G8_SELFTEST=REVIEW_STOP",repr(exc),flush=True)
        raise

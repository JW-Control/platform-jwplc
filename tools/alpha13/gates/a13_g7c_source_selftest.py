#!/usr/bin/env python3
"""G7C: validate exact canonical delta and run extracted production C++ tests.

No hardware, SPI, git or product writes. Tests the actual candidate
JW_FRAM::readString() body against an injected read() failure/FRAM model.
A native host g++/clang++ is optional: absence is NOT reported as PASS.
"""
from __future__ import annotations
import argparse
import difflib
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
PROD=ROOT/"JWPLC/2.1.0/libraries/JW_FRAM/src"
CAND=ROOT/"tools/alpha13/candidates/g7c"

def need(ok,label):
    if not ok:
        raise RuntimeError(label)
    print("G7C_SOURCE_"+label+"=PASS",flush=True)

def unique_replace(s,a,b,label):
    need(s.count(a)==1,"UNIQUE_ANCHOR_"+label)
    return s.replace(a,b,1)

def expected_candidate():
    cpp=(PROD/"JW_FRAM.cpp").read_text(encoding="utf-8")
    hdr=(PROD/"JW_FRAM.h").read_text(encoding="utf-8")
    guards="  if (_spi_dev) {\n    delete _spi_dev;\n  }\n\n"
    need(cpp.count(guards)==2,"TWO_DEAD_GUARDS")
    cpp=cpp.replace(guards,"",2)
    cpp=unique_replace(cpp,
        "  const uint8_t len = read8(addr);\n"
        "  if (len > maxLen || len > MAX_TEXT_LENGTH) {",
        "  uint8_t len = 0;\n"
        "  // read8() devuelve 0 tanto para cadena vacia como para fallo SPI.\n"
        "  // read() conserva el resultado de error sin cambiar el formato en FRAM.\n"
        "  if (!read(addr, &len, sizeof(len))) {\n"
        '    debugPrint(F("JW_FRAM::readString() fallo al leer longitud"));\n'
        "    return false;\n"
        "  }\n\n"
        "  if (len > maxLen || len > MAX_TEXT_LENGTH) {",
        "LENGTH_WITH_RESULT")
    cpp=unique_replace(cpp,
        '  if (!isAddressValid(addr + 1, len)) {\n'
        '    debugPrint(F("JW_FRAM::readString() payload fuera de rango"));\n'
        "    return false;\n"
        "  }",
        "  // Una cadena vacia solo almacena su byte de longitud: tambien es\n"
        "  // valida si empieza en la ultima direccion fisica de la FRAM.\n"
        "  if (len > 0 && !isAddressValid(addr + 1, len)) {\n"
        '    debugPrint(F("JW_FRAM::readString() payload fuera de rango"));\n'
        "    return false;\n"
        "  }",
        "EMPTY_LAST_ADDRESS")
    cpp=unique_replace(cpp,
        "  value.reserve(len);\n"
        "  for (uint8_t i = 0; i < len; i++) {",
        "  // Longitud cero es un valor valido; no reservar para una cadena vacia.\n"
        "  if (len > 0 && !value.reserve(len)) {\n"
        '    debugPrint(F("JW_FRAM::readString() memoria insuficiente"));\n'
        "    return false;\n"
        "  }\n\n"
        "  for (uint8_t i = 0; i < len; i++) {",
        "RESERVE_CHECK")
    overloads=(
        "void JW_FRAM::debugPrint(const String &msg) {\n"
        "  if (_debugEnabled && _debugPort != nullptr) {\n"
        "    _debugPort->println(msg);\n"
        "  }\n"
        "}\n\n"
        "void JW_FRAM::debugPrint(const char *msg) {\n"
        "  if (_debugEnabled && _debugPort != nullptr) {\n"
        "    _debugPort->println(msg);\n"
        "  }\n"
        "}\n\n"
    )
    cpp=unique_replace(cpp,overloads,"","PRIVATE_OVERLOAD_DEFINITIONS")
    hdr=unique_replace(hdr,
        "  void debugPrint(const __FlashStringHelper *msg);\n"
        "  void debugPrint(const String &msg);\n"
        "  void debugPrint(const char *msg);\n"
        "  void debugPrintValue",
        "  void debugPrint(const __FlashStringHelper *msg);\n"
        "  void debugPrintValue",
        "PRIVATE_OVERLOAD_DECLARATIONS")
    return cpp,hdr

def extract_method(text):
    start=text.index("bool JW_FRAM::readString(")
    end=text.index("bool JW_FRAM::writeCString(",start)
    body=text[start:end].strip()
    need(body.endswith("}"),"READSTRING_EXTRACT")
    return body

def code_prefix():
    return r"""
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#define F(x) x
class String {
public:
  static bool fail_reserve;
  std::string data;
  String() = default;
  String(const char *v): data(v) {}
  String &operator=(const char *v) { data=v; return *this; }
  bool reserve(size_t n) { if (fail_reserve) return false; data.reserve(n); return true; }
  String &operator+=(char c) { data+=c; return *this; }
};
bool String::fail_reserve=false;
class JW_FRAM {
public:
  static constexpr uint8_t MAX_TEXT_LENGTH=127;
  std::array<uint8_t,8192> memory{};
  uint32_t bytes=8192;
  int reads=0;
  int fail_at_read=0;
  bool isAddressValid(uint32_t addr,size_t len) const {
    if (!bytes) return false;
    if (!len) return addr<bytes;
    if (addr>=bytes) return false;
    return static_cast<uint64_t>(addr)+len<=bytes;
  }
  bool read(uint32_t addr,uint8_t *out,size_t count) {
    reads++;
    if (fail_at_read==reads || !out || !isAddressValid(addr,count)) return false;
    std::memcpy(out,memory.data()+addr,count);
    return true;
  }
  void debugPrint(const char *) {}
  bool readString(uint32_t,String &,uint8_t);
};
"""

def code_suffix():
    return r"""
int failures=0;
void check(bool ok,const char *label) {
  std::cout<<"G7C_NATIVE_CASE_"<<label<<"="<<(ok?"PASS":"FAIL")<<"\n";
  if(!ok) ++failures;
}
int main() {
  JW_FRAM fram;
  String value("previous");
  fram.fail_at_read=1;
  check(!fram.readString(100,value) && value.data.empty() && fram.reads==1,
        "LENGTH_READ_SPI_FAILURE");
  fram.fail_at_read=0;fram.reads=0;
  fram.memory[100]=0;
  String::fail_reserve=true;
  check(fram.readString(100,value) && value.data.empty() && fram.reads==1,
        "VALID_EMPTY_NO_ALLOCATION");
  String::fail_reserve=false; fram.reads=0;
  fram.memory[100]=4;
  std::memcpy(fram.memory.data()+101,"ABCD",4);
  check(fram.readString(100,value) && value.data=="ABCD" && fram.reads==2,
        "VALID_PAYLOAD");
  fram.reads=0;
  check(!fram.readString(100,value,3) && value.data.empty() && fram.reads==1,
        "MAXLEN_REJECTION");
  fram.reads=0;fram.memory[100]=128;
  check(!fram.readString(100,value) && value.data.empty(),
        "INTERNAL_MAX_REJECTION");
  fram.reads=0;fram.memory[8190]=3;
  check(!fram.readString(8190,value) && value.data.empty() && fram.reads==1,
        "PAYLOAD_TRUNCATED_BOUNDARY");
  fram.memory[100]=4;fram.reads=0;fram.fail_at_read=2;
  check(!fram.readString(100,value) && value.data.empty() && fram.reads==2,
        "PAYLOAD_SPI_FAILURE");
  fram.fail_at_read=0;fram.reads=0;String::fail_reserve=true;
  check(!fram.readString(100,value) && value.data.empty() && fram.reads==2,
        "RESERVE_FAILURE");
  String::fail_reserve=false; fram.reads=0;fram.memory[8191]=0;
  check(fram.readString(8191,value) && value.data.empty() && fram.reads==1,
        "EMPTY_LAST_FRAM_ADDRESS");
  fram.reads=0;fram.memory[8064]=127;
  for(int i=0;i<127;++i)fram.memory[8065+i]=static_cast<uint8_t>('a');
  check(fram.readString(8064,value,255) && value.data.size()==127 &&
        fram.reads==2,"MAX_127_EXACT_END");
  fram.reads=0;
  check(!fram.readString(8192,value) && value.data.empty() && fram.reads==0,
        "HEADER_OUTSIDE_FRAM");
  std::cout<<"G7C_NATIVE_TESTS="<<(failures==0?"PASS":"FAIL")<<"\n";
  return failures==0?0:2;
}
"""

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--native-cxx",default="",help="Path to native g++ or clang++ (optional)")
    p.add_argument("--require-native",action="store_true")
    args=p.parse_args()
    cpp,hdr=expected_candidate()
    actual_cpp=(CAND/"JW_FRAM.cpp").read_text(encoding="utf-8")
    actual_hdr=(CAND/"JW_FRAM.h").read_text(encoding="utf-8")
    need(actual_cpp==cpp,"CPP_EXACT_ALLOWED_DIFF_ONLY")
    need(actual_hdr==hdr,"HEADER_EXACT_ALLOWED_DIFF_ONLY")
    need("read8(addr)" not in extract_method(cpp).replace(
         "// read8() devuelve 0 tanto para cadena vacia como para fallo SPI.",""),
         "READSTRING_NO_AMBIGUOUS_READ8")
    need('if (!read(addr, &len, sizeof(len)))' in cpp,"LENGTH_FAILURE_PROPAGATED")
    need("len > 0 && !isAddressValid(addr + 1, len)" in cpp,
         "VALID_EMPTY_AT_BOUNDARY")
    need("len > 0 && !value.reserve(len)" in cpp,"RESERVE_FAILURE_HANDLED")
    need("void JW_FRAM::debugPrint(const __FlashStringHelper *msg)" in cpp and
         "void debugPrint(const __FlashStringHelper *msg);" in hdr,
         "ACTIVE_DEBUG_HELPER_PRESERVED")
    need("bool readString(uint32_t addr, String &value, uint8_t maxLen" in hdr,
         "PUBLIC_API_PRESERVED")
    print("G7C_SOURCE_CONTRACT=PASS",flush=True)
    cxx=args.native_cxx or shutil.which("g++") or shutil.which("clang++")
    if not cxx:
        print("G7C_NATIVE_CPP=NOT_AVAILABLE_NOT_A_PASS",flush=True)
        if args.require_native:
            return 3
        return 0
    with tempfile.TemporaryDirectory(prefix="jwplc_g7c_native_") as tmp:
        src=Path(tmp)/"test_readstring.cpp"
        exe=Path(tmp)/("test_readstring.exe" if sys.platform=="win32" else "test_readstring")
        src.write_text(code_prefix()+"\n"+extract_method(actual_cpp)+"\n"+code_suffix(),
                       encoding="utf-8")
        compile=subprocess.run([cxx,"-std=c++17","-Wall","-Wextra","-Werror",
                                "-pedantic",str(src),"-o",str(exe)],
                               capture_output=True,text=True,timeout=90)
        print("G7C_NATIVE_COMPILE_EXIT="+str(compile.returncode),flush=True)
        if compile.returncode:
            print((compile.stdout+"\n"+compile.stderr)[-3500:],flush=True)
            return 4
        process=subprocess.run([str(exe)],capture_output=True,text=True,timeout=20)
        print(process.stdout,flush=True)
        if process.stderr:print(process.stderr[-1500:],flush=True)
        if process.returncode:
            return 5
    print("G7C_NATIVE_CPP=PASS_ACTUAL_EXTRACTED_METHOD",flush=True)
    return 0

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as e:
        print("G7C_SOURCE_CONTRACT=FAIL",flush=True)
        print("G7C_SOURCE_ERROR="+repr(e),flush=True)
        sys.exit(2)

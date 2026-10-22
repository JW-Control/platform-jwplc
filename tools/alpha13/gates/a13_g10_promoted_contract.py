#!/usr/bin/env python3
"""Contrato read-only de promoción G10 funcional, sin instrumentación de laboratorio."""
from pathlib import Path
import hashlib
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[3]
BASE="0344ff9cbe42e9efa65e7d4caabb60ce15464270"
E="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
M="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/"
MODIFIED={
E+"EthernetClient.cpp",E+"EthernetServer.cpp",E+"EthernetUdp.cpp",
E+"JWPLC_Ethernet.cpp",E+"JWPLC_W5x00_Ethernet.h",E+"socket.cpp",
M+"JWPLC_ModbusTCP.cpp",M+"JWPLC_ModbusTCP.h",
M+"JWPLC_ModbusTCP_Client.cpp",M+"JWPLC_ModbusTCP_Client.h"}
ADDED={E+"JWPLC_W5500_SafeAccess.h",E+"JWPLC_W5500_SafeAccess.cpp"}
EXPECTED=MODIFIED|ADDED
def run(*args):
 p=subprocess.run(["git","-C",str(ROOT),*args],capture_output=True)
 if p.returncode:raise RuntimeError("G10_N0_GIT_"+repr(args)+":"+p.stderr.decode(errors="replace")[-200:])
 return p.stdout
def require(ok,why):
 if not ok:raise RuntimeError("G10_N0_REVIEW_STOP_"+why)
def canonical(path):
 return path.read_bytes().replace(b"\r\n",b"\n")
def verify():
 run("merge-base","--is-ancestor",BASE,"HEAD")
 tracked=run("diff","--name-only",BASE,"HEAD","--","JWPLC/").decode().splitlines()
 require(set(tracked)==EXPECTED,"PRODUCT_DIFF_"+repr(sorted(set(tracked)^EXPECTED)))
 added=set(run("diff","--name-only","--diff-filter=A",BASE,"HEAD","--","JWPLC/").decode().splitlines())
 require(added==ADDED,"NEW_SOURCES_DIFF")
 for rel in EXPECTED:
  require((ROOT/rel).is_file(),"MISSING_"+rel)
  blob=run("show","HEAD:"+rel)
  require(canonical(ROOT/rel)==blob,"CHECKOUT_CONTENT_MISMATCH_"+rel)
 for rel in [E+"utility/w5100.cpp",E+"Dns.cpp",E+"Dhcp.cpp",
             "JWPLC/2.1.0/boards.txt","JWPLC/2.1.0/platform.txt"]:
  require(canonical(ROOT/rel)==run("show",BASE+":"+rel),"PROTECTED_CHANGED_"+rel)
 # Byte-for-byte canónicos (hashes Git) de todos los archives ya congelados.
 archives=[p for p in (ROOT/"JWPLC/2.1.0").rglob("*.a") if p.is_file()]
 require(len(archives)>=5,"ARCHIVES_NOT_FOUND")
 for p in archives:
  rel=p.relative_to(ROOT).as_posix()
  require(p.read_bytes()==run("show",BASE+":"+rel),"ARCHIVE_CHANGED_"+rel)
 # APIs y servicio de autoload siguen existiendo; no confundir con N2.
 runtime=(ROOT/(E+"JWPLC_Ethernet.cpp")).read_text(encoding="utf-8")
 require("void JWPLC_EthernetClass::service()" in runtime,"SERVICE_MISSING")
 for p in [E+"JWPLC_W5500_SafeAccess.h",E+"JWPLC_W5500_SafeAccess.cpp"]:
  require("jwplc_w5500_candidate" in (ROOT/p).read_text(encoding="utf-8"),"SAFEACCESS_MISSING")
 print("G10_PROMOTED_N0=PASS_10_MODIFIED_2_ADDED")
 print("G10_PROMOTED_F1=PASS_UNMODIFIED")
 print("G10_PROMOTED_DNS_DHCP=PASS_UNMODIFIED")
 print("G10_PROMOTED_FROZEN_ARCHIVES=PASS_"+str(len(archives)))
 print("G10_PROMOTED_AUTOLOAD_SERVICE=SOURCE_PRESENT")
 print("G10_PROMOTED_NO_HARDWARE=YES")
if __name__=="__main__":
 try:verify()
 except (RuntimeError,OSError,UnicodeError) as ex:
  print(str(ex),file=sys.stderr);sys.exit(2)

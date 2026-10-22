#!/usr/bin/env python3
"""Alpha13 G9-P5: smoke fisico SD lectura, RTC cache y botonera, SOLO COM4.

Preflight y build sin cambios de producto. Antes del upload a COM4 exige
confirmacion de overwrite; COM14 no se abre ni se programa. Reportes serial
con token de sesion, 1 boton PRESSED/RELEASED y 3 heartbeats posteriores.
No restauracion automatica del firmware original de COM4.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time
import uuid

ROOT=Path(__file__).resolve().parents[3]
BRANCH="v2.1.0-alpha.13/feature/cleanup-robustness"
PRODUCT_COMMIT="63e0291a7b1480af846e955559ab175aa78e7300"
CLI_SHA="1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571"
FQBN="jwplc_local:esp32:jwplcbasic"
PORT="COM4"
PORT_VID=0x1A86
PORT_PID=0x7523
BAUD=115200
CONFIRM="G9 FLASH COM4 SD RTC BUTTONS"
LIBS=ROOT/"JWPLC/2.1.0/libraries"
SD=LIBS/"JW_SD"
GLOBAL=LIBS/"JWPLC_GlobalPeripherals"
ARCHIVE=SD/"src/esp32/libJW_SD.a"
ARCHIVE_SHA="58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e"
FROZEN={
 ARCHIVE:ARCHIVE_SHA,
 ROOT/"JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a":
     "a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d",
 LIBS/"JWPLC_TFT/src/esp32/libJWPLC_TFT.a":
     "4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc",
 LIBS/"JW_FRAM/src/esp32/libJW_FRAM.a":
     "df390f3379da13c6d4503a9fb83bc098f2944590fe3ad083348e95bfdcaf7299",
 LIBS/"JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a":
     "f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416",
}
SKETCH=ROOT/"tools/alpha13/sketches/g9_p5_sd_rtc_buttons"
FIELDS={"SD_PRESENT","SD_READY","SD_ROOT","RTC_PRESENT","RTC_VALID",
        "RTC_LOST","BTN_PRESS","BTN_RELEASE","BTN_ID"}
REPORT_RE=re.compile(r"^G9_P5_RESULT:([0-9a-f]{24}) (.*)$")
EVENT_RE=re.compile(r"^G9_P5_(PRESS|RELEASE):([0-9a-f]{24}) ID=([0-5])$")
PHASE={}
M={}
OUT=None
TEMP=None

def emit(k,v):
    print("G9_P5_"+str(k)+"="+str(v),flush=True)

def need(ok,reason):
    if not ok: raise RuntimeError(reason)

def sha(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for part in iter(lambda:f.read(1024*1024),b""):h.update(part)
    return h.hexdigest()

def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True)
    need(p.returncode==0,"GIT_COMMAND_FAIL_"+repr(args)+":"+
         p.stderr.decode("utf-8","replace")[-350:])
    return p.stdout.decode("utf-8","replace").strip()

def save():
    if OUT is not None:
        (OUT/"MANIFEST.json").write_text(
            json.dumps(M,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
        (OUT/"SUMMARY.log").write_text(
            "\n".join("G9_P5_"+k.upper()+"="+str(v) for k,v in M.items()
                      if not isinstance(v,dict))+"\n",encoding="utf-8")

def phase(name,status):
    M.setdefault("phases",{})[name]=status
    emit("PHASE",name+"_"+status)
    save()

def command(args,label):
    argv=[str(x) for x in args]
    p=subprocess.run(argv,cwd=ROOT,stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT,encoding="utf-8",errors="replace")
    txt=p.stdout or ""
    (OUT/(label+".log")).write_text(
        "COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+txt,
        encoding="utf-8")
    emit(label+"_EXIT",p.returncode)
    if p.returncode:
        lines=[s for s in txt.splitlines()
               if re.search(r"error:|fatal error|undefined reference|cannot|no such file",s,re.I)]
        for line in lines[-8:]:emit("DIAGNOSTIC",line[:260])
    need(p.returncode==0,label+"_FAILED_SEE_LOG")
    return txt

def selected(log,name,folder):
    paths=re.findall(r"(?m)^Using library "+re.escape(name)+
                     r" at version .+ in folder: (.+)$",log)
    need(len(paths)==1 and Path(paths[0].strip()).resolve()==folder.resolve(),
         "WRONG_LIBRARY_SELECTED_"+name)

def verify_archive(log):
    lines=[s.replace("\\\\","\\") for s in log.splitlines()
           if "-lJW_SD" in s and ".elf" in s]
    need(len(lines)==1,"SD_ARCHIVE_LINK_LINE_NOT_UNIQUE")
    text=lines[0]
    need(re.search(r"""(?<!\S)["']?-lJW_SD(?=\s|["']|$)""",text),
         "SD_ARCHIVE_LINK_FLAG_MISSING")
    dirs=re.findall(r"""(?<!\S)["']?-L["']?([A-Za-z]:[\\/][^\s"']+)""",text)
    need(ARCHIVE.parent.resolve() in [Path(x).resolve() for x in dirs],
         "SD_ARCHIVE_LINK_PATH_WRONG")
    need(sha(ARCHIVE)==ARCHIVE_SHA,"SD_ARCHIVE_SHA_WRONG_AFTER_LINK")
    emit("SD_ARCHIVE_LINK","PASS_EXACT_PATH_FLAG_SHA")

def get_port():
    from serial.tools import list_ports
    p=[x for x in list_ports.comports() if x.device.upper()==PORT]
    need(len(p)==1,"COM4_NOT_VISIBLE_OR_AMBIGUOUS")
    x=p[0]
    need(x.vid==PORT_VID and x.pid==PORT_PID,
         "COM4_NOT_EXPECTED_CH340_USB_DEVICE")
    return {"port":x.device,"vid":x.vid,"pid":x.pid,
            "description":x.description,"serial_number":x.serial_number}

def preflight(cli):
    phase("PREFLIGHT","RUNNING")
    need(ROOT==Path.cwd().resolve() and
         ROOT==Path(git("rev-parse","--show-toplevel")).resolve(),
         "WRONG_REPO_ROOT")
    need(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
    head=git("rev-parse","HEAD")
    need(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
         "REMOTE_TRACKING_NOT_EQUAL_HEAD")
    need(subprocess.run(["git","merge-base","--is-ancestor",
                         PRODUCT_COMMIT,head],cwd=ROOT,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL).returncode==0,
         "G9_PRODUCT_COMMIT_NOT_IN_HISTORY")
    need(not git("diff","--name-only") and
         not git("diff","--cached","--name-only") and
         not git("ls-files","--unmerged"),
         "TRACKED_WORKTREE_OR_INDEX_NOT_CLEAN")
    need(cli.is_absolute() and cli.is_file() and
         cli.name.lower()=="arduino-cli.exe" and
         sha(cli)==CLI_SHA,"CLI_NOT_PINNED_151")
    version=command([cli,"version"],"CLI_VERSION")
    need("Version: 1.5.1" in version,"CLI_NOT_151")
    installed=command([cli,"core","list"],"CLI_CORES")
    need(re.search(r"(?m)^\s*jwplc_local:esp32\s+2\.1\.0-dev(?:\s|$)",
                   installed),"JWPLC_LOCAL_CORE_MISSING")
    for file,expected in FROZEN.items():
        need(file.is_file() and sha(file)==expected,
             "PACKAGE_ARCHIVE_CHANGED_"+file.name)
    source=(SD/"src/JW_SD.cpp").read_bytes()
    need(b"\x00" not in source and b"_path[0] == '\\0'" in source,
         "SD_SOURCE_G9_NOT_COMMITTED")
    global_source=(GLOBAL/"src/JWPLC_GlobalPeripherals.cpp").read_text(
        encoding="utf-8")
    need("static constexpr uint32_t g_buttonRefreshThrottleMs = 25;" in global_source and
         "g_rtcEnabled" not in global_source and
         "if (rtc == nullptr)" in global_source,
         "GLOBAL_PRODUCT_NOT_G9")
    need(b"precompiled=full" in (SD/"library.properties").read_bytes(),
         "JW_SD_NOT_PRECOMPILED_FULL")
    need((SKETCH/(SKETCH.name+".ino")).is_file(),"SMOKE_SKETCH_MISSING")
    try:
        import serial
        M["pyserial_version"]=getattr(serial,"VERSION","unknown")
    except ImportError as exc:
        raise RuntimeError("PYSERIAL_NOT_INSTALLED") from exc
    p=get_port()
    M["usb_port"]=p
    M["git_head"]=head
    M["archive_sha256"]=ARCHIVE_SHA
    M["initial_git_clean"]=True
    emit("PORT",PORT+"_CH340_CONFIRMED")
    phase("PREFLIGHT","PASS")

def build(cli):
    phase("BUILD","RUNNING")
    buildpath=TEMP/"build"
    log=command([cli,"compile","--fqbn",FQBN,"-j","0","-v",
                 "--clean","--build-path",buildpath,
                 "--libraries",LIBS,SKETCH],"COMPILE")
    selected(log,"JW_SD",SD)
    selected(log,"JWPLC_GlobalPeripherals",GLOBAL)
    need(not list(buildpath.rglob("JW_SD.cpp.o")),
         "JW_SD_COMPILED_FROM_SOURCE_INSTEAD_ARCHIVE")
    need(len(list(buildpath.rglob("JWPLC_GlobalPeripherals.cpp.o")))==1,
         "GLOBALPERIPHERALS_CPP_SOURCE_OBJECT_NOT_SELECTED")
    need(re.search(r"[/\\]precompiled[/\\]core[/\\]JWPLCBASIC[/\\]core\.a",
                   log),"PRECOMPILED_CORE_NOT_SELECTED")
    verify_archive(log)
    M["build_path"]=str(buildpath)
    phase("BUILD","PASS")

def authorize():
    phase("AUTHORIZATION","RUNNING")
    print("\nATENCION: upload SOLO a COM4. El sketch actual se sobrescribira.\n"
          "NO existe restauracion automatica del firmware anterior.\n"
          "Usar JWPLC COM4 en banco sin actuadores ni cargas de salida,\n"
          "con una microSD instalada y sin datos que deban manipularse.\n"
          "El test solo abre el directorio raiz de SD con FILE_READ;\n"
          "no crea ni borra archivos, ni ajusta RTC, ni acciona reles.\n"
          "Cerrar monitores Serial/IDE y confirmar que COM14 queda intacto.\n",flush=True)
    try:answer=input("Escribe exactamente '"+CONFIRM+"' para autorizar upload COM4: ")
    except EOFError:answer=""
    need(answer.strip()==CONFIRM,"UPLOAD_NOT_AUTHORIZED")
    M["flash_authorized"]=True
    phase("AUTHORIZATION","PASS")

def upload(cli):
    phase("UPLOAD","RUNNING")
    now=get_port()
    need(now["vid"]==M["usb_port"]["vid"] and
         now["pid"]==M["usb_port"]["pid"],"COM4_DEVICE_CHANGED")
    command([cli,"upload","--fqbn",FQBN,"--port",PORT,
             "--input-dir",M["build_path"]],"UPLOAD_COM4")
    M["upload_COM4"]="PASS"
    M["upload_COM14"]="NOT_ATTEMPTED"
    phase("UPLOAD","PASS")

def parse_fields(blob):
    values={}
    for pair in blob.split():
        need(pair.count("=")==1,"TELEMETRY_FIELD_MALFORMED")
        k,v=pair.split("=",1)
        need(k not in values and k in FIELDS and v.isdecimal(),
             "TELEMETRY_FIELD_UNKNOWN_OR_NONINTEGER_"+k)
        values[k]=int(v)
    need(set(values)==FIELDS,"TELEMETRY_REPORT_FIELD_SET_WRONG")
    return values

def check_report(fields,events):
    need(fields["SD_PRESENT"]==1,"SD_CARD_NOT_DETECTED")
    need(fields["SD_READY"]==1,"SD_NOT_INITIALIZED")
    need(fields["SD_ROOT"]==1,"SD_ROOT_READ_ONLY_OPEN_FAILED")
    need(fields["RTC_PRESENT"]==1,"RTC_NOT_PRESENT")
    need(fields["RTC_VALID"]==1,"RTC_REPORTED_INVALID")
    need(fields["RTC_LOST"] in (0,1),"RTC_LOST_FLAG_INVALID")
    need(fields["BTN_PRESS"]==1 and fields["BTN_RELEASE"]==1,
         "BUTTON_PRESS_RELEASE_INCOMPLETE")
    need(fields["BTN_ID"] in range(6),"BUTTON_ID_OUT_OF_RANGE")
    need(len(events)==2 and events[0]==("PRESS",fields["BTN_ID"]) and
         events[1]==("RELEASE",fields["BTN_ID"]),
         "BUTTON_EVENTS_NOT_SAME_PHYSICAL_KEY")
    return True

def serial_port():
    import serial
    serial_obj=serial.Serial(port=None,baudrate=BAUD,timeout=0.1,
                             write_timeout=1.0,rtscts=False,
                             dsrdtr=False,xonxoff=False)
    serial_obj.dtr=False
    serial_obj.rts=False
    serial_obj.port=PORT
    serial_obj.open()
    return serial_obj

def lines(dev,buffer):
    data=dev.read(dev.in_waiting or 1)
    if not data:return []
    buffer.extend(data)
    need(len(buffer)<8192,"SERIAL_BUFFER_OVERFLOW")
    found=[]
    while b"\n" in buffer:
        idx=buffer.index(b"\n")
        value=bytes(buffer[:idx]).strip(b"\r")
        del buffer[:idx+1]
        found.append(value.decode("utf-8","backslashreplace").strip())
    return found

def monitor():
    phase("PHYSICAL","RUNNING")
    token=uuid.uuid4().hex[:24]
    need(re.fullmatch(r"[0-9a-f]{24}",token),"BAD_TOKEN")
    M["token"]=token
    observed={"ready":False,"started":False,"sd":None,
              "rtc":None,"events":[],"report":None,"heartbeats":0}
    buf=bytearray()
    serial_log=OUT/"SERIAL_COM4.log"
    try:
        with serial_port() as port,serial_log.open("w",encoding="utf-8") as log:
            emit("SERIAL_DTR_RTS","FALSE_FALSE")
            ready_deadline=time.monotonic()+45
            while time.monotonic()<ready_deadline and not observed["ready"]:
                for item in lines(port,buf):
                    log.write(item+"\n");log.flush()
                    if item=="G9_P5_READY:COM4:SD_RTC_BUTTONS:READ_ONLY":
                        observed["ready"]=True
                        emit("READY","PASS_COM4")
                time.sleep(0.005)
            need(observed["ready"],"G9_FIRMWARE_READY_TIMEOUT_OR_WRONG_PROGRAM")
            phase("READY","PASS")
            port.write(("G9_P5_START:"+token+"\n").encode("ascii"))
            port.flush()
            emit("START","SENT_COM4_WITH_TOKEN")
            print("\nPulsa y SUELTA una tecla de la botonera del JWPLC COM4.\n"
                  "Tienes hasta 70 segundos. El script observara SD, RTC y\n"
                  "la pulsacion/liberacion real, sin escribir a la tarjeta.\n",flush=True)
            end=time.monotonic()+70
            while time.monotonic()<end and observed["report"] is None:
                for item in lines(port,buf):
                    if not item:continue
                    log.write(item+"\n");log.flush()
                    if item=="G9_P5_STARTED:"+token:
                        observed["started"]=True
                        emit("TOKEN_ACK","PASS")
                    ev=EVENT_RE.fullmatch(item)
                    if ev and ev.group(2)==token:
                        event=(ev.group(1),int(ev.group(3)))
                        observed["events"].append(event)
                        emit("BUTTON_"+event[0],event[1])
                    if item.startswith("G9_P5_SD:"+token+" "):
                        observed["sd"]=item
                        emit("SD_READ_ONLY_TELEMETRY",item)
                    if item.startswith("G9_P5_RTC:"+token+" "):
                        observed["rtc"]=item
                    m=REPORT_RE.fullmatch(item)
                    if m and m.group(1)==token:
                        observed["report"]=parse_fields(m.group(2))
                        emit("REPORT",json.dumps(observed["report"],sort_keys=True))
                time.sleep(0.005)
            need(observed["started"],"SESSION_START_NOT_ACKNOWLEDGED")
            need(observed["sd"] is not None and observed["rtc"] is not None,
                 "SD_OR_RTC_TELEMETRY_MISSING")
            need(observed["report"] is not None,"BUTTON_OR_FINAL_REPORT_TIMEOUT")
            check_report(observed["report"],observed["events"])
            emit("SD_ROOT_READ_ONLY","PASS")
            emit("RTC_PRESENT_VALID","PASS")
            emit("RTC_LOST_POWER_FLAG",observed["report"]["RTC_LOST"])
            emit("BUTTON_PRESS_RELEASE","PASS_ID_"+str(observed["report"]["BTN_ID"]))
            # Heartbeats post-test cubren solo estabilidad de ventana corta,
            # NO demuestran resolucion de F120 intermitente.
            heartbeat_deadline=time.monotonic()+9
            while time.monotonic()<heartbeat_deadline and observed["heartbeats"]<3:
                for item in lines(port,buf):
                    if not item:continue
                    log.write(item+"\n");log.flush()
                    if item=="G9_P5_HEARTBEAT:"+token:
                        observed["heartbeats"]+=1
                        emit("HEARTBEAT",observed["heartbeats"])
                time.sleep(0.005)
            need(observed["heartbeats"]>=3,"POST_TEST_HEARTBEAT_TOO_FEW")
            emit("HEARTBEATS_POST_TEST","PASS_3")
            M["telemetry"]=observed
            phase("PHYSICAL","PASS")
    finally:
        M["telemetry"]=observed
        save()

def main():
    global OUT,TEMP
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument("--arduino-cli",default=r"C:\Program Files\Arduino CLI\arduino-cli.exe")
    p.add_argument("--preflight-only",action="store_true")
    args=p.parse_args()
    runid="g9_p5_"+datetime.now().strftime("%Y%m%d_%H%M%S")+"_"+uuid.uuid4().hex[:8]
    OUT=ROOT/"tools/alpha13/results"/runid
    TEMP=Path(tempfile.gettempdir())/("jwplc_a13_"+runid)
    need(not OUT.exists() and not TEMP.exists(),"RUN_COLLISION")
    OUT.mkdir(parents=True)
    TEMP.mkdir(parents=True)
    M.update({
      "schema":"A13-G9-P5-PHYSICAL-COM4-1","run_id":runid,
      "status":"REVIEW_STOP","phases":{},"port":"COM4",
      "micro_sd_confirmed_inserted_by_user":True,
      "product_modified":False,"git_commit":False,"git_push":False,
      "upload_COM14":"NOT_ATTEMPTED","flash_authorized":False,
      "sd_write_operations":False,"fram_write_operations":False,
      "rtc_time_write":False,"relay_write":False,
      "original_firmware_restoration":"NOT_AUTOMATED",
      "f120_freeze_root_cause":"UNKNOWN_PENDING",
    })
    try:
        cli=Path(args.arduino_cli).expanduser().resolve()
        preflight(cli)
        build(cli)
        if args.preflight_only:
            M["status"]="PREFLIGHT_BUILD_PASS_NO_UPLOAD"
            emit("STATUS",M["status"])
            emit("RESULTS",OUT)
            return 0
        authorize()
        upload(cli)
        monitor()
        phase("FINAL_AUDIT","RUNNING")
        need(sha(ARCHIVE)==ARCHIVE_SHA and
             git("rev-parse","HEAD")==M["git_head"] and
             not git("diff","--name-only") and
             not git("diff","--cached","--name-only"),
             "PRODUCT_SHA_OR_GIT_CHANGED")
        phase("FINAL_AUDIT","PASS")
        M["status"]="PHYSICAL_PASS_COM4_SD_READONLY_RTC_BUTTONS"
        emit("STATUS",M["status"])
        emit("RESULTS",OUT)
        emit("COM14","NOT_TOUCHED")
        emit("RESTORE_FIRMWARE","NOT_AUTOMATED")
        return 0
    except Exception as exc:
        M["status"]="REVIEW_STOP"
        M["reason"]=repr(exc)
        emit("STATUS","REVIEW_STOP")
        emit("REASON",repr(exc))
        emit("RESULTS",OUT)
        if M.get("upload_COM4")=="PASS":
            emit("RESTORE_FIRMWARE","NOT_AUTOMATED_COM4_HAS_DIAGNOSTIC")
        return 2
    finally:
        save()

if __name__=="__main__":
    try:sys.exit(main())
    except Exception as exc:
        emit("FATAL",repr(exc))
        sys.exit(2)

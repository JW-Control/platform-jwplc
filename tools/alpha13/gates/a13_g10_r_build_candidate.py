#!/usr/bin/env python3
"""G10-R: compila COM4/COM14 desde overlay G10-I aislado, SIN PUERTOS NI UPLOAD."""
from __future__ import annotations

import argparse
from datetime import datetime
import os
import uuid
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
FQBN = "jwplc_hbench:esp32:jwplcbasic"
G10I = "a954cabd9c6e37876b59995a41ff0a449308ccd1"
SKETCH_COM4 = ROOT / "tools/alpha13/sketches/g10_r_integrated"
SKETCH_COM14 = ROOT / "tools/alpha13/sketches/g8_p4_slave"


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(value, reason: str) -> None:
    if not value:
        raise RuntimeError(reason)


def run(argv: list[str], log: Path | None = None) -> str:
    p = subprocess.run([str(x) for x in argv],cwd=ROOT,capture_output=True,
                       text=True,errors="replace",timeout=360)
    txt = (p.stdout or "") + "\n" + (p.stderr or "")
    if log is not None:
        log.write_text("COMMAND="+repr(argv)+"\nEXIT="+str(p.returncode)+"\n"+txt,
                       encoding="utf-8")
    require(p.returncode == 0, "CLI_COMMAND_FAILED:" + " ".join(map(str,argv[:4]))+
            ":"+str(p.returncode)+": "+txt[-900:])
    return txt


def guard_sketch() -> None:
    path = SKETCH_COM4 / "g10_r_integrated.ino"
    s = path.read_text(encoding="utf-8")
    for needle in (
        "G10R_BOOT:NONCE=", "G10R_STOPPED:",
        "G10R_PROTOCOL:R1R2R4_R3SNAPSHOT_V1",
        "G10R_READY:IP=", "G10R_HEARTBEAT:",
        "JWPLC_Ethernet.useDHCP();",
        "JWPLC_ModbusTCP.beginServer(1,502)",
        "JWPLC_ModbusRTU.requestReadHoldingRegisters(",
        "JWPLC_SD.open(\"/\", FILE_READ)",
        "JWPLC_FRAM.read8(0)",
        "JWPLC_Buttons.pressed(key)",
        "JWPLC_W5500_SafeAccess.h",
        "G10R_SOCKETS:", "G10R_SOCK_STATUS:",
        "G10R_SOCK_BEGIN:", "G10R_SOCK_END:",
        "LabSocketSnapshot", "labCaptureSockets(",
    ):
        require(needle in s, "R1_R2_R4_SKETCH_ANCHOR_MISSING_" + needle)
    for forbidden in (".write8(", "FILE_WRITE", "digitalWrite(", "analogWrite(",
                      "jwplcWriteQ(", "ethernetReset(", "ESP.restart(",
                      "esp_restart(", "gpio_set_level("):
        require(forbidden not in s,"R1_R2_R4_FORBIDDEN_API_"+forbidden)
    require(SKETCH_COM14.is_dir(),"SLAVE_SKETCH_MISSING")
    print("G10_R_SKETCH_GUARD=PASS_STATIC_NO_Q_NO_STORAGE_WRITE_NO_RESET")


def build(cli: str, *, upload: bool = False) -> None:
    require(Path.cwd().resolve() == ROOT, "RUN_FROM_REPO_ROOT")
    guard_sketch()
    if upload:
        require(os.name=="nt","G10R_UPLOAD_WINDOWS_ONLY")
        v=run([cli,"version"])
        require(re.search(r"\bVersion:\s*1\.5\.1\b",v) is not None,
                "G10R_UPLOAD_ARDUINO_CLI_1_5_1_REQUIRED")
        ref="refs/remotes/origin/codex/alpha13-g10-r-recovery-prep"
        current=run(["git","rev-parse","HEAD"]).strip()
        remote=run(["git","rev-parse",ref]).strip()
        require(current==remote,"G10R_UPLOAD_REMOTE_HEAD_MISMATCH")
    spec = importlib.util.spec_from_file_location(
        "g10r_integrator", ROOT/"tools/alpha13/gates/a13_g10_integrated.py")
    m = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = m
    assert spec.loader is not None
    spec.loader.exec_module(m)
    head = m.preflight(True)  # source-only, no hardware or git mutation
    # Sólo staging de laboratorio: se añade inspección read-only a las
    # mismas 14 fuentes, sin modificar G10-I ni JWPLC/2.1.0.
    labpath=ROOT/"tools/alpha13/gates/a13_g10_r_instrumentation.py"
    lab_spec=importlib.util.spec_from_file_location("g10_r_lab_snapshot",labpath)
    require(lab_spec is not None and lab_spec.loader is not None,"LAB_MODULE_MISSING")
    lab=importlib.util.module_from_spec(lab_spec)
    sys.modules[lab_spec.name]=lab
    lab_spec.loader.exec_module(lab)
    trace_path=ROOT/"tools/alpha13/gates/a13_g10_r_trace.py"
    trace_spec=importlib.util.spec_from_file_location("g10_r_event_trace",trace_path)
    require(trace_spec is not None and trace_spec.loader is not None,"TRACE_MODULE_MISSING")
    trace=importlib.util.module_from_spec(trace_spec)
    sys.modules[trace_spec.name]=trace
    trace_spec.loader.exec_module(trace)
    files = trace.compose_trace(lab.compose_lab(m.compose()))
    require("LabEventResult labReadEvents(" in files[trace.C],
            "R3_TRACE_NOT_LINKED")
    require("LabCaptureOutcome labCaptureSockets(" in files[lab.CPP],
            "R3_STAGE_INSTRUMENTATION_MISSING")
    hashes = {rel:{"sha256":hashlib.sha256(content.encode("utf-8")).hexdigest()}
              for rel,content in files.items()}
    require(len(files)==14,"EXPECTED_14_SOURCE_OVERLAYS")
    with tempfile.TemporaryDirectory(prefix="jwplc-g10r-build-") as scratch:
        tmp=Path(scratch)
        stage=tmp/"staged"
        m.stage_platform(stage,files,hashes)
        user=tmp/"sketchbook"
        platform=user/"hardware/jwplc_hbench/esp32"
        platform.parent.mkdir(parents=True)
        shutil.copytree(stage,platform)
        for relative in ("boards.txt","platform.txt",
                         "precompiled/core/JWPLCBASIC/core.a"):
            require(sha256(ROOT/"JWPLC/2.1.0"/relative)==sha256(platform/relative),
                    "PRODUCT_FROZEN_FILE_DRIFT_"+relative)
        require(m.load(m.MODULES["frozen"],"g10r_frozen").check_frozen()==5,
                "FROZEN_ARCHIVES_DRIFT")

        confdir=tmp/"arduino-config"
        confdir.mkdir()
        run([cli,"config","init","--dest-dir",str(confdir)])
        conf=confdir/"arduino-cli.yaml"
        run([cli,"--config-file",str(conf),"config","set",
             "directories.user",str(user)])
        output={"source_head":head,"g10_i_source_sha":G10I,
                "schema":"G10-R-AUTHORIZED-UPLOAD-V1",
                "gpio4_reset":"DISABLED","ports":["COM4","COM14"],
                "uploaded":{"COM4":"NOT_RUN","COM14":"NOT_RUN"},
                "elf_sha256":{},
                "sketch_sha256":{"COM4":sha256(SKETCH_COM4/"g10_r_integrated.ino"),
                                 "COM14":sha256(SKETCH_COM14/"g8_p4_slave.ino")},
                "r3_lab_snapshot":"READ_ONLY_INSTRUMENTED_NOT_FAULT_INJECTED",
                "r3_product_api_modified":False,
                "fqbn":FQBN,"overlay_source_count":len(files),
                "staged_core_sha256":sha256(platform/"precompiled/core/JWPLCBASIC/core.a"),
                "product_modified":False,"physical_test":"NOT_RUN",
                "upload":"NOT_RUN","builds":{}}
        for name,sketch in (("COM4",SKETCH_COM4),("COM14",SKETCH_COM14)):
            props=run([cli,"--config-file",str(conf),"compile",
                       "--fqbn",FQBN,"--show-properties",str(sketch)])
            values=[line.partition("=")[2].strip().strip('"')
                    for line in props.splitlines()
                    if line.partition("=")[0].strip()=="runtime.platform.path"]
            require(len(values)==1,"PROPERTIES_PLATFORM_UNIQUE_"+name)
            path=Path(values[0])
            require(path.is_dir() and path.samefile(platform),
                    "WRONG_PLATFORM_"+name)
            target=tmp/("build-"+name)
            logs=run([cli,"--config-file",str(conf),"compile",
                      "--fqbn",FQBN,"--warnings","default",
                      "--build-path",str(target),"--clean","-v",str(sketch)],
                      tmp/("COMPILE_"+name+".log"))
            if name=="COM4":
                for lib in ("JWPLC_Ethernet","JWPLC_ModbusTCP"):
                    lines=[line.split(" in folder: ",1)[1].strip().strip('"')
                           for line in logs.splitlines()
                           if line.startswith("Using library "+lib+" ") and
                              " in folder: " in line]
                    require(len(lines)==1 and Path(lines[0]).is_dir() and
                            Path(lines[0]).samefile(platform/"libraries"/lib),
                            "WRONG_LIBRARY_"+lib)
                objects=list(target.rglob("JWPLC_W5500_SafeAccess.cpp.o"))
                require(len(objects)==1,"SAFEACCESS_OBJECT_NOT_UNIQUE")
                dep=Path(str(objects[0])+".d")
                if not dep.exists():
                    dep=objects[0].with_suffix(".d")
                require(dep.is_file() and "JWPLC_W5500_SafeAccess.cpp" in
                        dep.read_text(encoding="utf-8",errors="replace"),
                        "SAFEACCESS_SOURCE_DEP_MISSING")
                output["safeaccess_object_sha256"]=sha256(objects[0])
            elfs=list(target.glob("*.elf"))
            require(len(elfs)==1,"ELF_NOT_UNIQUE_"+name)
            output["builds"][name]={"status":"PASS_SOURCE_FIRST",
                                    "elf_sha256":sha256(elfs[0])}
            output["elf_sha256"][name]=sha256(elfs[0])
            print("G10_R_BUILD_"+name+"=PASS_SOURCE_FIRST")
        print("G10_R_STAGE=PASS_14_SOURCES")
        print("G10_R_PLATFORM=PASS_SAME_DIRECTORY_ISOLATED")
        print("G10_R_PHYSICAL_TEST=NOT_RUN")
        print("G10_R_BUILD_MANIFEST="+json.dumps(output,sort_keys=True))
        if upload:
            upload_after_build(cli,conf,tmp,output)



def upload_after_build(cli: str, config: Path, tmp: Path,
                       evidence: dict) -> None:
    """Frontera física separada. Nada se sube antes del consentimiento."""
    from serial.tools import list_ports

    def verify_usb():
        result={}
        for name in ("COM4","COM14"):
            matches=[p for p in list_ports.comports()
                     if p.device.upper()==name]
            require(len(matches)==1,"G10R_USB_MISSING_"+name)
            port=matches[0]
            require((port.vid,port.pid)==(0x1a86,0x7523),
                    "G10R_USB_IDENTITY_MISMATCH_"+name)
            result[name]={"vid":port.vid,"pid":port.pid,
                          "description":port.description}
        return result

    evidence["port_metadata"]=verify_usb()
    require(evidence["source_head"]==
            run(["git","rev-parse","HEAD"]).strip(),
            "G10R_HEAD_CHANGED_AFTER_BUILD")
    print("G10_R_FLASH_NOTICE=BANCO_SIN_RELES_NI_CARGAS_Q")
    print("G10_R_FLASH_NOTICE=COM14_Slave_Y_COM4_Diagnostico_R3")
    print("G10_R_FLASH_NOTICE=FIRMWARE_ANTERIOR_NO_SE_RESTAURA")
    print("G10_R_FLASH_NOTICE=GPIO4_DISABLED_SD_FRAM_READ_ONLY")
    print("G10_R_FLASH_NOTICE=CIERRE_MONITORES_SERIALES_ANTES_UPLOAD")
    consent="G10R AUTORIZO FLASH COM4 COM14"
    try:answer=input("Escribe exactamente '"+consent+"': ").strip()
    except EOFError:answer=""
    require(answer==consent,"G10R_UPLOAD_NOT_AUTHORIZED")
    evidence["consent"]="EXPLICIT_AFTER_SOURCE_FIRST_BUILD"
    out=ROOT/"tools/alpha13/results"/(
         "g10_r_upload_"+datetime.now().strftime("%Y%m%d_%H%M%S")+
         "_"+uuid.uuid4().hex[:8])
    out.mkdir(parents=True,exist_ok=False)
    try:
        for port in ("COM14","COM4"):
            current=verify_usb()
            require(current[port]==evidence["port_metadata"][port],
                    "G10R_USB_CHANGED_BEFORE_UPLOAD_"+port)
            logs=run([cli,"--config-file",str(config),"upload",
                      "--fqbn",FQBN,"--port",port,
                      "--input-dir",str(tmp/("build-"+port))],
                      out/("UPLOAD_"+port+".log"))
            evidence["uploaded"][port]="PASS_CONFIRMED_UPLOAD"
            evidence["upload_log_"+port+"_sha256"]=sha256(
                out/("UPLOAD_"+port+".log"))
            (out/"MANIFEST.json").write_text(
                json.dumps(evidence,indent=2)+"\n",encoding="utf-8")
        evidence["physical_test"]="NOT_RUN_FIRMWARE_UPLOAD_ONLY"
        evidence["upload"]="PASS_BOTH_EXPLICIT"
        print("G10_R_UPLOAD_STATUS=PASS_BOTH_EXPLICIT")
    except Exception as exc:
        evidence["upload"]="REVIEW_STOP_PARTIAL_POSSIBLE"
        evidence["error"]=str(exc)
        print("G10_R_UPLOAD_STATUS=REVIEW_STOP_PARTIAL_POSSIBLE")
        raise
    finally:
        (out/"MANIFEST.json").write_text(
            json.dumps(evidence,indent=2)+"\n",encoding="utf-8")
        print("G10_R_UPLOAD_EVIDENCE_PATH="+str(out/"MANIFEST.json"))


def main() -> int:
    ap=argparse.ArgumentParser(description=__doc__)
    selection=ap.add_mutually_exclusive_group(required=True)
    selection.add_argument("--selftest",action="store_true")
    selection.add_argument("--build",action="store_true")
    selection.add_argument("--upload",action="store_true",
                           help="Requiere consentimiento humano y dos USB CH340; NO usar en CI")
    ap.add_argument("--cli",default="arduino-cli")
    args=ap.parse_args()
    try:
        if args.selftest:
            guard_sketch()
            print("G10_R_BUILD_SELFTEST=PASS_NO_CLI_NO_USB_NO_UPLOAD")
        else:
            build(args.cli,upload=args.upload)
        return 0
    except Exception as e:
        print("G10_R_REVIEW_STOP="+str(e))
        return 2


if __name__=="__main__":
    sys.exit(main())

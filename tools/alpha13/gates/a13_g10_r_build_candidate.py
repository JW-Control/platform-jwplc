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
# Fail-closed hasta completar R3 físico DNS/DHCP/TCP + gate integrado.
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY = False


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
        "G10R_PROTOCOL:R1R2R3R4_FAULT_LAB_V2",
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
        "G10R_EVENTS:", "G10R_EVT_HEAD:", "G10R_EVT_END:",
        "G10R_R3_UDP:", "G10R_R3_DNS:", "G10R_R3_TCP:",
        "#include <JWPLC_Ethernet_DNS.h>", "r3ProbeDns.cancelResolveAsync()",
        "r3ProbeTcp.cancelConnectAsync()", "r3ProbeTcp.beginConnectAsync(",
        "G10R_R3_DHCP_CANCEL:", "G10R_R3_DHCP_FINISH:",
        "G10RLabDhcpProbe::arm", "G10RLabDhcpProbe::finish",
        "G10R_R3_LEASE:", "G10R_R3_HOLD:",
        "r3HoldDurationUs", "HOLD_TIMING_REVIEW", "CLOSE_DURING_HOLD",
        "AWAIT_LIFECYCLE_REVIEW", "BusScope scope(0)",
        "vTaskDelay(pdMS_TO_TICKS(80))",
        "LabSocketSnapshot", "labCaptureSockets(",
    ):
        require(needle in s, "R1_R2_R4_SKETCH_ANCHOR_MISSING_" + needle)
    for forbidden in (".write8(", "FILE_WRITE", "digitalWrite(", "analogWrite(",
                      "jwplcWriteQ(", "ethernetReset(", "ESP.restart(",
                      "esp_restart(", "gpio_set_level("):
        require(forbidden not in s,"R1_R2_R4_FORBIDDEN_API_"+forbidden)
    require("r3ProbeDhcp.beginWithDHCP" not in s and
            "Ethernet.cancelDHCP(" not in s and
            "G10RLabDhcpProbe::arm" in s and
            "r3ProbeDhcp.cancelDHCP()" in s and
            "G10RLabDhcpProbe::finish" in s,
            "G10R_DHCP_LAB_MUST_NOT_EMIT_DHCP_DISCOVER_OR_TOUCH_MANAGER")
    product_header=ROOT/"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"
    require("friend class G10RLabDhcpProbe;" not in
            product_header.read_text(encoding="utf-8-sig"),
            "G10R_DHCP_LAB_FRIEND_LEAKED_INTO_PRODUCT")
    require("r3ProbeUdp.begin(15021)" in s and
            "r3ProbeUdp.stop()" in s and
            "r3HoldState.load(std::memory_order_acquire)" in s,
            "R3_UDP_BOUNDED_PROBE_GUARD_MISSING")
    require("g10rLabTcpRequest" in s and "G10R_TCP_EVENTS:" in s,
            "TCP124_SOURCE_CALLBACK_OR_EXPORT_MISSING")
    require(SKETCH_COM14.is_dir(),"SLAVE_SKETCH_MISSING")
    print("G10_R_SKETCH_GUARD=PASS_STATIC_NO_Q_NO_STORAGE_WRITE_NO_RESET")


def build(cli: str, *, upload: bool = False, stage_out: Path | None = None,
          bench_evidence: Path | None = None,
          dev_waiver: Path | None = None) -> None:
    waiver_data=None
    if upload:
        require((bench_evidence is None) != (dev_waiver is None),
                "H0_REQUIRE_EXACTLY_ONE_BENCH_BACKUP_OR_DEVELOPMENT_WAIVER")
        if dev_waiver is not None:
            # Excepcion del banco de desarrollo; NO habilita la campaña H.
            import a13_g10_r_h0_dev_waiver as h0
            waiver_data=json.loads(dev_waiver.read_text(encoding="utf-8-sig"))
            issues=h0.validate(waiver_data)
            require(not issues,"H0_DEV_WAIVER_REVIEW_STOP_"+",".join(issues))
            print("G10_R_H0_BACKUP=WAIVED_FOR_TWO_DEV_BOARDS_ONLY")
        else:
            import a13_g10_r_b4_bench as b4
            issues=b4.validate(json.loads(bench_evidence.read_text(encoding="utf-8-sig")))
            require(not issues,"B4_PREFLIGHT_REVIEW_STOP_"+",".join(issues))
            print("G10_R_B4_LOCAL_BACKUP_AND_BENCH=CHECKED_NOT_HARDWARE_ATTESTED")
            require(R3_COMPLETE_PHYSICAL_CAMPAIGN_READY,
                    "G10R_FLASH_BLOCKED_R3_FULL_CAMPAIGN_NOT_READY")
    require(Path.cwd().resolve() == ROOT, "RUN_FROM_REPO_ROOT")
    guard_sketch()
    if upload:
        require(os.name=="nt","G10R_UPLOAD_WINDOWS_ONLY")
        v=run([cli,"version"])
        require(re.search(r"\bVersion:\s*1\.5\.1\b",v) is not None,
                "G10R_UPLOAD_ARDUINO_CLI_1_5_1_REQUIRED")
        ref="refs/remotes/origin/codex/alpha13-preh-tcp124-review-20261016"
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
    # Hook TID+socket del servidor Modbus, SOLO overlay temporal de banco.
    tcp_path=ROOT/"tools/alpha13/gates/a13_g10_r_tcp124_stage.py"
    tcp_spec=importlib.util.spec_from_file_location("g10r_tcp124_stage",tcp_path)
    require(tcp_spec is not None and tcp_spec.loader is not None,
            "TCP124_STAGE_MODULE_MISSING")
    tcp=importlib.util.module_from_spec(tcp_spec)
    sys.modules[tcp_spec.name]=tcp
    tcp_spec.loader.exec_module(tcp)
    files=tcp.compose(files)
    # B2: mantenimiento DHCP real, observacional y sin cambio de flujo.
    b2_path=ROOT/"tools/alpha13/gates/a13_g10_r_b2_dhcp_stage.py"
    b2_spec=importlib.util.spec_from_file_location("g10r_b2_stage",b2_path)
    require(b2_spec is not None and b2_spec.loader is not None,
            "B2_DHCP_STAGE_MISSING")
    b2=importlib.util.module_from_spec(b2_spec)
    sys.modules[b2_spec.name]=b2
    b2_spec.loader.exec_module(b2)
    files=b2.compose(files)
    # SOLO staging G10-R: friend diagnóstica en DhcpClass para armar
    # el UDP interno en un puerto TEMPORAL (15023), SIN DHCP DISCOVER,
    # OFFER, REQUEST, renovación de lease ni tocar el manager global.
    dhcp_header=("JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
                 "JWPLC_W5x00_Ethernet.h")
    friend_anchor="class DhcpClass {\nprivate:"
    require(dhcp_header in files and
            files[dhcp_header].count(friend_anchor)==1,
            "G10R_DHCP_LAB_FRIEND_HEADER_DRIFT")
    files[dhcp_header]=files[dhcp_header].replace(
        friend_anchor,
        friend_anchor+"\n    friend class G10RLabDhcpProbe; "
        "// solo stage del bench, no API producto",1)
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
        if stage_out is not None:
            destination=stage_out.resolve()
            require(destination!=ROOT and ROOT not in destination.parents,
                    "B3_STAGE_MUST_BE_OUTSIDE_REPOSITORY")
            require(not destination.exists(),"B3_STAGE_TARGET_MUST_NOT_EXIST")
            destination.parent.mkdir(parents=True,exist_ok=True)
            shutil.copytree(stage,destination)
            require(sha256(ROOT/"JWPLC/2.1.0"/"boards.txt")==
                    sha256(destination/"boards.txt"),"B3_BOARDS_DRIFT")
            require(sha256(ROOT/"JWPLC/2.1.0"/"platform.txt")==
                    sha256(destination/"platform.txt"),"B3_PLATFORM_DRIFT")
            require(m.load(m.MODULES["frozen"],"g10r_b3_frozen").check_frozen()==5,
                    "B3_FROZEN_ARCHIVES_DRIFT")
            print("G10_R_B3_STAGE=PASS_14_SOURCE_OVERLAYS_NO_CLI")
            print("G10_R_B3_PRODUCT=UNCHANGED")
            print("G10_R_B3_PHYSICAL=NOT_RUN")
            return
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
                "backup_policy":("WAIVED_DEV_ONLY" if waiver_data else "FULL_FLASH_BACKUPS"),
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
                # Fuente real de Modbus TCP: sin ella el hook del candidato
                # podria quedar oculto tras un archive precompilado.
                modbus_objects=list(target.rglob("JWPLC_ModbusTCP.cpp.o"))
                require(len(modbus_objects)==1,"MODBUS_TCP_OBJECT_NOT_UNIQUE")
                modbus_dep=Path(str(modbus_objects[0])+".d")
                if not modbus_dep.exists():
                    modbus_dep=modbus_objects[0].with_suffix(".d")
                require(modbus_dep.is_file() and "JWPLC_ModbusTCP.cpp" in
                        modbus_dep.read_text(encoding="utf-8",errors="replace"),
                        "MODBUS_TCP_SOURCE_DEP_MISSING")
                output["modbus_tcp_object_sha256"]=sha256(modbus_objects[0])
                # B2: comprobar fuente DHCP real compilada, no archive antiguo.
                ethernet_objects=list(target.rglob("JWPLC_Ethernet.cpp.o"))
                require(len(ethernet_objects)==1,"ETHERNET_OBJECT_NOT_UNIQUE")
                ethernet_dep=Path(str(ethernet_objects[0])+".d")
                if not ethernet_dep.exists():
                    ethernet_dep=ethernet_objects[0].with_suffix(".d")
                require(ethernet_dep.is_file() and "JWPLC_Ethernet.cpp" in
                        ethernet_dep.read_text(encoding="utf-8",errors="replace"),
                        "ETHERNET_SOURCE_DEP_MISSING")
                output["ethernet_object_sha256"]=sha256(ethernet_objects[0])
                print("G10_R_B2_DHCP_SOURCE_TO_OBJECT=PASS")
                print("G10_R_TCP124_MODBUS_SOURCE_TO_OBJECT=PASS")
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
            upload_after_build(cli,conf,tmp,output,dev_waiver=waiver_data)



def upload_after_build(cli: str, config: Path, tmp: Path,
                       evidence: dict, *, dev_waiver: dict | None = None) -> None:
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
    if dev_waiver is not None:
        # Verificar la MAC y los 4MB del ESP32 en cada puerto ANTES de escribir.
        for name in ("COM4","COM14"):
            observed=run([sys.executable,"-m","esptool","--chip","esp32",
                          "--port",name,"--baud","115200","read_mac"])
            m=re.search(r"MAC:\s*([0-9a-f]{2}(?::[0-9a-f]{2}){5})",
                        observed,re.IGNORECASE)
            require(m is not None,"H0_ESPTOOL_MAC_NOT_READ_"+name)
            require(m.group(1).lower()==dev_waiver["device_roles"][name]["expected_mac"].lower(),
                    "H0_ESP32_MAC_MISMATCH_"+name)
            flash=run([sys.executable,"-m","esptool","--chip","esp32",
                       "--port",name,"--baud","115200","flash_id"])
            require(re.search(r"Detected flash size:\s*4MB\b",flash,re.IGNORECASE)
                    is not None,"H0_FLASH_SIZE_NOT_4MB_"+name)
        print("G10_R_H0_BOTH_MAC_AND_FLASH4MB=PASS_BEFORE_WRITE")
    require(evidence["source_head"]==
            run(["git","rev-parse","HEAD"]).strip(),
            "G10R_HEAD_CHANGED_AFTER_BUILD")
    print("G10_R_FLASH_NOTICE=BANCO_SIN_RELES_NI_CARGAS_Q")
    print("G10_R_FLASH_NOTICE=COM14_Slave_Y_COM4_Diagnostico_R3")
    print("G10_R_FLASH_NOTICE=FIRMWARE_ANTERIOR_NO_SE_RESTAURA")
    print("G10_R_FLASH_NOTICE=GPIO4_DISABLED_SD_FRAM_READ_ONLY")
    print("G10_R_FLASH_NOTICE=CIERRE_MONITORES_SERIALES_ANTES_UPLOAD")
    consent=("G10R AUTORIZO FLASH COM4 COM14 SIN BACKUP" if dev_waiver
             is not None else "G10R AUTORIZO FLASH COM4 COM14")
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
    selection.add_argument("--stage-only",action="store_true",
                           help="Exporta plataforma candidata a temp externa, sin CLI ni hardware")
    ap.add_argument("--stage-output",type=Path)
    ap.add_argument("--bench-evidence",type=Path,
                    help="Ficha local B4 con backups full flash antes de cualquier upload")
    ap.add_argument("--dev-waiver",type=Path,
                    help="Excepcion dos placas desarrollo sin backup con chequeo MAC/flash")
    selection.add_argument("--upload",action="store_true",
                           help="Requiere consentimiento humano y dos USB CH340; NO usar en CI")
    ap.add_argument("--cli",default="arduino-cli")
    args=ap.parse_args()
    try:
        if args.selftest:
            guard_sketch()
            require(R3_COMPLETE_PHYSICAL_CAMPAIGN_READY is False,
                    "G10R_SELFTEST_MUST_FAIL_CLOSED_FLASH")
            require("codex/alpha13-preh-tcp124-review-20261016" in
                    Path(__file__).read_text(encoding="utf-8"),
                    "B4_UPLOAD_REMOTE_SOURCE_BRANCH_DRIFT")
            print("G10_R_BUILD_SELFTEST=PASS_NO_CLI_NO_USB_NO_UPLOAD")
            print("G10_R_FLASH=BLOCKED_R3_FULL_CAMPAIGN_NOT_READY")
        elif args.stage_only:
            require(args.stage_output is not None,"B3_STAGE_OUTPUT_REQUIRED")
            build(args.cli,stage_out=args.stage_output)
        else:
            require(args.stage_output is None,"B3_STAGE_OUTPUT_ONLY_FOR_STAGE_ONLY")
            require(not args.upload or args.bench_evidence is not None or
                    args.dev_waiver is not None,"B4_BENCH_BACKUP_EVIDENCE_REQUIRED_OR_DEV_WAIVER")
            build(args.cli,upload=args.upload,bench_evidence=args.bench_evidence,
                  dev_waiver=args.dev_waiver)
        return 0
    except Exception as e:
        print("G10_R_REVIEW_STOP="+str(e))
        return 2


if __name__=="__main__":
    sys.exit(main())

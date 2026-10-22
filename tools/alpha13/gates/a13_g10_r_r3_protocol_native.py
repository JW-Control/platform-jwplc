#!/usr/bin/env python3
"""R3 G10-R: gate INTEGRADO N1 DNS/DHCP/UDP + SafeAccess R3 real.

Compila los cuerpos reales de 14 métodos de la composición G10-I con el
SafeAccess.cpp de la misma composición, más snapshots/traza read-only.
Simula un mutex SPI no recursivo en host: NO es evidencia física ni da
permiso para --live, upload o adopción productiva.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT=Path(__file__).resolve().parents[3]
NET="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b03_deferred_close_test.cpp"
POSITIVE=(
    "G10_I_B03_DNS_FINISH_LIFETIME=PASS",
    "G10_I_B03_DNS_CANCEL_IDLE_RETRY=PASS",
    "G10_I_B03_DHCP_FINISH_IDLE_RETRY=PASS",
    "G10_I_B03_DNS_TIMEOUT=PASS",
    "G10_I_B03_DNS_BUILD_ERROR=PASS",
    "G10_I_B03_DHCP_TIMEOUT_ACK_NAK_CANCEL=PASS",
    "G10_I_B03_DHCP_SYNC_REBEGIN=PASS",
    "G10_I_B03_STALE_RESET_OWNER=PASS",
    "G10_I_B03_DNS_DHCP_REBEGIN=PASS",
    "G10_I_B03_DHCP_RENEW_REBIND=PASS",
    "G10_I_B03_MULTICAST_BUSY_REUSE=PASS",
    "G10_I_B03_NATIVE=PASS_SOURCE_BACKED_DEFERRED_CLOSE",
)
SIGNATURES=(
    ("void EthernetUDP::stop()","EthernetUdp.cpp"),
    ("void DNSClient::finishResolveAsync(int result)","Dns.cpp"),
    ("void DNSClient::cancelResolveAsync()","Dns.cpp"),
    ("void DhcpClass::finishAsync()","Dhcp.cpp"),
    ("uint8_t EthernetUDP::begin(uint16_t port)","EthernetUdp.cpp"),
    ("int DNSClient::beginResolveAsync(","Dns.cpp"),
    ("int DhcpClass::beginWithDHCPAsync(","Dhcp.cpp"),
    ("bool DhcpClass::startLeaseMaintenance(uint8_t mode)","DhcpAsyncControl.cpp"),
    ("uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port)","EthernetUdp.cpp"),
    ("int DNSClient::pollResolveAsync()","Dns.cpp"),
    ("int DhcpClass::pollDHCP()","Dhcp.cpp"),
    ("int DhcpClass::request_DHCP_lease()","Dhcp.cpp"),
    ("int DhcpClass::beginWithDHCP(uint8_t *mac, unsigned long timeout, unsigned long responseTimeout)","Dhcp.cpp"),
    ("void DhcpClass::cancelDHCP()","DhcpAsyncControl.cpp"),
)


def require(ok,why):
    if not ok:raise RuntimeError(why)


def load(rel,name):
    path=ROOT/"tools/alpha13/gates"/rel
    spec=importlib.util.spec_from_file_location(name,path)
    require(spec is not None and spec.loader is not None,"R3_MODULE_MISSING_"+name)
    obj=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=obj
    spec.loader.exec_module(obj)
    return obj


def run_native():
    require(sys.platform.startswith("linux"),"R3_PROTOCOL_NATIVE_LINUX_ONLY")
    compiler=shutil.which("g++") or shutil.which("clang++")
    require(compiler is not None,"R3_NATIVE_COMPILER_MISSING")
    integrator=load("a13_g10_integrated.py","r3i_integrator")
    lab=load("a13_g10_r_instrumentation.py","r3i_lab")
    trace=load("a13_g10_r_trace.py","r3i_trace")
    repro=load("a13_g10_p1a_a13_006_repro.py","r3i_repro")
    native_runner=load("a13_g10_r_r3_native.py","r3i_native_stubs")
    baseline=integrator.compose()
    sources=trace.compose_trace(lab.compose_lab(baseline))
    require(len(sources)==14 and len(baseline)==14,"R3_SOURCE_COUNT_CHANGED")
    h=lab.HEADER
    c=lab.CPP
    require(all(sources[k]==baseline[k] for k in baseline if k not in (h,c)),
            "R3_CHANGED_UNRELATED_PRODUCT_SOURCE")
    versions=[]
    for signature, filename in SIGNATURES:
        path=NET+filename
        source=sources.get(path)
        if source is None:
            # DhcpAsyncControl.cpp no pertenece a las 14 rutas overlay;
            # se toma sólo como cuerpo productivo read-only.
            require(filename=="DhcpAsyncControl.cpp","R3_PROTOCOL_MISSING_"+filename)
            source=(ROOT/path).read_text(encoding="utf-8-sig")
        versions.append(repro.extract(source,signature))
    require(len(versions)==14 and all("stop()" in m for m in versions[1:4]),
            "R3_SOURCE_METHOD_COUNT_OR_GUARD")
    with tempfile.TemporaryDirectory(prefix="jwplc-r3-dns-dhcp-complete-") as dirname:
        root=Path(dirname)
        inc=root/"include"
        (inc/"freertos").mkdir(parents=True)
        (inc/"freertos"/"task.h").write_text(native_runner.TASK_H,encoding="utf-8")
        (inc/"jwplc_spi_bus.h").write_text(native_runner.SPI_H,encoding="utf-8")
        (root/"JWPLC_W5500_SafeAccess.h").write_text(sources[h],encoding="utf-8")
        (root/"JWPLC_W5500_SafeAccess.cpp").write_text(sources[c],encoding="utf-8")
        joined="\n\n".join(versions)+"\n"
        exe=root/"r3_protocol_native"
        cmd=[compiler,"-std=c++17","-O0","-Wall","-Wextra",
             "-I",str(inc),"-I",str(root),
             str(root/"JWPLC_W5500_SafeAccess.cpp"),str(TEST),"-o",str(exe)]
        def run(bodies):
            (root/"C2_S2B2A_REAL_PATCHED_METHODS.inc").write_text(
                "// Source-extracted 14 real C++ methods, no model copies\n"+bodies,
                encoding="utf-8")
            built=subprocess.run(cmd,capture_output=True,text=True,timeout=120,
                                 errors="replace")
            require(built.returncode==0 and exe.is_file(),
                    "R3_PROTOCOL_COMPILE_FAILED_"+built.stderr[-1600:])
            return subprocess.run([str(exe)],capture_output=True,text=True,
                                  timeout=25,errors="replace")
        result=run(joined)
        require(result.returncode==0 and all(tag in result.stdout for tag in POSITIVE),
                "R3_PROTOCOL_POSITIVE_FAILED_"+(result.stdout+result.stderr)[-1500:])
        # Tres negativos de alto impacto sobre los MISMOS cuerpos:
        # conservar cobertura de nueve mutantes original en G10-I sin
        # repetir toda la matriz N3 anterior.
        mutants=[
          ("DNS_DROP_TICKET",0,
           "(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);",
           "/* R3 mutant: ticket lost */",
           "DNS_FINISH_TICKET_NOT_DRAINED"),
          ("DHCP_CANCEL_LEAK",13,
           "_dhcpUdpSocket.stop();",
           "/* R3 mutant: DHCP cancel leaked */",
           "DHCP_CANCEL_NOT_DRAINED"),
          ("UDP_MULTICAST_WITHOUT_BUS",8,
           "if (!bus) return 0; // preserve previous owner on BUS_BUSY",
           "if (false) return 0;",
           "B03_BEGIN_WITHOUT_BUS"),
        ]
        for label,index,needle,replacement,reason in mutants:
            require(versions[index].count(needle)==1,
                    "R3_MUTANT_SOURCE_DRIFT_"+label)
            changed=list(versions)
            changed[index]=changed[index].replace(needle,replacement,1)
            trial=run("\n\n".join(changed)+"\n")
            require(trial.returncode!=0 and reason in trial.stderr,
                    "R3_MUTANT_NOT_REJECTED_"+label+"_"+trial.stderr[-550:])
        for tag in POSITIVE:print(tag)
        print("G10_R_R3_PROTOCOL_NATIVE_METHODS=14_PASS_REAL_CPP")
        print("G10_R_R3_PROTOCOL_NATIVE_NEGATIVES=3_REJECTED")
        print("G10_R_R3_PROTOCOL_SOURCE_OVERLAY=14_ROUTES_ONLY_2_INSTRUMENTED")
        print("G10_R_R3_PROTOCOL_PHYSICAL=NOT_RUN")


def selftest():
    require(TEST.is_file() and len(SIGNATURES)==14 and len(POSITIVE)==12,
            "R3_PROTOCOL_SUITE_INCOMPLETE")
    print("G10_R_R3_PROTOCOL_SELFTEST=PASS_STATIC_CONTRACT")


def main():
    p=argparse.ArgumentParser(description=__doc__)
    m=p.add_mutually_exclusive_group(required=True)
    m.add_argument("--selftest",action="store_true")
    m.add_argument("--native",action="store_true")
    args=p.parse_args()
    try:
        if args.selftest:selftest()
        else:run_native()
        return 0
    except (RuntimeError,OSError,subprocess.TimeoutExpired) as exc:
        print("G10_R_R3_PROTOCOL_REVIEW_STOP="+str(exc))
        return 2

if __name__=="__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""Alpha13 G10 — integrador por hitos; fase actual: preparación unificada.

Compone TODOS los overlays ya probados sobre la MISMA fuente y reporta
brechas. No repite gates individuales ni confunde composición con build
Arduino o adopción. Solo escrituras dentro de tools/alpha13/results/.
El avance a compilación integrada requiere resolver bloqueos y autorizar
separadamente cualquier cambio de firmware y cualquier gate físico.
"""
from __future__ import annotations

import argparse
from datetime import datetime
import hashlib
import importlib.util
import json
import shutil
from pathlib import Path
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
PREVIOUS_HEAD = "c25a1201e7ead17475b06c3dd0f7fa3554c94757"
PREVIOUS_RUN = "g10_p1g_c2_s2c2b1_20261013_135948_9b7cb66e"
PREVIOUS_STATUS = "UDP_REAL_CPP_LIFECYCLE_NATIVE_PASS_DNS_PENDING_ADOPTION_BLOCKED"
MODULES = {
    "repro": "a13_g10_p1a_a13_006_repro.py",
    "s1": "a13_g10_p1g_c2_first_real_spi_slice.py",
    "s2a": "a13_g10_p1g_c2_s2a_socket_begin_lease.py",
    "b1": "a13_g10_p1g_c2_s2b1_client_server_lease.py",
    "b2a": "a13_g10_p1g_c2_s2b2a_cooperative_stop.py",
    "b2b": "a13_g10_p1g_c2_s2b2b_outbound_connect.py",
    "c1": "a13_g10_p1g_c2_s2c1_tx_cancel.py",
    "c2a": "a13_g10_p1g_c2_s2c2a_legacy_connect.py",
    "udp": "a13_g10_p1g_c2_s2c2b1_udp_lifecycle.py",
    "frozen": "a13_g10_p1g_a_mutex_callpaths.py",
}
# Ningún PASS previo resuelve todavía estas fronteras de integración.
BLOCKERS = {
    "B01": "SPI_BUSY: EthernetClient.connected() y socketStatus() pueden simular CLOSED o desconexión",
    "B02": "Client capacity, flush y endpoints corregidos; status/connected dependen de política B01 y ownership Server/UDP de B04/B05",
    "B03": "Dns.cpp real, EthernetUDP dependiente y DHCP no están integrados en un mismo flujo",
    "B04": "stop() / connect() con timeout y SPI ocupado pueden dejar handles sin reintento automático garantizado",
    "B05": "Identidad real de server_port[]/listeners, socket recycling y reset físico no validada a nivel end-to-end",
    "B06": "Sin build Arduino CLI/IDE baseline-vs-overlay con autoload y cinco archives congelados",
    "B07": "Sin medición física de fairness y regresión W5500, Display, SD, FRAM, RTC, TCA, RS-485, Modbus RTU/TCP",
    "B08": "Overlay único no equivale a compilación completa: revisar doble lock, firmas públicas y compatibilidad del core",
}
def emit(k,v):
    print("G10_INTEGRATED_"+str(k)+"="+str(v),flush=True)
def require(ok,why):
    if not ok:raise RuntimeError(why)
def git(*args):
    p=subprocess.run(["git","-c","core.quotepath=false",*args],cwd=ROOT,
                     capture_output=True,text=True,errors="replace",timeout=35)
    require(p.returncode==0,"GIT_FAILED_"+repr(args))
    return p.stdout.strip()
def digest(path):
    h=hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda:f.read(1024*1024),b""):
            h.update(chunk)
    return h.hexdigest()
def load(rel,name):
    p=ROOT/"tools/alpha13/gates"/rel
    spec=importlib.util.spec_from_file_location("g10_integrated_"+name,p)
    require(spec is not None and spec.loader is not None,"IMPORT_ERROR_"+name)
    m=importlib.util.module_from_spec(spec)
    sys.modules[spec.name]=m
    spec.loader.exec_module(m)
    return m
def preflight(ci):
    require(Path.cwd().resolve()==ROOT,"WRONG_CWD")
    require(Path(git("rev-parse","--show-toplevel")).resolve()==ROOT,
            "WRONG_REPO")
    head=git("rev-parse","HEAD")
    require(subprocess.run(["git","merge-base","--is-ancestor",PREVIOUS_HEAD,head],
                           cwd=ROOT,capture_output=True).returncode==0,
            "S2C2B1_HEAD_NOT_ANCESTOR")
    require(not git("diff","--name-only") and
            not git("diff","--cached","--name-only"),
            "TRACKED_WORKTREE_DIRTY_STOP_REVIEW_NO_CLEAN")
    if not ci:
        require(git("branch","--show-current")==BRANCH,"WRONG_BRANCH")
        require(head==git("rev-parse","refs/remotes/origin/"+BRANCH),
                "REMOTE_HEAD_MISMATCH")
        path=ROOT/"tools/alpha13/results"/PREVIOUS_RUN/"MANIFEST.json"
        require(path.exists(),"LOCAL_S2C2B1_MANIFEST_MISSING")
        m=json.loads(path.read_text(encoding="utf-8-sig"))
        require(m.get("run_id")==PREVIOUS_RUN and
                m.get("head")==PREVIOUS_HEAD and
                m.get("status")==PREVIOUS_STATUS,
                "LOCAL_S2C2B1_NOT_QUALIFIED")
    f=load(MODULES["frozen"],"frozen")
    require(f.check_frozen()==5,"PRECOMPILED_HASH_DRIFT")
    f.check(f.sources())
    return head

def patch_client_modbus_b01_b04(repro, first):
    """Parcheo source-backed de consulta triestado y cierre reintentable.

    No altera la API Arduino legacy; los nuevos símbolos son adiciones
    internas al overlay. Cada guard de deriva debe fallar cerrado.
    """
    hdr=first["HEADER"]
    signature="uint8_t getSocketNumber() const { return _sockindex; }"
    require(hdr.count(signature)==1,"G10_CLIENT_PROBE_HEADER_DRIFT")
    hdr=hdr.replace(signature,signature+
        "\n\t// Estado tri-valuado interno; no convertir BUS_BUSY a CLOSED.\n"
        "\tjwplc_w5500_candidate::StatusReadOutcome jwplcProbeSocketStatus(uint8_t& result);",1)
    first["HEADER"]=hdr
    cpp=first["CLIENT"]
    require("EthernetClient::jwplcProbeSocketStatus" not in cpp,
            "CLIENT_PROBE_ALREADY_PRESENT")
    cpp+="""

// G10-I B01: Status bajo ownership y la misma exclusión W5500.
jwplc_w5500_candidate::StatusReadOutcome
EthernetClient::jwplcProbeSocketStatus(uint8_t& result)
{
    if (_sockindex >= MAX_SOCK_NUM)
        return jwplc_w5500_candidate::StatusReadOutcome::Stale;
    return jwplc_w5500_candidate::readStatusIfCurrent(
        _jwplcSocketToken,
        [](uint8_t index, void*) -> uint8_t {
            return Ethernet.socketStatus(index);
        },
        nullptr, 20, &result);
}
"""
    first["CLIENT"]=cpp
    mh_path="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h"
    mh=(ROOT/mh_path).read_text(encoding="utf-8-sig")
    field="    bool _sendStarted;"
    require(mh.count(field)==1,"MODBUS_HEADER_SEND_FIELD_DRIFT")
    mh=mh.replace(field,field+
        "\n    // G10-I: retener el objeto/socket hasta CLOSE o STALE real.\n"
        "    bool _jwplcClosePending = false;",1)
    proto="    void closeSession();"
    require(mh.count(proto)==1,"MODBUS_HEADER_CLOSE_DECL_DRIFT")
    mh=mh.replace(proto,proto+
        "\n    bool serviceDeferredClose();",1)
    first["MB_CLIENT_HEADER"]=mh
    mb=first["MB_CLIENT"]
    def replace_body(signature,old_fragment,new_fragment):
        nonlocal mb
        old=repro.extract(mb,signature)
        require(old.count(old_fragment)==1,
                "MODBUS_BODY_SOURCE_DRIFT_"+signature)
        new=old.replace(old_fragment,new_fragment,1)
        require(new!=old and mb.count(old)==1,
                "MODBUS_BODY_NOT_UNIQUE_"+signature)
        mb=mb.replace(old,new,1)
    replace_body("bool JWPLC_ModbusTCPClientClass::busy() const",
        "{",
        "{\n    // Mantener task() activo si el cierre necesita reintento.\n"
        "    if (_jwplcClosePending) return true;")
    replace_body("bool JWPLC_ModbusTCPClientClass::begin(",
        "    if (busy())",
        "    if (_jwplcClosePending) {\n"
        "        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;\n"
        "        return false;\n"
        "    }\n    if (busy())")
    replace_body("bool JWPLC_ModbusTCPClientClass::startRequest(",
        "    if (busy())",
        "    if (_jwplcClosePending) {\n"
        "        _result = JWPLC_MODBUS_TCP_CLIENT_BUSY;\n"
        "        return false;\n"
        "    }\n    if (busy())")
    replace_body("void JWPLC_ModbusTCPClientClass::task()",
        "    if (!busy())",
        "    // Incluso despues de fail()/end() se debe poder drenar CLOSE.\n"
        "    if (_jwplcClosePending && !serviceDeferredClose()) return;\n"
        "    if (!busy())")
    mb_sig="void JWPLC_ModbusTCPClientClass::closeSession()"
    before=repro.extract(mb,mb_sig)
    require(before.count("_tcpClient = EthernetClient();")==1 and
            "cancelConnectAsync();" in before,"CLOSESESSION_SOURCE_DRIFT")
    after=mb_sig+"""
{
    _asyncTx.reset();
    // No abandonar el token si otro task posee SPI.
    _jwplcClosePending = _jwplcClosePending || (bool)_tcpClient;
    if (_jwplcClosePending)
        (void)serviceDeferredClose();
    else
        _tcpClient = EthernetClient();

    _sessionConnected = false;
    _connectStarted = false;
    _sendStarted = false;
    resetRx();
}
"""
    require(mb.count(before)==1,"CLOSESESSION_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    mb+="""

// Servicio cooperativo, sin espera ocupada ni cierre por indice sin owner.
bool JWPLC_ModbusTCPClientClass::serviceDeferredClose()
{
    if (!_jwplcClosePending) return true;
    if (!acquireBus(20)) return false; // retain _tcpClient and token
    _tcpClient.cancelStopAsync(); // guard anidado y lease en EthernetClient
    const bool released = !(bool)_tcpClient;
    releaseBus();
    if (released) {
        _tcpClient = EthernetClient();
        _jwplcClosePending = false;
    }
    return released;
}
"""
    # Lectura previa al uso: los consumidores internos nunca deben
    # interpretar BusBusy como desconexion. La validacion se hace dentro
    # de su scope de Modbus para evitar carreras entre probe y consumo.
    status_sig="void JWPLC_ModbusTCPClientClass::serviceConnection()"
    before=repro.extract(mb,status_sig)
    probe="        const bool stillConnected = _tcpClient.connected() != 0;"
    require(before.count(probe)==1,"MODBUS_CONNECT_STATUS_DRIFT")
    after=before.replace(probe,"""        uint8_t observedStatus = 0;
        const auto probe = _tcpClient.jwplcProbeSocketStatus(observedStatus);
        if (probe == jwplc_w5500_candidate::StatusReadOutcome::BusBusy) {
            releaseBus();
            return; // no falsa desconexion con SPI ocupado
        }
        const bool stillConnected =
            probe == jwplc_w5500_candidate::StatusReadOutcome::Read &&
            _tcpClient.connected() != 0;""",1)
    require(mb.count(before)==1,"MODBUS_SERVICE_CONNECT_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    resp_sig="void JWPLC_ModbusTCPClientClass::serviceResponse()"
    before=repro.extract(mb,resp_sig)
    needle="    int availableBytes = _tcpClient.available();"
    require(before.count(needle)==1,"MODBUS_RESPONSE_AVAILABLE_DRIFT")
    after=before.replace(needle,"""    uint8_t observedStatus = 0;
    const auto probe = _tcpClient.jwplcProbeSocketStatus(observedStatus);
    if (probe == jwplc_w5500_candidate::StatusReadOutcome::BusBusy) {
        releaseBus();
        return; // SPI_BUSY != link failure
    }
    if (probe == jwplc_w5500_candidate::StatusReadOutcome::Stale) {
        releaseBus();
        fail(JWPLC_MODBUS_TCP_CLIENT_TRANSPORT_ERROR, true);
        return;
    }
    int availableBytes = _tcpClient.available();""",1)
    require(mb.count(before)==1,"MODBUS_SERVICE_RESPONSE_NOT_UNIQUE")
    mb=mb.replace(before,after,1)
    first["MB_CLIENT"]=mb
    return first


def patch_modbus_server_deferred_drop(repro, first):
    """B01/B04 server: mantener el cliente hasta close validado, sin GPIO4."""
    hdr_path="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"
    h=(ROOT/hdr_path).read_text(encoding="utf-8-sig")
    field="    bool _clientActive;"
    require(h.count(field)==1,"SERVER_DROP_HEADER_STATE_DRIFT")
    h=h.replace(field,field+"\n    bool _jwplcDropPending = false; // G10-I overlay only",1)
    decl="    void dropClient();"
    require(h.count(decl)==1,"SERVER_DROP_DECL_DRIFT")
    h=h.replace(decl,decl+"\n    bool serviceDeferredDrop();",1)
    first["MB_SERVER_HEADER"]=h
    cpp=first["MB_SERVER"]
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::dropClient()")
    require("if (_clientActive && acquireBus(20))" in old
            and "_client = EthernetClient();" in old,
            "SERVER_DROP_SOURCE_CHANGED")
    new="""void JWPLC_ModbusTCPClass::dropClient()
{
    // No perder la identidad del socket mientras falte mutex.
    _jwplcDropPending = _jwplcDropPending || (bool)_client;
    _clientActive = false;
    resetRx();
    if (_jwplcDropPending && !serviceDeferredDrop()) {
        return; // must be serviced before accepting any new socket
    }
    if (_serverEnabled && JWPLC_Ethernet.isReady())
        _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;
    else if (_serverEnabled)
        _serverState = JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET;
    else
        _serverState = JWPLC_MODBUS_TCP_SERVER_STOPPED;
}"""
    require(cpp.count(old)==1,"SERVER_DROP_METHOD_NOT_UNIQUE")
    cpp=cpp.replace(old,new,1)
    cpp+="""

// G10-I server cooperative cleanup. Never close an index without lease.
bool JWPLC_ModbusTCPClass::serviceDeferredDrop()
{
    if (!_jwplcDropPending) return true;
    if (!acquireBus(20)) return false;
    // The configured INT socket can no longer belong to this client.
    // Only touch W5500 INT state if the token is CURRENT.
    uint8_t observedStatus = 0;
    const auto outcome = _client.jwplcProbeSocketStatus(observedStatus);
    if (outcome == jwplc_w5500_candidate::StatusReadOutcome::Read &&
        _rxIntConfigured && _rxIntSocket == _client.getSocketNumber())
        disableRxIntLocked();
    _client.cancelStopAsync();  // nested scope checks token before close
    const bool released = !(bool)_client;
    releaseBus();
    if (released) {
        _client = EthernetClient();
        _jwplcDropPending = false;
        resetRx();
        resetRxIntSoftware();
    }
    return released;
}
"""
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::task()")
    anchor="    if (!_serverEnabled)"
    require(old.count(anchor)==1,"SERVER_TASK_SOURCE_CHANGED")
    revised=old.replace(anchor,
       """    // El cierre pendiente se atiende incluso si Ethernet deja de READY.
    if (_jwplcDropPending && !serviceDeferredDrop()) return;
"""+anchor,1)
    reset="""        _serverListening = false;
        _clientActive = false;
        _client = EthernetClient();"""
    require(revised.count(reset)==1,"SERVER_NOT_READY_RESET_DRIFT")
    revised=revised.replace(reset,"""        if (_clientActive || (bool)_client) {
            dropClient();
            if (_jwplcDropPending) return;
        }
        _serverListening = false;
        _clientActive = false;
        _client = EthernetClient();""",1)
    require(cpp.count(old)==1,"SERVER_TASK_NOT_UNIQUE")
    cpp=cpp.replace(old,revised,1)
    old=repro.extract(cpp,"void JWPLC_ModbusTCPClass::serviceServer()")
    anchor="""    const uint32_t now = millis();"""
    require(old.count(anchor)==1,"SERVER_SERVICE_NOW_DRIFT")
    revised=old.replace(anchor,anchor+"""
    if (_jwplcDropPending && !serviceDeferredDrop())
        return;
""",1)
    # If status was actually CLOSED, use the guarded deferred close path
    # rather than throwing the client handle away without invalidate.
    old_drop="""            _client = EthernetClient();
            _clientActive = false;
            resetRx();
            _serverState = JWPLC_MODBUS_TCP_SERVER_LISTENING;"""
    require(revised.count(old_drop)==1,"SERVER_PEER_DISCONNECT_DRIFT")
    revised=revised.replace(old_drop,"""            dropClient(); // owns valid token or defers safely
            if (_jwplcDropPending) return;""",1)
    require(cpp.count(old)==1,"SERVER_SERVICE_NOT_UNIQUE")
    cpp=cpp.replace(old,revised,1)
    first["MB_SERVER"]=cpp
    return first


def patch_udp_rx_tx_b02(repro, source):
    """B02: keep direct UDP data access inside the current socket lease."""
    paths=(
        ("size_t EthernetUDP::write(const uint8_t *buffer, size_t size)","0","0"),
        ("int EthernetUDP::parsePacket()","0","0"),
        ("int EthernetUDP::read()","-1","-1"),
        ("int EthernetUDP::read(unsigned char *buffer, size_t len)","-1","-1"),
        ("int EthernetUDP::peek()","-1","-1"),
        ("int EthernetUDP::jwplcReadPacketFastDeferred(","0","-1"),
        ("bool EthernetUDP::jwplcCommitRxFast()","false","false"),
    )
    for signature,busy,stale in paths:
        old=repro.extract(source,signature)
        require("jwplc_w5500_candidate::BusScope bus" not in old,
                "B02_UDP_ALREADY_GUARDED_"+signature)
        opener=old.index("{")
        guard=("\n\tjwplc_w5500_candidate::BusScope bus(20);"
               "\n\tif (!bus) return "+busy+";"
               "\n\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken))"
               " return "+stale+";")
        new=old[:opener+1]+guard+old[opener+1:]
        require(source.count(old)==1,"B02_UDP_METHOD_NOT_UNIQUE_"+signature)
        source=source.replace(old,new,1)
    return source


def patch_tcp_rx_b02(repro, source):
    """B02: guard direct Client RX calls without changing public signatures."""
    paths=(
        ("int EthernetClient::available()","0","0"),
        ("int EthernetClient::read(uint8_t *buf, size_t size)","0","0"),
        ("int EthernetClient::read()","-1","-1"),
        ("int EthernetClient::peek()","-1","-1"),
        ("int EthernetClient::jwplcReadTcpFastDeferred(","0","-1"),
        ("bool EthernetClient::jwplcCommitRxFast()","false","false"),
    )
    for signature,busy,stale in paths:
        old=repro.extract(source,signature)
        require("jwplc_w5500_candidate::BusScope bus" not in old,
                "B02_TCP_ALREADY_GUARDED_"+signature)
        opener=old.index("{")
        guard=("\n\tjwplc_w5500_candidate::BusScope bus(20);"
               "\n\tif (!bus) return "+busy+";"
               "\n\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken))"
               " return "+stale+";")
        new=old[:opener+1]+guard+old[opener+1:]
        require(source.count(old)==1,"B02_TCP_METHOD_NOT_UNIQUE_"+signature)
        source=source.replace(old,new,1)
    return source


def patch_tcp_tx_b02(repro, source):
    """Run legacy writes through bounded, lease-checked async SEND steps."""
    signature="size_t EthernetClient::write(const uint8_t *buf, size_t size)"
    old=repro.extract(source,signature)
    require(old.count("Ethernet.socketSend(")==1 and
            old.count("setWriteError();")==2,
            "B02_TCP_WRITE_SOURCE_DRIFT")
    new="""size_t EthernetClient::write(const uint8_t *buf, size_t size)
{
    if (size == 0) return 0;
    if (_sockindex >= MAX_SOCK_NUM || buf == nullptr ||
        writeAsyncInProgress() || _stopPending || _flushPending) {
        setWriteError();
        return 0;
    }

    // One SEND step per poll; yield only after its bus scope was released.
    const uint32_t startedMs = millis();
    size_t totalWritten = 0;
    while (totalWritten < size) {
        if ((uint32_t)(millis() - startedMs) >= _timeout) break;
        const size_t remaining = size - totalWritten;
        const uint16_t chunk = (uint16_t)(
            remaining < (size_t)W5100.SSIZE ? remaining : (size_t)W5100.SSIZE);
        int state = beginWriteAsync(buf + totalWritten, chunk);
        while (state == 0) {
            if ((uint32_t)(millis() - startedMs) >= _timeout) {
                // If SEND was not issued, cancellation releases the buffer
                // even under BUS_BUSY. After SEND, cleanup retains its lease.
                cancelWriteAsync();
                break;
            }
            yield(); // no JWPLC SPI mutex held here
            state = pollWriteAsync();
        }
        if (state != 1) break;
        totalWritten += chunk;
    }
    if (totalWritten != size) setWriteError();
    return totalWritten;
}"""
    require(source.count(old)==1,"B02_TCP_WRITE_NOT_UNIQUE")
    source=source.replace(old,new,1)

    signature="int EthernetClient::pollWriteAsync()"
    old=repro.extract(source,signature)
    require(old.count("Ethernet.socketBeginSendTCP(")==1 and
            old.count("Ethernet.socketPollSendTCP(")==1,
            "B02_TCP_POLL_SOURCE_DRIFT")
    guard="""
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) return 0; // retain the SEND state and its buffer
    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM;
        _writeAsyncState = 0;
        _writeAsyncBuffer = nullptr;
        _writeAsyncLength = 0;
        _writeAsyncStartedAtMs = 0;
        return -1; // recycled slot or chip epoch: no W5500 access
    }
"""
    anchor="\tif (_writeAsyncState == 1) {"
    require(old.count(anchor)==1,"B02_TCP_POLL_STATE_ANCHOR")
    new=old.replace(anchor,guard+anchor,1)
    require(source.count(old)==1,"B02_TCP_POLL_NOT_UNIQUE")
    source=source.replace(old,new,1)

    signature="void EthernetClient::cancelWriteAsync()"
    old=repro.extract(source,signature)
    anchor="\tjwplc_w5500_candidate::BusScope bus(20);"
    require(old.count(anchor)==1,"B02_TCP_CANCEL_ANCHOR")
    early="""    if (_writeAsyncState == 1) {
        // No SEND command was issued; dropping the borrowed buffer is safe.
        _writeAsyncState = 0;
        _writeAsyncBuffer = nullptr;
        _writeAsyncLength = 0;
        _writeAsyncStartedAtMs = 0;
        return;
    }
"""
    new=old.replace(anchor,early+anchor,1)
    require(source.count(old)==1,"B02_TCP_CANCEL_NOT_UNIQUE")
    return source.replace(old,new,1)

def patch_tcp_remaining_b02(repro, source):
    """Guard direct Client capacity, flush and endpoint reads by socket lease."""
    signature="int EthernetClient::availableForWrite(void)"
    old=repro.extract(source,signature)
    require(old.count("Ethernet.socketSendAvailable(_sockindex)")==1 and
            "BusScope" not in old,"B02_CAPACITY_SOURCE_DRIFT")
    new=old.replace("\tif (_sockindex >= MAX_SOCK_NUM) return 0;",
        "\tjwplc_w5500_candidate::BusScope bus(20);\n"
        "\tif (!bus || _sockindex >= MAX_SOCK_NUM) return 0;\n"
        "\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;",1)
    require(source.count(old)==1,"B02_CAPACITY_NOT_UNIQUE")
    source=source.replace(old,new,1)

    signature="int EthernetClient::pollFlushAsync()"
    old=repro.extract(source,signature)
    anchor="\tconst uint8_t stat = Ethernet.socketStatus(_sockindex);"
    require(old.count(anchor)==1 and "BusScope" not in old,
            "B02_FLUSH_SOURCE_DRIFT")
    guard="""    // pollWriteAsync() has completed its own short bus scope. Acquire a
    // fresh lease before reading status and TX space on this socket.
    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) {
        if ((uint32_t)(millis() - _flushStartedAtMs) < _timeout) return 0;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
    }
    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
    }
"""
    new=old.replace(anchor,guard+anchor,1)
    require(source.count(old)==1,"B02_FLUSH_NOT_UNIQUE")
    source=source.replace(old,new,1)

    for signature,zero in (("uint16_t EthernetClient::localPort()","0"),
                           ("IPAddress EthernetClient::remoteIP()",
                            "IPAddress((uint32_t)0)"),
                           ("uint16_t EthernetClient::remotePort()","0")):
        old=repro.extract(source,signature)
        anchor="\tif (_sockindex >= MAX_SOCK_NUM) return "+zero+";"
        require(old.count(anchor)==1 and old.count("SPI.beginTransaction(")==1,
                "B02_ENDPOINT_SOURCE_DRIFT_"+signature)
        new=old.replace(anchor,
            "\tjwplc_w5500_candidate::BusScope bus(20);\n"
            "\tif (!bus || _sockindex >= MAX_SOCK_NUM) return "+zero+";\n"
            "\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken))"
            " return "+zero+";",1)
        require(source.count(old)==1,"B02_ENDPOINT_NOT_UNIQUE_"+signature)
        source=source.replace(old,new,1)
    return source

def patch_server_b04_b05(repro, source):
    """Snapshot only current server sockets; send after releasing SPI."""
    signature="EthernetServer::operator bool()"
    old=repro.extract(source,signature)
    require(old.count("W5100.getChip()")==1 and
            old.count("Ethernet.socketStatus(i)")==1 and
            "BusScope" not in old,"B04_SERVER_BOOL_SOURCE_DRIFT")
    new=old.replace("{", "{\n\tjwplc_w5500_candidate::BusScope bus(20);\n"
                    "\tif (!bus) return false; // public bool cannot encode BUS_BUSY",1)
    new=new.replace("if (server_port[i] == _port) {",
        "if (server_port[i] == _port &&\n"
        "            jwplc_w5500_candidate::listenerMatchesLocked(\n"
        "                i, _port, jwplc_w5500_candidate::currentSocketTokenLocked(i))) {",1)
    require(source.count(old)==1,"B04_SERVER_BOOL_NOT_UNIQUE")
    source=source.replace(old,new,1)

    signature="size_t EthernetServer::write(const uint8_t *buffer, size_t size)"
    old=repro.extract(source,signature)
    require(old.count("W5100.getChip()")==1 and
            old.count("Ethernet.socketStatus(i)")==1 and
            old.count("client.write(buffer, size)")==1 and
            "BusScope" not in old,"B05_SERVER_WRITE_SOURCE_DRIFT")
    new="""size_t EthernetServer::write(const uint8_t *buffer, size_t size)
{
    if (buffer == nullptr || size == 0) return 0;
    // Keep the legacy listener-maintenance pass, outside the snapshot lock.
    available();

    EthernetClient clients[MAX_SOCK_NUM];
    uint8_t count = 0;
    {
        jwplc_w5500_candidate::BusScope bus(20);
        if (!bus) return 0; // no listener or socket inference on SPI_BUSY
        const uint8_t chip = W5100.getChip();
        if (!chip) return 0;
        uint8_t maxindex = MAX_SOCK_NUM;
#if MAX_SOCK_NUM > 4
        if (chip == 51) maxindex = 4;
#endif
        for (uint8_t i = 0; i < maxindex; i++) {
            const auto token = jwplc_w5500_candidate::currentSocketTokenLocked(i);
            if (server_port[i] != _port ||
                !jwplc_w5500_candidate::listenerMatchesLocked(i, _port, token) ||
                Ethernet.socketStatus(i) != SnSR::ESTABLISHED) continue;
            // The constructor captures the token while this same scope owns
            // SPI. A recycled slot cannot become a new broadcast target.
            clients[count] = EthernetClient(i);
            if (clients[count].getSocketNumber() == i &&
                jwplc_w5500_candidate::matchesLocked(clients[count]._jwplcSocketToken))
                ++count;
        }
    }

    // Each write may wait for SEND_OK. Never hold the server scan lock here.
    size_t minimumWritten = size;
    for (uint8_t i = 0; i < count; i++) {
        const size_t written = clients[i].write(buffer, size);
        if (written < minimumWritten) minimumWritten = written;
    }
    return count ? minimumWritten : 0;
}"""
    require(source.count(old)==1,"B05_SERVER_WRITE_NOT_UNIQUE")
    return source.replace(old,new,1)

def compose():
    mods={k:load(v,k) for k,v in MODULES.items() if k!="frozen"}
    repro=mods["repro"]
    s1,s2a,b1=mods["s1"],mods["s2a"],mods["b1"]
    b2a,b2b,c1,c2a,udp=(mods[k] for k in ("b2a","b2b","c1","c2a","udp"))
    # Cada capa se aplica a la salida de la anterior: evita overlays
    # independientes incompatibles que antes daban falsos PASS aislados.
    raw={k:(ROOT/p).read_text(encoding="utf-8-sig")
         for k,p in b1.FILES.items()}
    first=b1.patch(repro,raw)
    s1_inputs={k:(first["CLIENT"] if k=="RAW_CLIENT" else
                  (ROOT/p).read_text(encoding="utf-8-sig"))
               for k,p in s1.TARGETS.items()}
    s1_overlay=s1.patch_functions(repro,s1_inputs)
    first["CLIENT"]=s1_overlay["RAW_CLIENT"]
    first["SOCKET"]=s1_overlay["SOCKET"]
    for k,p in s1.TARGETS.items():
        if k not in ("RAW_CLIENT","SOCKET"):
            first[k]=s1_overlay[k]
    first["CLIENT"],_=b2a.patched_methods(repro,first["CLIENT"])
    field="jwplc_w5500_candidate::SocketToken _jwplcSocketToken{};"
    require(first["HEADER"].count(field)==1,"CLIENT_TOKEN_FIELD_NOT_UNIQUE")
    first["HEADER"]=first["HEADER"].replace(field,field+"""
    bool _jwplcDeferredConnect = false;
    IPAddress _jwplcDeferredIp;
    uint16_t _jwplcDeferredPort = 0;""",1)
    first["CLIENT"],_=b2b.patch(repro,first["CLIENT"])
    first["CLIENT"],_=c1.patch_tx(repro,first["CLIENT"])
    first["CLIENT"],_=c2a.patch_legacy(repro,first["CLIENT"])
    first["CLIENT"]=patch_tcp_rx_b02(repro,first["CLIENT"])
    first["CLIENT"]=patch_tcp_tx_b02(repro,first["CLIENT"])
    first["CLIENT"]=patch_tcp_remaining_b02(repro,first["CLIENT"])
    first["SERVER"]=patch_server_b04_b05(repro,first["SERVER"])
    socket_overlay,_=s2a.patch(repro,first["SOCKET"])
    first["SOCKET"]=socket_overlay
    udp_original=(ROOT/udp.UDP_PATH).read_text(encoding="utf-8-sig")
    udp_overlay,_=udp.patches(repro,udp_original)
    first["UDP"]='#include "JWPLC_W5500_SafeAccess.h"\n'+udp_overlay
    # A DNS stack object can disappear immediately after finish/cancel. Its
    # UDP stop therefore publishes a lease ticket before returning BUS_BUSY;
    # the autoload tick owns the retry independently of that object's life.
    busy="if (!bus) return; // never discard socket on SPI_BUSY"
    require(first["UDP"].count(busy)==1,"B03_UDP_BUSY_SOURCE_DRIFT")
    first["UDP"]=first["UDP"].replace(busy,
        "if (!bus) {\n"
        "\t\t(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);\n"
        "\t\treturn; // preserve the local handle and publish a durable ticket\n"
        "\t}",1)
    first["UDP"] += """

// Callable by the core autoload tick after a temporary DNS object is gone.
uint8_t EthernetUDP::jwplcServiceDeferredClose()
{
    return jwplc_w5500_candidate::serviceDeferredClose(
        [](uint8_t index, void*) { Ethernet.socketClose(index); }, nullptr, 0);
}
"""
    multicast_sig="uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port)"
    multicast_old=repro.extract(first["UDP"],multicast_sig)
    require(multicast_old.count("if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);")==1,
            "B03_UDP_MULTICAST_SOURCE_DRIFT")
    multicast_new=multicast_old.replace(multicast_sig+"\n{",multicast_sig+"\n{\n"
        "\tjwplc_w5500_candidate::BusScope bus(20);\n"
        "\tif (!bus) return 0; // preserve previous owner on BUS_BUSY",1)
    multicast_new=multicast_new.replace(
        "if (sockindex < MAX_SOCK_NUM) Ethernet.socketClose(sockindex);",
        "if (sockindex < MAX_SOCK_NUM &&\n"
        "\t    jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {\n"
        "\t\tEthernet.socketClose(sockindex);\n"
        "\t\t(void)jwplc_w5500_candidate::invalidateSocketLocked(_jwplcSocketToken);\n"
        "\t}",1)
    multicast_new=multicast_new.replace("if (sockindex >= MAX_SOCK_NUM) return 0;",
        "if (sockindex >= MAX_SOCK_NUM) return 0;\n"
        "\t_jwplcSocketToken=jwplc_w5500_candidate::currentSocketTokenLocked(sockindex);\n"
        "\tif (!bool(_jwplcSocketToken)) { sockindex=MAX_SOCK_NUM; return 0; }",1)
    first["UDP"]=first["UDP"].replace(multicast_old,multicast_new,1)
    first["UDP"]=patch_udp_rx_tx_b02(repro,first["UDP"])
    hdr_anchor="virtual void stop();  // Finish with the UDP socket"
    require(first["HEADER"].count(hdr_anchor)==1,"B03_UDP_HEADER_DRIFT")
    first["HEADER"]=first["HEADER"].replace(hdr_anchor,hdr_anchor+
        "\n\t// Internal cooperative drain, independent of object lifetime.\n"
        "\tstatic uint8_t jwplcServiceDeferredClose();",1)
    first=patch_client_modbus_b01_b04(repro,first)
    first=patch_modbus_server_deferred_drop(repro,first)
    udp_anchor="bool _sendPending = false; // JWPLC cooperative UDP SEND state"
    require(first["HEADER"].count(udp_anchor)==1,"UDP_HEADER_FIELD_DRIFT")
    first["HEADER"]=first["HEADER"].replace(udp_anchor,udp_anchor+
          "\n\tbool _jwplcDeferredSendStart = false;"
          "\n\tjwplc_w5500_candidate::SocketToken _jwplcSocketToken{};",1)
    # Includes duplicados son producto de combinar transformadores.
    # El overlay integrado debe contener solo uno; no modificar lógica.
    inc='#include "JWPLC_W5500_SafeAccess.h"\n'
    for k,v in first.items():
        if inc in v:
            require(v.count(inc)<=3,"UNEXPECTED_INCLUDE_MULTIPLICITY_"+k)
            first[k]=inc+v.replace(inc,"")
    paths={}
    for k,path in b1.FILES.items():
        require(k in first,"MISSING_LAYER_"+k)
        paths[path]=first[k]
    for k,path in s1.TARGETS.items():
        paths[path]=first["CLIENT"] if k=="RAW_CLIENT" else first[k]
    paths[udp.UDP_PATH]=first["UDP"]
    # Keep DNS/DHCP source byte-for-byte; their terminal paths intentionally
    # retain legacy results while the lease ticket outlives the UDP object.
    eth_src="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
    for name in ("Dns.cpp","Dhcp.cpp"):
        rel=eth_src+name
        paths[rel]=(ROOT/rel).read_text(encoding="utf-8-sig")
    socket_rel=eth_src+"socket.cpp"
    socket_source=paths[socket_rel]
    socket_anchor="if (!bus) return MAX_SOCK_NUM;"
    require(socket_source.count(socket_anchor)==1,"B03_SOCKET_BEGIN_DRIFT")
    paths[socket_rel]=socket_source.replace(socket_anchor,
        socket_anchor+"\n\tif (!jwplc_w5500_candidate::allocationAvailableLocked()) return MAX_SOCK_NUM;\n"
        "\t// Drain old UDP tickets before choosing a reusable slot.\n"
        "\t(void)jwplc_w5500_candidate::serviceDeferredClose(\n"
        "\t\t[](uint8_t index, void*) { Ethernet.socketClose(index); }, nullptr, 0);",1)
    multi_sig="uint8_t EthernetClass::socketBeginMulticast(uint8_t protocol, IPAddress ip, uint16_t port)"
    socket_source=paths[socket_rel]
    multi_old=repro.extract(socket_source,multi_sig)
    require(multi_old.count("\tSPI.endTransaction();\n\treturn s;")==1,
            "B03_SOCKET_MULTICAST_SOURCE_DRIFT")
    multi_new=multi_old.replace(multi_sig+"\n{",multi_sig+"\n{\n"
        "\tjwplc_w5500_candidate::BusScope bus(20);\n"
        "\tif (!bus || !jwplc_w5500_candidate::allocationAvailableLocked()) return MAX_SOCK_NUM;\n"
        "\t(void)jwplc_w5500_candidate::serviceDeferredClose(\n"
        "\t\t[](uint8_t index, void*) { Ethernet.socketClose(index); }, nullptr, 0);",1)
    multi_new=multi_new.replace("\tSPI.endTransaction();\n\treturn s;",
        "\tSPI.endTransaction();\n"
        "\t(void)jwplc_w5500_candidate::allocateSocketLockedAuto(s);\n"
        "\treturn s;",1)
    paths[socket_rel]=socket_source.replace(multi_old,multi_new,1)
    service_rel=eth_src+"JWPLC_Ethernet.cpp"
    service=(ROOT/service_rel).read_text(encoding="utf-8-sig")
    service_anchor="void JWPLC_EthernetClass::service()\n{\n#if !JWPLC_HAS_ETHERNET\n    return;\n#else"
    require(service.count(service_anchor)==1,"B03_AUTOLOAD_SERVICE_DRIFT")
    service=service.replace(service_anchor,service_anchor+
        "\n    // The core tick runs even after DNS/DHCP becomes logically inactive.\n"
        "    (void)EthernetUDP::jwplcServiceDeferredClose();",1)
    paths[service_rel]='#include "JWPLC_W5500_SafeAccess.h"\n'+service
    paths["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h"]=first["MB_CLIENT_HEADER"]
    paths["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h"]=first["MB_SERVER_HEADER"]
    # La fase antigua componía referencias a SafeAccess, pero OMITÍA sus
    # dos archivos. Un build Arduino completo exige ambas unidades.
    base=ROOT/"tools/alpha13/candidates/g10_p1g_c"
    candidate_base="JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
    for name in ("JWPLC_W5500_SafeAccess.h","JWPLC_W5500_SafeAccess.cpp"):
        rel=candidate_base+name
        require(rel not in paths and not (ROOT/rel).exists(),
                "SAFE_ACCESS_ALREADY_IN_PRODUCT_REVIEW_REQUIRED_"+name)
        paths[rel]=(base/name).read_text(encoding="utf-8-sig")
    return paths

def write_evidence(out,head,files):
    overlay=out/"overlay"
    hashes={}
    for name,source in sorted(files.items()):
        p=overlay/name
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text(source,encoding="utf-8")
        candidate=(ROOT/name)
        if not candidate.is_file():
            candidate=(ROOT/"tools/alpha13/candidates/g10_p1g_c"/Path(name).name)
        require(candidate.is_file(),"OVERLAY_SOURCE_PROVENANCE_MISSING_"+name)
        hashes[name]={"sha256":digest(p),"bytes":p.stat().st_size,
                      "source_sha256":digest(candidate),
                      "source_path":candidate.relative_to(ROOT).as_posix()}
    out.joinpath("BLOCKERS.md").write_text(
        "# G10-I — bloqueos de adopción\n\n"+
        "\n".join(f"- **{k}**: {v}" for k,v in BLOCKERS.items())+"\n",
        encoding="utf-8")
    return hashes

def stage_platform(destination,files,hashes):
    """Copia el package en ubicación temporal externa, sin modificar repo."""
    target=Path(destination).resolve()
    require(not target.is_relative_to(ROOT),"STAGE_CANNOT_BE_INSIDE_REPOSITORY")
    require(not target.exists(),"STAGE_MUST_NOT_ALREADY_EXIST")
    require(target.parent.is_dir(),"STAGE_PARENT_MISSING")
    require(str(target)!=str(target.anchor),"INVALID_STAGE_ROOT")
    source=ROOT/"JWPLC/2.1.0"
    shutil.copytree(source,target)
    manifest={}
    for rel,content in files.items():
        prefix="JWPLC/2.1.0/"
        require(rel.startswith(prefix),"OVERLAY_ESCAPES_PACKAGE_"+rel)
        dst=target/rel[len(prefix):]
        require(dst.resolve().is_relative_to(target),"OVERLAY_ESCAPE_DETECTED")
        dst.parent.mkdir(parents=True,exist_ok=True)
        dst.write_text(content,encoding="utf-8")
        require(digest(dst)==hashes[rel]["sha256"],
                "STAGED_SOURCE_SHA_DIFFERS_"+rel)
        manifest[rel]={"sha256":digest(dst),"relative_staged_path":rel[len(prefix):]}
    # Las bibliotecas .a se copian byte-a-byte. Los cinco archives son
    # inmutables, no se regeneran ni se vinculan de nuevo en esta etapa.
    frozen=load(MODULES["frozen"],"staged_frozen")
    refs=frozen.FROZEN if hasattr(frozen,"FROZEN") else {}
    # La comprobación del freeze canónico ya se ejecutó en preflight.
    return manifest

def server_cleanup_native(out,files):
    """Source-backed Server drop/retry tested inside the single G10-I run."""
    repro=load(MODULES["repro"],"server_cleanup_extract")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","server_cleanup_compiler")
    builder=load(MODULES["b2a"],"server_cleanup_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_server_cleanup_test.cpp"
    rel="JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp"
    src=files[rel]
    sigs=("void JWPLC_ModbusTCPClass::dropClient()",
          "bool JWPLC_ModbusTCPClass::serviceDeferredDrop()",
          "void JWPLC_ModbusTCPClass::task()")
    bodies={sig:repro.extract(src,sig) for sig in sigs}
    def run(tag, methods):
        return builder.native(out,tag,"\n\n".join(methods[sig] for sig in sigs)+"\n",compiler)
    code,output=run("server_cleanup_positive",bodies)
    wanted=("G10_I_SERVER_DEFERRED_CLOSE_BUSY_RETRY=PASS",
            "G10_I_SERVER_STALE_OWNER_NOT_CLOSED=PASS",
            "G10_I_SERVER_EMPTY_AND_LINK_DOWN=PASS",
            "G10_I_SERVER_TASK_PENDING_DRAIN=PASS",
            "G10_I_SERVER_LINK_DOWN_BUSY_DRAIN=PASS",
            "G10_I_SERVER_DISABLED_DRAIN=PASS",
            "G10_I_SERVER_CLEANUP_NATIVE=PASS_THREE_REAL_SOURCE_METHODS")
    if code!=0 or not all(x in output for x in wanted):
        emit("SERVER_CLEANUP_DIAG",
             "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:1200])
    require(code==0 and all(x in output for x in wanted),
            "SERVER_CLEANUP_REAL_SOURCE_POSITIVE_FAILED")
    negatives=[
        ("LOSE_OWNER_WHEN_BUSY",sigs[0],
         "_jwplcDropPending = _jwplcDropPending || (bool)_client;",
         "_jwplcDropPending = false;",
         "SERVER_BUSY_DISCARDED_LEASE"),
        ("SKIP_CLOSE",sigs[1],
         "_client.cancelStopAsync();",
         "/* no hardware close */",
         "SERVER_DEFERRED_RETRY_DID_NOT_CLOSE"),
        ("STALE_INT_DISABLE",sigs[1],
         "outcome == jwplc_w5500_candidate::StatusReadOutcome::Read",
         "outcome != jwplc_w5500_candidate::StatusReadOutcome::BusBusy",
         "SERVER_STALE_CLOSED_OTHER_OWNER"),
        ("TASK_SKIPS_PENDING",sigs[2],
         "if (_jwplcDropPending && !serviceDeferredDrop()) return;",
         "if (false) return;",
         "SERVER_TASK_ENTERED_WHILE_DROP_BUSY"),
        ("TASK_NOT_READY_DROPS_HANDLE",sigs[2],
         "if (_jwplcDropPending) return;",
         "/* ignore deferred cleanup, unsafe */",
         "SERVER_LINK_DOWN_BUSY_ABANDONED_CLIENT"),
    ]
    for label,sig,a,b,expected in negatives:
        original=bodies[sig]
        require(original.count(a)==1,"SERVER_NEGATIVE_ANCHOR_MISSING_"+label)
        mutant=dict(bodies);mutant[sig]=original.replace(a,b,1)
        code,out_text=run("server_cleanup_negative_"+label,mutant)
        if code==0 or expected not in out_text:
            emit("SERVER_NEGATIVE_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+out_text.strip().replace("\n"," | ")[:600])
        require(code!=0 and expected in out_text,"SERVER_NEGATIVE_NOT_REJECTED_"+label)
    return {"result":"PASS_TWO_SOURCE_METHODS","negatives":len(negatives),
            "end_to_end":False,
            "limitations":"complete task(), serviceServer(), PHY link and RX paths remain open"}


def modbus_cleanup_native(out,files):
    """Compilar tres METODOS C++ del overlay actual y negativos integrados."""
    repro=load(MODULES["repro"],"modbus_cleanup_extract")
    cxx=load("a13_g10_p1g_c1_native_prototype.py","modbus_cleanup_compiler")
    builder=load(MODULES["b2a"],"modbus_cleanup_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_modbus_cleanup_test.cpp"
    mb=files["JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp"]
    signatures=(
        "bool JWPLC_ModbusTCPClientClass::busy() const",
        "void JWPLC_ModbusTCPClientClass::closeSession()",
        "bool JWPLC_ModbusTCPClientClass::serviceDeferredClose()",
    )
    bodies={key:repro.extract(mb,key) for key in signatures}
    def compile_test(tag,versions):
        joined="\n\n".join(versions[key] for key in signatures)+"\n"
        return builder.native(out,tag,joined,cxx)
    rc,log=compile_test("modbus_cleanup_positive",bodies)
    expectations=[
        "G10_I_MB_DEFERRED_CLOSE_BUSY_RETRY=PASS",
        "G10_I_MB_STALE_REUSE_PROTECTED=PASS",
        "G10_I_MB_NO_HANDLE_IDEMPOTENT=PASS",
        "G10_I_MB_CLEANUP_NATIVE=PASS_SOURCE_METHODS_NO_PRODUCT_ADOPTION",
    ]
    if rc!=0 or not all(x in log for x in expectations):
        emit("MB_CLEANUP_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:700])
    require(rc==0 and all(x in log for x in expectations),
            "MB_CLEANUP_REAL_SOURCE_POSITIVE_FAILED")
    variants=[
        ("NO_BUSY_FLAG",signatures[0],
         "if (_jwplcClosePending) return true;",
         "if (_jwplcClosePending) return false;",
         "SPI_BUSY_DROPPED_MODBUS_HANDLE"),
        ("DROP_HANDLE_ON_BUSY",signatures[2],
         "if (!acquireBus(20)) return false; // retain _tcpClient and token",
         "if (!acquireBus(20)) { _tcpClient=EthernetClient(); return false; }",
         "SPI_BUSY_DROPPED_MODBUS_HANDLE"),
        ("SKIP_REAL_CLOSE",signatures[2],
         "_tcpClient.cancelStopAsync();",
         "/* never close socket */",
         "DEFERRED_CLOSE_DID_NOT_DRAIN"),
    ]
    for label,signature,original,replacement,expected in variants:
        version=dict(bodies)
        require(version[signature].count(original)==1,
                "MB_CLEANUP_NEGATIVE_ANCHOR_MISSING_"+label)
        version[signature]=version[signature].replace(original,replacement,1)
        code,output=compile_test("modbus_cleanup_negative_"+label,version)
        if code==0 or expected not in output:
            emit("MB_NEGATIVE_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,
                "MB_CLEANUP_NEGATIVE_NOT_REJECTED_"+label)
    return {"result":"PASS_THREE_SOURCE_METHODS","negatives":len(variants),
            "limitations":"task scheduler and Modbus end-to-end not proven"}


def b02_tcp_rx_native(out, files):
    """Execute real Client RX bodies against current, busy and stale leases."""
    repro=load(MODULES["repro"],"b02_tcp_real_methods")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b02_tcp_real_compiler")
    builder=load(MODULES["b2a"],"b02_tcp_native_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b02_tcp_rx_test.cpp"
    source=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"]
    signatures=(
        "int EthernetClient::available()",
        "int EthernetClient::read(uint8_t *buf, size_t size)",
        "int EthernetClient::read()",
        "int EthernetClient::peek()",
        "int EthernetClient::jwplcReadTcpFastDeferred(",
        "bool EthernetClient::jwplcCommitRxFast()",
    )
    methods=[repro.extract(source,signature) for signature in signatures]
    def run(tag,versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b02_tcp_rx_positive",methods)
    required=("G10_I_B02_TCP_CURRENT_OWNER=PASS",
              "G10_I_B02_TCP_BUSY=PASS",
              "G10_I_B02_TCP_RECYCLED=PASS",
              "G10_I_B02_TCP_INVALID_LEASE=PASS")
    if rc!=0 or not all(marker in log for marker in required):
        emit("B02_TCP_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:800])
    require(rc==0 and all(marker in log for marker in required),
            "B02_TCP_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    variants=(
        ("available_without_guard",0,
         "\tjwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return 0;\n\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;",
         "\t/* mutant: read available without bus or lease */",
         "B02_TCP_IO_WITHOUT_BUS"),
        ("read_without_lease",1,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_TCP_IO_STALE_OWNER"),
        ("fast_without_lease",4,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_TCP_IO_STALE_OWNER"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B02_TCP_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        code,output=run("b02_tcp_negative_"+label,mutant)
        if code==0 or expected not in output:
            emit("B02_TCP_MUTANT_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,
                "B02_TCP_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_TCP_RX","cpp_methods":len(methods),
            "negatives":len(variants),"scope":"TCP Client RX only; TX and public status remain open"}


def b02_tcp_tx_native(out, files):
    """Exercise the five real TCP TX bodies and reject unsafe mutations."""
    repro=load(MODULES["repro"],"b02_tcp_tx_real_methods")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b02_tcp_tx_compiler")
    builder=load(MODULES["b2a"],"b02_tcp_tx_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b02_tcp_tx_test.cpp"
    source=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"]
    signatures=(
        "size_t EthernetClient::write(const uint8_t *buf, size_t size)",
        "int EthernetClient::beginWriteAsync(const uint8_t *buf, size_t size)",
        "int EthernetClient::pollWriteAsync()",
        "bool EthernetClient::writeAsyncInProgress() const",
        "void EthernetClient::cancelWriteAsync()",
    )
    methods=[repro.extract(source,signature) for signature in signatures]
    def run(tag,versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b02_tcp_tx_positive",methods)
    required=("CURRENT_OWNER","BUSY_RETRY","RECYCLED",
              "TIMEOUT_BEFORE_SEND","TIMEOUT_AFTER_SEND","TRANSMISSION_FAILURE")
    if rc!=0 or not all("G10_I_B02_TCP_TX_"+name+"=PASS" in log for name in required):
        emit("B02_TCP_TX_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:800])
    require(rc==0 and all("G10_I_B02_TCP_TX_"+name+"=PASS" in log for name in required),
            "B02_TCP_TX_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    variants=(
        ("poll_without_guard",2,
         "    jwplc_w5500_candidate::BusScope bus(20);\n    if (!bus) return 0; // retain the SEND state and its buffer\n    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {",
         "    if (false) {", "B02_TX_IO_WITHOUT_BUS"),
        ("poll_without_lease",2,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false", "B02_TX_STALE_SENT_TO_NEW_OWNER"),
        ("retain_borrowed_buffer",4,
         "    if (_writeAsyncState == 1) {",
         "    if (false) {", "B02_TX_TIMEOUT_BORROWED_BUFFER_RETAINED"),
        ("ignore_begin_failure",0,
         "        if (state != 1) break;",
         "        if (false) break;", "B02_TX_BEGIN_FAILURE_NOT_REPORTED"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B02_TCP_TX_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        code,output=run("b02_tcp_tx_negative_"+label,mutant)
        if code==0 or expected not in output:
            emit("B02_TCP_TX_MUTANT_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,
                "B02_TCP_TX_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_TCP_TX","cpp_methods":len(methods),
            "negatives":len(variants),
            "scope":"TCP write/async SEND; other TCP helpers and public status remain open"}


def b02_client_remaining_native(out, files):
    """Run Client capacity, flush and endpoint code with real lease scopes."""
    repro=load(MODULES["repro"],"b02_remaining_repro")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b02_remaining_compiler")
    builder=load(MODULES["b2a"],"b02_remaining_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b02_client_remaining_test.cpp"
    source=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp"]
    signatures=(
        "int EthernetClient::availableForWrite(void)",
        "int EthernetClient::beginFlushAsync()",
        "int EthernetClient::pollFlushAsync()",
        "bool EthernetClient::flushAsyncInProgress() const",
        "void EthernetClient::cancelFlushAsync()",
        "void EthernetClient::flush()",
        "uint16_t EthernetClient::localPort()",
        "IPAddress EthernetClient::remoteIP()",
        "uint16_t EthernetClient::remotePort()",
        "bool EthernetClient::writeAsyncInProgress() const",
        "int EthernetClient::pollWriteAsync()",
        "void EthernetClient::cancelWriteAsync()",
    )
    methods=[repro.extract(source,sig) for sig in signatures]
    def run(tag,versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b02_remaining_positive",methods)
    required=("CURRENT","BUSY","RECYCLED","TIMEOUT_CANCEL",
              "SEND_FAILURE","COOPERATIVE_WAIT")
    if rc!=0 or not all("G10_I_B02_REMAIN_"+x+"=PASS" in log for x in required):
        emit("B02_REMAIN_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:800])
    require(rc==0 and all("G10_I_B02_REMAIN_"+x+"=PASS" in log for x in required),
            "B02_REMAIN_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    guard="""    jwplc_w5500_candidate::BusScope bus(20);
    if (!bus) {
        if ((uint32_t)(millis() - _flushStartedAtMs) < _timeout) return 0;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
    }
    if (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) {
        _sockindex = MAX_SOCK_NUM;
        _flushPending = false;
        _flushStartedAtMs = 0;
        return -1;
    }
"""
    variants=(
        ("capacity_without_bus",0,
         "\tjwplc_w5500_candidate::BusScope bus(20);\n"
         "\tif (!bus || _sockindex >= MAX_SOCK_NUM) return 0;\n"
         "\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;",
         "\tif (_sockindex >= MAX_SOCK_NUM) return 0;",
         "B02_REMAIN_IO_WITHOUT_BUS"),
        ("endpoint_without_lease",6,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_REMAIN_STALE_SOCKET_ACCESS"),
        ("flush_without_bus",2,guard,"", "B02_REMAIN_IO_WITHOUT_BUS"),
        ("flush_without_lease",2,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_REMAIN_STALE_FLUSH_ACCESS"),
        ("flush_busy_timeout_ignored",2,
         "(uint32_t)(millis() - _flushStartedAtMs) < _timeout",
         "true","B02_REMAIN_BUSY_TIMEOUT_NOT_HONORED"),
        ("send_failure_ignored",2,
         "if (writeState < 0) {","if (false) {",
         "B02_REMAIN_SEND_FAILURE_MASKED"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B02_REMAIN_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        code,output=run("b02_remaining_negative_"+label,mutant)
        if code==0 or expected not in output:
            emit("B02_REMAIN_MUTANT_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:450])
        require(code!=0 and expected in output,"B02_REMAIN_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_CLIENT_REMAINING","cpp_methods":len(methods),
            "newly_guarded":5,"negatives":len(variants),
            "scope":"capacity, flush, endpoint reads; B01 public status and B04/B05 ownership open"}


def b04_b05_server_native(out, files):
    """Run real Server scan/broadcast and constructor against slot epochs."""
    repro=load(MODULES["repro"],"b05_server_repro")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b05_server_compiler")
    builder=load(MODULES["b2a"],"b05_server_builder")
    header=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h"]
    ctor=repro.extract(header,"EthernetClient(uint8_t s)")
    template=(ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b04_b05_server_test.cpp").read_text(encoding="utf-8-sig")
    require(template.count("__REAL_CLIENT_CTOR__")==1,"B05_CTOR_TEMPLATE_DRIFT")
    test=out/"b05_server_native_test.cpp"
    test.write_text(template.replace("__REAL_CLIENT_CTOR__",ctor),encoding="utf-8")
    builder.TEST=test
    source=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetServer.cpp"]
    signatures=("void EthernetServer::begin()",
                "EthernetClient EthernetServer::available()",
                "EthernetServer::operator bool()",
                "size_t EthernetServer::write(const uint8_t *buffer, size_t size)")
    methods=[repro.extract(source,s) for s in signatures]
    def run(tag,versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b05_server_positive",methods)
    required=("LISTENER_REOPEN","BUSY","RECYCLED_RESET",
              "BROADCAST_PARTIAL_RECYCLED","NO_PEER_TIMEOUT")
    if rc!=0 or not all("G10_I_B05_"+name+"=PASS" in log for name in required):
        emit("B05_SERVER_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:800])
    require(rc==0 and all("G10_I_B05_"+name+"=PASS" in log for name in required),
            "B05_SERVER_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    variants=(
        ("bool_without_bus",2,
         "\tjwplc_w5500_candidate::BusScope bus(20);\n"
         "\tif (!bus) return false; // public bool cannot encode BUS_BUSY",
         "", "B05_W5500_OUTSIDE_BUS"),
        ("bool_without_listener_lease",2,
         "jwplc_w5500_candidate::listenerMatchesLocked(\n"
         "                i, _port, jwplc_w5500_candidate::currentSocketTokenLocked(i))",
         "true", "B05_STALE_LISTENER_STATUS_READ"),
        ("write_without_bus",3,
         "        jwplc_w5500_candidate::BusScope bus(20);\n"
         "        if (!bus) return 0; // no listener or socket inference on SPI_BUSY",
         "", "B05_W5500_OUTSIDE_BUS"),
        ("write_without_listener_lease",3,
         "!jwplc_w5500_candidate::listenerMatchesLocked(i, _port, token)",
         "false", "B05_STALE_LISTENER_STATUS_READ"),
        ("write_with_lock_across_tx",3,
         "    // Each write may wait for SEND_OK. Never hold the server scan lock here.",
         "    jwplc_w5500_candidate::BusScope heldAcrossWrite(20);\n"
         "    // mutant: keep SPI while waiting for SEND_OK.",
         "B05_BROADCAST_WRITE_WITH_BUS"),
        ("ignore_partial_failure",3,
         "return count ? minimumWritten : 0;",
         "return count ? size : 0;", "B05_BROADCAST_PARTIAL_RESULT_WRONG"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B05_SERVER_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        code,output=run("b05_server_negative_"+label,mutant)
        if code==0 or expected not in output:
            emit("B05_SERVER_MUTANT_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,"B05_SERVER_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_SERVER_SNAPSHOT","cpp_methods":len(methods),
            "real_client_constructor":True,"negatives":len(variants),
            "scope":"Server listener bool/broadcast; BUS_BUSY bool policy and full B04/B05 remain REVIEW"}


def b02_udp_io_native(out, files):
    """Execute real UDP RX/TX bodies with bus and lease assertions."""
    repro=load(MODULES["repro"],"b02_real_methods")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b02_real_compiler")
    builder=load(MODULES["b2a"],"b02_native_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b02_udp_io_test.cpp"
    source=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"]
    signatures=(
        "size_t EthernetUDP::write(const uint8_t *buffer, size_t size)",
        "int EthernetUDP::parsePacket()",
        "int EthernetUDP::read()",
        "int EthernetUDP::read(unsigned char *buffer, size_t len)",
        "int EthernetUDP::peek()",
        "int EthernetUDP::jwplcReadPacketFastDeferred(",
        "bool EthernetUDP::jwplcCommitRxFast()",
    )
    methods=[repro.extract(source,signature) for signature in signatures]
    def run(tag,versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b02_udp_io_positive",methods)
    required=("G10_I_B02_UDP_CURRENT_OWNER=PASS",
              "G10_I_B02_UDP_BUSY=PASS",
              "G10_I_B02_UDP_STALE=PASS")
    if rc!=0 or not all(marker in log for marker in required):
        emit("B02_UDP_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:800])
    require(rc==0 and all(marker in log for marker in required),
            "B02_UDP_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    variants=(
        ("write_without_guard",0,
         "\tjwplc_w5500_candidate::BusScope bus(20);\n\tif (!bus) return 0;\n\tif (!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)) return 0;",
         "\t/* mutant: issue write without bus or lease */",
         "B02_UDP_IO_WITHOUT_BUS"),
        ("parse_without_lease",1,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_UDP_IO_STALE_OWNER"),
        ("read_without_lease",3,
         "!jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "false","B02_UDP_IO_STALE_OWNER"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B02_UDP_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        code,output=run("b02_udp_negative_"+label,mutant)
        if code==0 or expected not in output:
            emit("B02_UDP_MUTANT_DIAG_"+label,
                 "EXIT_"+str(code)+"_"+output.strip().replace("\n"," | ")[:500])
        require(code!=0 and expected in output,
                "B02_UDP_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_UDP_IO","cpp_methods":len(methods),
            "negatives":len(variants),"scope":"UDP data paths only; TCP still open"}


def b03_deferred_close_native(out, files):
    """Real UDP/DNS/DHCP methods plus real lease queue, with live mutants."""
    repro=load(MODULES["repro"],"b03_real_methods")
    compiler=load("a13_g10_p1g_c1_native_prototype.py","b03_real_compiler")
    builder=load(MODULES["b2a"],"b03_native_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_b03_deferred_close_test.cpp"
    dns=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp"]
    dhcp=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dhcp.cpp"]
    udp=files["JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp"]
    dhcp_async=(ROOT/"JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/DhcpAsyncControl.cpp").read_text(encoding="utf-8-sig")
    signatures=(
        ("void EthernetUDP::stop()",udp),
        ("void DNSClient::finishResolveAsync(int result)",dns),
        ("void DNSClient::cancelResolveAsync()",dns),
        ("void DhcpClass::finishAsync()",dhcp),
        ("uint8_t EthernetUDP::begin(uint16_t port)",udp),
        ("int DNSClient::beginResolveAsync(",dns),
        ("int DhcpClass::beginWithDHCPAsync(",dhcp),
        ("bool DhcpClass::startLeaseMaintenance(uint8_t mode)",dhcp_async),
        ("uint8_t EthernetUDP::beginMulticast(IPAddress ip, uint16_t port)",udp),
        ("int DNSClient::pollResolveAsync()",dns),
        ("int DhcpClass::pollDHCP()",dhcp),
        ("int DhcpClass::request_DHCP_lease()",dhcp),
        ("int DhcpClass::beginWithDHCP(uint8_t *mac, unsigned long timeout, unsigned long responseTimeout)",dhcp),
        ("void DhcpClass::cancelDHCP()",dhcp_async),
    )
    methods=[repro.extract(source,signature) for signature,source in signatures]
    require(all("stop()" in body for body in methods[1:4]),
            "B03_SOURCE_TERMINATION_CONTRACT_DRIFT")
    def run(tag, versions):
        return builder.native(out,tag,"\n\n".join(versions)+"\n",compiler)
    rc,log=run("b03_deferred_close_positive",methods)
    required=(
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
    if rc!=0 or not all(marker in log for marker in required):
        emit("B03_SOURCE_DIAG","EXIT_"+str(rc)+"_"+log.strip().replace("\n"," | ")[:1100])
    require(rc==0 and all(marker in log for marker in required),
            "B03_SOURCE_CORRECTION_NOT_DEMONSTRATED")
    variants=(
        ("drop_ticket",0,
         "(void)jwplc_w5500_candidate::requestDeferredClose(_jwplcSocketToken);",
         "/* mutant: discard durable ticket */",
         "DNS_FINISH_TICKET_NOT_DRAINED"),
        ("stop_without_owner",0,
         "jwplc_w5500_candidate::matchesLocked(_jwplcSocketToken)",
         "true", "B03_DOUBLE_OR_UNKNOWN_CLOSE"),
        ("dns_rebegin_on_busy",5,
         "if (iUdp.begin(1024 + (millis() & 0xF)) != 1)",
         "if (false)", "DNS_REBEGIN_REUSED_BUSY_SOCKET"),
        ("dhcp_renew_on_busy",7,
         "if (_dhcpUdpSocket.begin(DHCP_CLIENT_PORT) == 0)",
         "if (false)", "DHCP_MAINT_REUSED_BUSY_SOCKET"),
        ("multicast_without_bus",8,
         "if (!bus) return 0; // preserve previous owner on BUS_BUSY",
         "if (false) return 0;", "B03_BEGIN_WITHOUT_BUS"),
        ("dns_timeout_without_finish",9,
         "finishResolveAsync(TIMED_OUT);",
         "/* mutant: timeout returns without releasing socket */",
         "DNS_TIMEOUT_LOST_OWNER"),
        ("dhcp_timeout_without_finish",10,
         "if ((now - _asyncStartTime) > _timeout)",
         "if (false)", "DHCP_TIMEOUT_LOST_OWNER"),
        ("dhcp_cancel_without_stop",13,
         "_dhcpUdpSocket.stop();",
         "/* mutant: cancellation leaves socket open */",
         "DHCP_CANCEL_NOT_DRAINED"),
        ("dns_build_error_without_finish",5,
         "ret = BuildRequest(aHostname);\n\tif (ret == 0) {\n\t\tfinishResolveAsync(-11);",
         "ret = BuildRequest(aHostname);\n\tif (ret == 0) {\n\t\t/* mutant: discard opened UDP socket */",
         "DNS_BUILD_ERROR_NOT_DRAINED"),
    )
    for label,index,anchor,replacement,expected in variants:
        require(methods[index].count(anchor)==1,"B03_MUTANT_ANCHOR_"+label)
        mutant=list(methods)
        mutant[index]=mutant[index].replace(anchor,replacement,1)
        rc,log=run("b03_negative_"+label,mutant)
        if rc==0 or expected not in log:
            emit("B03_MUTANT_DIAG_"+label,log.strip().replace("\n"," | ")[:500])
        require(rc!=0 and expected in log,"B03_MUTANT_NOT_REJECTED_"+label)
    return {"status":"PASS_SOURCE_BACKED_DEFERRED_CLOSE",
            "cpp_methods":len(methods),
            "negatives":len(variants),
            "safe_for_adoption":False,
            "limitations":"host bus/hardware stub; DHCP packet parsing and other B01-B08 paths remain open"}


def status_probe_native(out):
    """Una fase DEL MISMO G10-I; no es un nuevo gate manual."""
    cxx=load("a13_g10_p1g_c1_native_prototype.py","g10i_cxx_status")
    builder=load(MODULES["b2a"],"g10i_status_builder")
    builder.TEST=ROOT/"tools/alpha13/candidates/g10_p1g_c/native_g10_i_status_probe_test.cpp"
    code,log=builder.native(out,"status_probe","",cxx)
    expected=(
        "G10_I_STATUS_READ_CLOSED_ESTABLISHED=PASS",
        "G10_I_STATUS_SPI_BUSY_NO_FALSE_CLOSED=PASS",
        "G10_I_STATUS_REUSE_RESET_STALE=PASS",
        "G10_I_STATUS_NATIVE=PASS_TRISTATE_PRIMITIVE_ONLY_ADOPTION_BLOCKED",
    )
    if code != 0 or not all(item in log for item in expected):
        emit("STATUS_PROBE_DIAG",log.strip().replace("\n"," | ")[:800])
    require(code==0 and all(item in log for item in expected),
            "NATIVE_TRISTATE_STATUS_PROBE_FAILED")
    return {"result":"PASS_PRIMITIVE_ONLY","log":"status_probe/native.log",
            "qualified_for_adoption":False}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--ci-standalone",action="store_true",
                        help="CI: solo verificación de branch ancestor, no acredita gate local")
    parser.add_argument("--require-ready",action="store_true",
                        help="Falla mientras existan bloqueos de integración")
    parser.add_argument("--stage-platform",type=Path,
                        help="Copiar package a ruta TEMPORAL EXTERNA inexistente y aplicar overlay")
    opts=parser.parse_args()
    runid=("g10_i_"+datetime.now().strftime("%Y%m%d_%H%M%S")
           +"_"+uuid.uuid4().hex[:8])
    out=ROOT/"tools/alpha13/results"/runid
    out.mkdir(parents=True,exist_ok=False)
    m={"schema":"A13-G10-INTEGRATED-PREP-1","run_id":runid,
       "phase":"G10-I-PREP","status":"REVIEW_STOP",
       "product_modified":False,"hardware_touched":False,
       "arduino_build":False,"archives_frozen":False,
       "candidate_adopted":False}
    exit_code=2
    try:
        head=preflight(opts.ci_standalone)
        m["archives_frozen"]=True
        files=compose()
        shas=write_evidence(out,head,files)
        m["status_probe"]=status_probe_native(out)
        emit("TRISTATE_STATUS_NATIVE","PASS_PRIMITIVE_ONLY")
        m["modbus_cleanup_probe"]=modbus_cleanup_native(out,files)
        emit("MODBUS_CLEANUP_NATIVE","PASS_THREE_SOURCE_METHODS_3_NEGATIVES")
        m["modbus_server_cleanup_probe"]=server_cleanup_native(out,files)
        emit("MODBUS_SERVER_CLEANUP_NATIVE","PASS_THREE_SOURCE_METHODS_5_NEGATIVES")
        m["b02_udp_io"]=b02_udp_io_native(out,files)
        emit("B02_UDP_IO","PASS_SOURCE_BACKED_7_METHODS_3_NEGATIVES")
        m["b02_tcp_rx"]=b02_tcp_rx_native(out,files)
        emit("B02_TCP_RX","PASS_SOURCE_BACKED_6_METHODS_3_NEGATIVES")
        m["b02_tcp_tx"]=b02_tcp_tx_native(out,files)
        emit("B02_TCP_TX","PASS_SOURCE_BACKED_5_METHODS_4_NEGATIVES")
        m["b02_client_remaining"]=b02_client_remaining_native(out,files)
        emit("B02_CLIENT_REMAINING","PASS_SOURCE_BACKED_12_METHODS_6_NEGATIVES")
        m["b02_nonphysical_status"]="REVIEW_B01_B04_B05_DEPENDENCIES"
        emit("B02_NONPHYSICAL",m["b02_nonphysical_status"])
        m["b04_b05_server"]=b04_b05_server_native(out,files)
        emit("B04_B05_SERVER","PASS_SOURCE_BACKED_4_METHODS_1_CTOR_6_NEGATIVES")
        m["b04_b05_status"]="REVIEW_B01_BUSY_POLICY_AND_RESET_OWNERSHIP"
        emit("B04_B05",m["b04_b05_status"])
        m["b03_dns_dhcp_shutdown"]=b03_deferred_close_native(out,files)
        emit("B03_DNS_DHCP_SHUTDOWN","PASS_SOURCE_BACKED_DEFERRED_CLOSE_9_NEGATIVES")
        if opts.stage_platform is not None:
            staged=stage_platform(opts.stage_platform,files,shas)
            m["staged_sources"]=staged
            m["staged_platform"]=str(opts.stage_platform.resolve())
            emit("ARDUINO_PLATFORM_EXPORT","STAGED_NOT_COMPILED")
        m.update({"head":head,"overlay_files":shas,
                  "blockers":BLOCKERS,
                  "status":"B03_CANDIDATE_REVIEW_BLOCKED"})
        emit("SOURCE_OVERLAY","COMPOSED")
        emit("OVERLAY_FILES",len(files))
        emit("FROZEN_ARCHIVES",5)
        emit("BLOCKERS",len(BLOCKERS))
        emit("ARDUINO_BUILD","NOT_RUN")
        emit("PRODUCT_CHANGED","NO")
        emit("STATUS",m["status"])
        exit_code=3 if opts.require_ready else 0
    except Exception as exc:
        m["reason"]=repr(exc)
        emit("STATUS","REVIEW_STOP")
        emit("REASON",repr(exc))
    finally:
        m["exit_code"]=exit_code
        (out/"MANIFEST.json").write_text(json.dumps(m,indent=2,ensure_ascii=False)+"\n",
                                         encoding="utf-8")
        (out/"SUMMARY.log").write_text(
          "PHASE=G10-I-PREP\nSTATUS="+m["status"]+
          "\nCLASSIFICATION="+("PRECONDITION" if m["status"]=="REVIEW_STOP" else "DESIGN")+
          "\nARDUINO_BUILD=NOT_RUN\nPRODUCT_CHANGED=NO\n",
          encoding="utf-8")
        emit("RESULTS",out)
    return exit_code

if __name__=="__main__":
    sys.exit(main())

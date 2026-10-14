#!/usr/bin/env python3
"""Check the composed B03 source path; the native gate proves behavior.

The product baseline still reproduces the defect. A green source check alone
never qualifies adoption: the integrated native gate executes real bodies and
rejects a mutant that removes the durable close ticket.
"""
from __future__ import annotations
import importlib.util
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
ETH = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"

def require(condition: bool, reason: str) -> None:
    if not condition:
        raise RuntimeError(reason)

def main() -> int:
    path = ROOT / "tools/alpha13/gates/a13_g10_integrated.py"
    spec = importlib.util.spec_from_file_location("g10_b03_compose", path)
    require(spec is not None and spec.loader is not None, "COMPOSE_MISSING")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    files = module.compose()
    stop = files[ETH + "EthernetUdp.cpp"]
    tick = files[ETH + "JWPLC_Ethernet.cpp"]
    begin = files[ETH + "socket.cpp"]
    lease = files[ETH + "JWPLC_W5500_SafeAccess.cpp"]
    def fixed(u: str, t: str, b: str, l: str) -> bool:
        return ("requestDeferredClose(_jwplcSocketToken)" in u and
                "JWPLC_EthernetClass::service()" in t and
                "EthernetUDP::jwplcServiceDeferredClose()" in t and
                "serviceDeferredClose(" in b and
                "allocationAvailableLocked()" in b and
                "EthernetClass::socketBeginMulticast" in b and
                "EthernetUDP::beginMulticast" in u and
                "slot.allocation == ticket" in l and
                "close(index, context);" in l)
    require(fixed(stop, tick, begin, lease), "B03_COMPOSED_SOURCE_PATH_INCOMPLETE")
    require(not fixed(stop.replace("requestDeferredClose(_jwplcSocketToken)",
                                   "discardTicket(_jwplcSocketToken)"),
                      tick, begin, lease), "B03_NO_TICKET_MUTANT_ACCEPTED")
    require(not fixed(stop, tick.replace("jwplcServiceDeferredClose()", "skipClose()"),
                      begin, lease), "B03_NO_AUTOLOAD_MUTANT_ACCEPTED")
    print("G10_I_B03_SOURCE_CONTRACT=PASS_COMPOSED_TICKET_AND_AUTOLOAD_2_MUTANTS")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())

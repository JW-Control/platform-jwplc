#!/usr/bin/env python3
"""Alpha13 G10-P1G-A: inventario reproducible read-only de mutex/call paths.

Este gate NO prueba que 81 metodos sean defectuosos, ni acepta un fix.
No modifica el package, no compila firmware y no accede a hardware.
"""
from __future__ import annotations

import argparse
import csv
from datetime import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
import sys
import uuid

ROOT = Path(__file__).resolve().parents[3]
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
P1F_HEAD = "f5d8d32369ecbc64163e65ed3e52e0a4f88733f6"
P1F_RUN = "g10_p1f_20261012_162335_c18278de"
ETH = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/"
MB = "JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/"
SOURCES = {
    "RUNTIME": ETH + "JWPLC_Ethernet.cpp",
    "SERVER_MODBUS": MB + "JWPLC_ModbusTCP.cpp",
    "CLIENT_MODBUS": MB + "JWPLC_ModbusTCP_Client.cpp",
    "CLIENT": ETH + "EthernetClient.cpp",
    "SERVER": ETH + "EthernetServer.cpp",
    "UDP": ETH + "EthernetUdp.cpp",
    "SOCKET": ETH + "socket.cpp",
    "BACKEND": ETH + "JWPLC_W5x00_Ethernet.cpp",
    "RUNTIME_H": ETH + "JWPLC_Ethernet.h",
    "BACKEND_H": ETH + "JWPLC_W5x00_Ethernet.h",
    "BOARDS": "JWPLC/2.1.0/boards.txt",
    "SPI_MUTEX": "JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_spi_bus.cpp",
    "PERIPHERALS": "JWPLC/2.1.0/cores/jwcontrol/peripherals_init.cpp",
}
BASE_METHODS = [42, 63, 44, 34, 6, 20, 26, 22]
BASE_GUARDS = [11, 4, 4]
BASE_DIRECT = [24, 5, 14, 20, 18]
METHOD_PATTERN = re.compile(
    r"^[ \t]*(?:[A-Za-z_][A-Za-z_0-9:*&<>]*\s+){1,3}"
    r"([A-Za-z_]\w*)::([A-Za-z_]\w*)\s*\(", re.M)
DIRECT = re.compile(r"\b(?:Ethernet|W5100)\s*\.\s*\w+\s*\("
                    r"|\bSPI\s*\.\s*(?:beginTransaction|transfer)\s*\(")
LOCK = re.compile(r"\b(?:acquireBus|jwplcSPI_acquire)\s*\(")


def say(key, value):
    print("G10_P1G_A_" + key + "=" + str(value), flush=True)


def need(ok, why):
    if not ok:
        raise RuntimeError(why)


def sha(path):
    digest = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git(*args):
    p = subprocess.run(["git", "-c", "core.quotepath=false", *args],
                       cwd=ROOT, capture_output=True, text=True,
                       encoding="utf-8", errors="replace", timeout=25)
    need(p.returncode == 0, "GIT_" + "_".join(args))
    return p.stdout.strip()


def balanced(src, open_at, start, end):
    depth = 0
    for i in range(open_at, len(src)):
        if src[i] == start:
            depth += 1
        elif src[i] == end:
            depth -= 1
            if depth == 0:
                return i
    raise RuntimeError("UNBALANCED_" + start + "_" + end)


def methods(src):
    """Definiciones Class::method de fuente, sin constructores ni inline."""
    found = []
    for match in METHOD_PATTERN.finditer(src):
        close = balanced(src, src.index("(", match.start()), "(", ")")
        j = close + 1
        while j < len(src) and src[j].isspace():
            j += 1
        if src.startswith("const", j):
            j += 5
            while j < len(src) and src[j].isspace():
                j += 1
        if j >= len(src) or src[j] != "{":
            continue
        close_body = balanced(src, j, "{", "}")
        body = src[j:close_body + 1]
        name = match.group(1) + "::" + match.group(2)
        found.append({
            "name": name,
            "line": src.count("\n", 0, match.start()) + 1,
            "direct": bool(DIRECT.search(body)),
            "local_guard": bool(LOCK.search(body)) and
                match.group(2) != "acquireBus",
            "body": body,
        })
    return found


def check(src):
    keys = list(SOURCES)[:8]
    parsed = {key: methods(src[key]) for key in keys}
    actual_methods = [len(parsed[key]) for key in keys]
    need(actual_methods == BASE_METHODS,
         "METHOD_COUNT_DRIFT_" + repr(actual_methods))
    guards = [sum(x["local_guard"] for x in parsed[key])
              for key in keys[:3]]
    need(guards == BASE_GUARDS,
         "EXPLICIT_GUARD_COUNT_DRIFT_" + repr(guards))
    directs = [sum(x["direct"] and not x["local_guard"]
                   for x in parsed[key]) for key in keys[3:]]
    need(directs == BASE_DIRECT,
         "LOWER_DIRECT_COUNT_DRIFT_" + repr(directs))

    runtime = src["RUNTIME"]
    probe = between(runtime, "bool JWPLC_EthernetClass::probeHardware()",
                    "void JWPLC_EthernetClass::service()")
    reset = between(runtime,
                    "void JWPLC_EthernetClass::resetHardwareIfNeeded()",
                    "bool JWPLC_EthernetClass::prepareSPI()")
    need("resetHardwareIfNeeded();" in probe and
         "if (!acquireBus(500))" in probe and
         probe.index("resetHardwareIfNeeded();") <
         probe.index("if (!acquireBus(500))"),
         "RESET_BEFORE_MUTEX_ORDER_DRIFT")
    need("if (_resetPin == 255)" in reset and
         "digitalWrite(_resetPin, LOW);" in reset and
         "digitalWrite(_resetPin, HIGH);" in reset,
         "RESET_GPIO_PATH_DRIFT")
    need("#define JWPLC_ETH_RESET_PIN 255" in src["RUNTIME_H"],
         "RESET_DEFAULT_NOT_255")
    need("-DJWPLC_ETH_RESET_PIN" not in src["BOARDS"],
         "BOARD_RESET_OVERRIDE_NEEDS_REVIEW")
    need("xSemaphoreCreateMutex()" in src["SPI_MUTEX"] and
         "xSemaphoreCreateRecursiveMutex()" not in src["SPI_MUTEX"],
         "FREERTOS_MUTEX_KIND_CHANGED")
    need("SPI.beginTransaction(SPI_ETHERNET_SETTINGS)" in src["SOCKET"] and
         "jwplcSPI_acquire(" not in src["SOCKET"],
         "SOCKET_SPI_CONTRACT_CHANGED")
    need("jwplcSPI_acquire(" not in src["CLIENT"] and
         "jwplcSPI_acquire(" not in src["SERVER"] and
         "jwplcSPI_acquire(" not in src["UDP"],
         "DIRECT_API_MUTEX_CONTRACT_CHANGED")
    need("virtual operator bool() { return _sockindex < MAX_SOCK_NUM; }"
         in src["BACKEND_H"],
         "CLIENT_BOOL_SOFTWARE_ONLY_CONTRACT_CHANGED")
    need("server_port[MAX_SOCK_NUM]" in src["SERVER"] and
         "server_port[i] == _port" in src["SERVER"],
         "LISTENER_MAP_CHANGED")

    def body(key, name):
        xs = [row["body"] for row in parsed[key] if row["name"] == name]
        need(len(xs) == 1, "MISSING_METHOD_" + name)
        return xs[0]

    # Control positivo: nuestras rutas protegidas.
    need("acquireBus(20)" in body(
            "SERVER_MODBUS", "JWPLC_ModbusTCPClass::serviceServer") and
         "_server.available()" in body(
            "SERVER_MODBUS", "JWPLC_ModbusTCPClass::serviceServer"),
         "MODBUS_SERVER_WRAPPED_PATH_CHANGED")
    need("acquireBus(20)" in body(
            "CLIENT_MODBUS", "JWPLC_ModbusTCPClientClass::serviceConnection") and
         "_tcpClient.connected()" in body(
            "CLIENT_MODBUS", "JWPLC_ModbusTCPClientClass::serviceConnection"),
         "MODBUS_CLIENT_WRAPPED_PATH_CHANGED")

    # Control de entrada sin wrapper: los metodos reales, no un mock.
    need("Ethernet.socketStatus(" in body(
            "CLIENT", "EthernetClient::connected") and
         not LOCK.search(body("CLIENT", "EthernetClient::connected")),
         "RAW_CLIENT_ENTRY_CHANGED")
    need("Ethernet.socketBegin(" in body(
            "SERVER", "EthernetServer::begin") and
         not LOCK.search(body("SERVER", "EthernetServer::begin")),
         "RAW_SERVER_ENTRY_CHANGED")
    need("Ethernet.socketBegin(" in body(
            "UDP", "EthernetUDP::begin") and
         not LOCK.search(body("UDP", "EthernetUDP::begin")),
         "RAW_UDP_ENTRY_CHANGED")
    return parsed, guards, directs


def between(text, left, right):
    need(text.count(left) == 1 and text.count(right) == 1,
         "NON_UNIQUE_BOUND_" + left[:40])
    a = text.index(left)
    b = text.index(right, a + len(left))
    need(b > a, "REVERSED_BOUNDS")
    return text[a:b]


def sources():
    return {key: (ROOT / rel).read_text(encoding="utf-8-sig")
            for key, rel in SOURCES.items()}


def negative_tests(orig):
    check(orig)
    tests = [
        ("MUTEX_RECURSIVE", "SPI_MUTEX",
         "xSemaphoreCreateMutex()", "xSemaphoreCreateRecursiveMutex()"),
        ("GPIO4_DEFAULT", "RUNTIME_H",
         "#define JWPLC_ETH_RESET_PIN 255",
         "#define JWPLC_ETH_RESET_PIN 4"),
        ("RESET_ORDER", "RUNTIME",
         "    resetHardwareIfNeeded();\n\n    if (!acquireBus(500))",
         "    if (!acquireBus(500))\n\n    resetHardwareIfNeeded();"),
        ("RAW_CLIENT_GUARD", "CLIENT",
         "uint8_t EthernetClient::connected()\n{",
         "uint8_t EthernetClient::connected()\n{\n    jwplcSPI_acquire(20);"),
    ]
    for label, key, before, after in tests:
        need(orig[key].count(before) == 1, "SELFTEST_ANCHOR_" + label)
        mutated = dict(orig)
        mutated[key] = orig[key].replace(before, after, 1)
        try:
            check(mutated)
        except RuntimeError:
            say("SELFTEST_" + label, "PASS_REJECTED")
        else:
            raise RuntimeError("NEGATIVE_NOT_CAUGHT_" + label)
    say("SELFTEST", "PASS_4_NEGATIVES")
    return 0


def validate_repo(ci):
    need(Path.cwd().resolve() == ROOT and
         Path(git("rev-parse", "--show-toplevel")).resolve() == ROOT,
         "REPOSITORY_ROOT_REQUIRED")
    head = git("rev-parse", "HEAD")
    need(subprocess.run(
         ["git", "merge-base", "--is-ancestor", P1F_HEAD, head],
         cwd=ROOT, capture_output=True).returncode == 0,
         "P1F_HEAD_NOT_ANCESTOR")
    if not ci:
        need(git("branch", "--show-current") == BRANCH, "WRONG_BRANCH")
        need(head == git("rev-parse", "refs/remotes/origin/" + BRANCH),
             "LOCAL_REMOTE_HEAD_MISMATCH")
        p = ROOT / "tools/alpha13/results" / P1F_RUN / "MANIFEST.json"
        need(p.is_file(), "P1F_LOCAL_MANIFEST_MISSING")
        m = json.loads(p.read_text(encoding="utf-8-sig"))
        need(m.get("run_id") == P1F_RUN and
             m.get("schema") == "A13-G10-P1F-READONLY-1" and
             m.get("status") ==
             "SOCKET_EPOCH_BASELINE_CAPTURED_ADOPTION_BLOCKED",
             "P1F_LOCAL_EVIDENCE_UNQUALIFIED")
    need(not git("diff", "--name-only") and
         not git("diff", "--cached", "--name-only"), "TRACKED_DIRTY")
    return head


def check_frozen():
    spec = importlib.util.spec_from_file_location(
        "a13_g10_p1f_frozen", ROOT /
        "tools/alpha13/gates/a13_g10_p1f_socket_epoch_baseline.py")
    need(spec is not None and spec.loader is not None,
         "P1F_REFERENCE_NOT_IMPORTABLE")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    frozen = mod.FROZEN
    need(len(frozen) == 5, "FROZEN_SET_CHANGED")
    for rel, expected in frozen.items():
        need((ROOT / rel).is_file() and sha(ROOT / rel) == expected,
             "FROZEN_SHA_MISMATCH_" + rel)
    return len(frozen)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--ci-standalone", action="store_true")
    args = ap.parse_args()
    src = sources()
    if args.selftest:
        return negative_tests(src)
    runid = ("g10_p1g_a_" + datetime.now().strftime("%Y%m%d_%H%M%S")
             + "_" + uuid.uuid4().hex[:8])
    out = ROOT / "tools/alpha13/results" / runid
    out.mkdir(parents=True, exist_ok=False)
    manifest = {
        "schema": "A13-G10-P1G-A-CALLPATHS-1",
        "run_id": runid,
        "status": "REVIEW_STOP",
        "product_mutation": False, "git_mutation": False,
        "compile": False, "hardware_access": False,
        "candidate_adoption": False,
    }
    try:
        head = validate_repo(args.ci_standalone)
        archives = check_frozen()
        parsed, guards, directs = check(src)
        matrix = out / "CALL_PATHS.csv"
        with matrix.open("w", encoding="utf-8", newline="") as f:
            w = csv.writer(f)
            w.writerow(["source", "line", "method", "direct_backend_or_spi",
                        "local_mutex_acquire", "classification"])
            for key in list(SOURCES)[:8]:
                for row in parsed[key]:
                    classification = (
                        "EXPLICIT_LOCAL_GUARD" if row["local_guard"]
                        else ("DIRECT_NEEDS_CALLER_PROVENANCE"
                              if row["direct"] else "DELEGATED_OR_SOFTWARE"))
                    w.writerow([SOURCES[key], row["line"], row["name"],
                                int(row["direct"]), int(row["local_guard"]),
                                classification])
        status = "CALLPATH_BASELINE_CAPTURED_ADOPTION_BLOCKED"
        manifest.update({
            "status": status, "head": head,
            "sources": {rel: sha(ROOT / rel) for rel in SOURCES.values()},
            "archives_frozen": archives, "local_guards": guards,
            "lower_direct_without_local_guards": directs,
            "raw_entries_verified": ["EthernetClient::connected",
                                      "EthernetServer::begin",
                                      "EthernetUDP::begin"],
            "hardware_gpio4": "SCHEMATIC_USER_CONFIRMED",
            "firmware_reset_default": 255,
            "true_unprotected_callgraph_count": "NOT_ESTABLISHED",
            "matrix_sha256": sha(matrix),
        })
        say("HEAD", head)
        say("RUNTIME_LOCAL_GUARDS", guards[0])
        say("MODBUS_SERVER_LOCAL_GUARDS", guards[1])
        say("MODBUS_CLIENT_LOCAL_GUARDS", guards[2])
        say("TOTAL_HIGH_LEVEL_LOCAL_GUARDS", sum(guards))
        say("RAW_CLIENT_DIRECT_NO_LOCAL_GUARD", directs[0])
        say("RAW_SERVER_DIRECT_NO_LOCAL_GUARD", directs[1])
        say("RAW_UDP_DIRECT_NO_LOCAL_GUARD", directs[2])
        say("SOCKET_DIRECT_NO_LOCAL_GUARD", directs[3])
        say("BACKEND_DIRECT_NO_LOCAL_GUARD", directs[4])
        say("LOWER_DIRECT_TOTAL_NOT_BUG_COUNT", sum(directs))
        say("RESET_HW_GPIO", 4)
        say("RESET_FIRMWARE_DEFAULT", 255)
        say("ARCHIVES_FROZEN", archives)
        say("MATRIX_ROWS", sum(map(len, parsed.values())))
        say("STATUS", status)
        say("RESULTS", out)
        return 0
    except Exception as err:
        manifest["reason"] = repr(err)
        say("STATUS", "REVIEW_STOP")
        say("REASON", repr(err))
        say("RESULTS", out)
        return 2
    finally:
        (out / "MANIFEST.json").write_text(
            json.dumps(manifest, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8")


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as err:
        say("FATAL", repr(err))
        sys.exit(2)

#!/usr/bin/env python3
"""A13-G3-P0: source-anchored, read-only TCA RMW concurrency baseline."""
import argparse
import datetime as dt
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BASE = "bb538fa98a26b6486bae9f2f037a6a5f1b4e5dbe"
BRANCH = "v2.1.0-alpha.13/feature/cleanup-robustness"
BRIDGE = "JWPLC/2.1.0/cores/jwcontrol/peripherals/src/jwplc_i2c_bridge.cpp"
TCA = "JWPLC/2.1.0/cores/jwcontrol/peripherals/src/peripheral-tca6424a.c"


def require(ok, label):
    if not ok:
        raise RuntimeError(label)


def git(*args, check=True):
    p = subprocess.run(["git", *args], cwd=ROOT, stdout=subprocess.PIPE,
                       stderr=subprocess.PIPE, check=False)
    if check and p.returncode:
        raise RuntimeError("GIT_FAILED: " + " ".join(args) + ": " +
                           p.stderr.decode("utf-8", "replace")[:300])
    return p


def function(source, name):
    pattern = r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{"
    found = list(re.finditer(pattern, source))
    require(len(found) == 1, "FUNCTION_SIGNATURE_AMBIGUOUS: " + name)
    start = found[0].end() - 1
    depth = 0
    for i in range(start, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[start:i + 1]
    raise RuntimeError("UNBALANCED_FUNCTION: " + name)


def source_contract(bridge, tca):
    r = function(bridge, "jwplcI2C_readReg8")
    w = function(bridge, "jwplcI2C_writeReg8")
    u = function(bridge, "jwplcI2C_updateBit")
    pin = function(tca, "TCA6424A_writePin")
    bank = function(tca, "TCA6424A_writeBank")
    require(all("jwplcI2CLock()" in x and "jwplcI2CUnlock()" in x for x in (r, w)),
            "READ_WRITE_LOCK_CONTRACT_CHANGED")
    require("jwplcI2CLock()" not in u and "jwplcI2CUnlock()" not in u,
            "UPDATE_BIT_LOCK_CONTRACT_CHANGED")
    a, b = u.find("jwplcI2C_readReg8("), u.find("jwplcI2C_writeReg8(")
    require(0 <= a < b and "bitValue" in u, "UPDATE_BIT_RMW_CONTRACT_CHANGED")
    require("jwplcI2C_updateBit(" in pin and "g_outputShadowValid[bank]" in pin and
            "currentState == state" in pin, "SHADOW_WRITE_PIN_CONTRACT_CHANGED")
    require("g_outputShadow[bank] = state" in bank and "jwplcI2C_writeReg8(" in bank,
            "SHADOW_WRITE_BANK_CONTRACT_CHANGED")
    require("jwplcI2CLock()" not in pin and "jwplcI2CLock()" not in bank,
            "SHADOW_LOCK_CONTRACT_CHANGED")
    return {
        "read_reg_separately_locked": True,
        "write_reg_separately_locked": True,
        "rmw_has_no_enclosing_lock": True,
        "shadow_has_no_enclosing_lock": True,
        "write_pin_calls_update_bit": True,
    }


def scenario(order):
    registers = 0x00
    shadow = 0x00
    snapshots = {}
    log = []
    for step in order:
        actor, verb = step.split("_")
        bit = 0 if actor == "A" else 1
        if verb == "READ":
            snapshots[actor] = registers
            log.append(f"{step}: snapshot=0x{registers:02X}")
        else:
            require(actor in snapshots, "MISSING_SNAPSHOT")
            registers = snapshots[actor] | (1 << bit)
            shadow |= 1 << bit
            log.append(f"{step}: hw=0x{registers:02X} shadow=0x{shadow:02X}")
    return {"schedule": order, "hardware": registers, "shadow": shadow, "events": log}


def run_model():
    sequential = scenario(["A_READ", "A_WRITE", "B_READ", "B_WRITE"])
    overlap_ab = scenario(["A_READ", "B_READ", "A_WRITE", "B_WRITE"])
    overlap_ba = scenario(["A_READ", "B_READ", "B_WRITE", "A_WRITE"])
    require(sequential["hardware"] == 0x03 and sequential["shadow"] == 0x03,
            "SEQUENTIAL_CONTROL_NOT_VALID")
    require(overlap_ab["hardware"] == 0x02 and overlap_ab["shadow"] == 0x03,
            "OVERLAP_AB_NOT_REPRODUCED")
    require(overlap_ba["hardware"] == 0x01 and overlap_ba["shadow"] == 0x03,
            "OVERLAP_BA_NOT_REPRODUCED")
    return {"sequential_control": sequential, "overlap_ab": overlap_ab,
            "overlap_ba": overlap_ba,
            "shadow_false_noop_possible": bool(overlap_ab["shadow"] & 1 and
                                              not (overlap_ab["hardware"] & 1))}


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--self-test", action="store_true")
    ns = ap.parse_args()
    model = run_model()
    if ns.self_test:
        print("G3_P0_SELF_TEST=PASS")
        print("LOST_UPDATE_SIMULATION=PASS")
        return 0

    require(ROOT.joinpath(".git").exists(), "REPOSITORY_ROOT_NOT_FOUND")
    branch = git("branch", "--show-current").stdout.decode().strip()
    require(branch == BRANCH, "WRONG_BRANCH: " + branch)
    head = git("rev-parse", "HEAD").stdout.decode().strip()
    require(git("merge-base", "--is-ancestor", BASE, head, check=False).returncode == 0,
            "BASELINE_NOT_ANCESTOR")
    source = {}
    hashes = {}
    for path in (BRIDGE, TCA):
        raw = (ROOT / path).read_bytes()
        expected = git("show", BASE + ":" + path).stdout
        require(raw == expected, "SOURCE_CHANGED_SINCE_G3_BASELINE: " + path)
        source[path] = raw.decode("utf-8")
        hashes[path] = hashlib.sha256(raw).hexdigest()
    require(git("diff", "--quiet", "--", BRIDGE, TCA, check=False).returncode == 0,
            "UNCOMMITTED_SOURCE_CHANGE")
    require(git("diff", "--cached", "--quiet", "--", BRIDGE, TCA, check=False).returncode == 0,
            "STAGED_SOURCE_CHANGE")
    contract = source_contract(source[BRIDGE], source[TCA])
    stamp = dt.datetime.now().strftime("%Y%m%d_%H%M%S_%f")
    results = ROOT / "tools" / "alpha13" / "results" / ("g3_p0_" + stamp)
    results.mkdir(parents=True, exist_ok=False)
    payload = {
        "hito": "A13-G3-P0", "status": "PASS_BASELINE_RACE_REPRODUCED",
        "test_kind": "READ_ONLY_SOURCE_ANCHORED_DETERMINISTIC_MODEL",
        "physical_executed": False, "firmware_changed": False, "upload_executed": False,
        "branch": branch, "head": head, "baseline": BASE,
        "source_sha256": hashes, "source_contract": contract, "model": model,
        "interpretation": "An interleaving is possible; actual on-device scheduling not measured",
        "next_gate": "A13-G3-P1_CANDIDATE_DESIGN_AND_INSTRUMENTATION",
    }
    (results / "MANIFEST.json").write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    lines = ["GATE=A13-G3-P0", "STATUS=PASS_BASELINE_RACE_REPRODUCED",
             "SOURCE_CONTRACT=PASS", "SEQUENTIAL_CONTROL=0x03",
             "INTERLEAVED_AB_HARDWARE=0x02", "INTERLEAVED_BA_HARDWARE=0x01",
             "INTERLEAVED_SHADOW=0x03", "EXPECTED_HARDWARE=0x03",
             "SHADOW_FALSE_NOOP_POSSIBLE=YES", "PHYSICAL_EXECUTED=NO",
             "PRODUCT_MUTATED=NO", "UPLOAD_EXECUTED=NO", "HEAD=" + head,
             "RESULTS=" + str(results),
             "NEXT_GATE=A13-G3-P1_CANDIDATE_DESIGN_AND_INSTRUMENTATION"]
    (results / "SUMMARY.log").write_text("\n".join(lines) + "\n", encoding="utf-8")
    for x in lines:
        print(x)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as exc:
        print("STATUS=REVIEW_PRECONDITION_OR_HARNESS")
        print("REASON=" + str(exc))
        sys.exit(2)

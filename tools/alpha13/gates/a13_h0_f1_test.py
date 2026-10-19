#!/usr/bin/env python3
"""Alpha13 H0-F1: N0 de alcance y N1 host sobre el bloque SS ORIGINAL."""
from __future__ import annotations
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[3]
BASE = "a9c96273274170195498ff282decf93f0388a450"
W5100 = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp"
SPI = ROOT / "JWPLC/2.1.0/cores/jwcontrol/peripherals/include"
PATCH = '''// JWPLC F1: el CS real del W5500 procede exclusivamente del package.
 //'''
# El ancla identifica código real: nunca construir un sustituto de la selección SS.
F1 = '// JWPLC F1: el CS real del W5500 procede exclusivamente del package.'
INCLUDE = '#include "jwplc_spi_bus.h"'
ALIAS = '#define PIN_SPI_SS_ETHERNET_LIB JWPLC_ETH_CS'
GUARD = '#if defined(CONFIG_IDF_TARGET_ESP32) && (SS_PIN_DEFAULT >= 6) && (SS_PIN_DEFAULT <= 11)'
ERROR = '#error "SS_PIN_DEFAULT apunta a GPIO6-11, reservado para flash SPI en ESP32"'

def run(cmd, cwd=ROOT, timeout=30):
    p = subprocess.run(cmd, cwd=cwd, text=True, capture_output=True, errors="replace", timeout=timeout)
    return p.returncode, p.stdout + "\n" + p.stderr

def preflight():
    code, diff = run(["git", "diff", "--name-only", BASE, "HEAD"])
    assert code == 0, diff
    changed = set(filter(None, diff.splitlines()))
    product = {p for p in changed if p.startswith("JWPLC/")}
    assert product == {W5100}, f"F1_PRODUCT_SCOPE_VIOLATION: {sorted(product)}"
    assert not any("alpha15" in p.lower() or "bootloader.bin" in p.lower()
                   or p.endswith(".a") or "gpio4" in p.lower() for p in changed)
    assert "tools/alpha13/gates/a13_h0_f1_test.py" in changed
    assert ".github/workflows/alpha13-h0-f1-package.yml" in changed
    code, result = run(["git", "status", "--porcelain"])
    assert code == 0 and not result.strip(), "DIRTY_WORKTREE:" + result
    print("H0_F1_N0=PASS_EXACT_PRODUCT_ALLOWLIST_NO_GPIO4_NO_ARCHIVES")

def validate_source(source):
    assert source.count(F1) == 1, "F1_MISSING"
    assert source.count(INCLUDE) == 1, "SPI_BUS_INCLUDE_MISSING_OR_DUPLICATE"
    assert source.count(ALIAS) == 1, "SOURCE_OF_TRUTH_MISSING"
    assert source.count(GUARD) == 1 and source.count(ERROR) == 1, "FLASH_GUARD_MISSING"
    assert "W5100Class::ss_pin = SS_PIN_DEFAULT;" in source, "SS_INITIALIZER_DRIFT"
    header = (SPI / "jwplc_spi_bus.h").read_text(encoding="utf-8")
    assert "#define JWPLC_ETH_CS   5" in header, "JWPLC_ETH_CS_NOT_5_REVIEW"
    variant = (ROOT / "JWPLC/2.1.0/variants/jwplcbasic/pins_arduino.h").read_text()
    assert "static const uint8_t SS = 5;" in variant, "VARIANT_SS_DRIFT"
    return True

def ss_block(source):
    # Tomamos macros y guardas LITERALMENTE de w5100.cpp, sin reimplementarlas.
    end = source.index("// W5100 controller instance")
    start = source.index(F1) if F1 in source else source.index("// If variant.h or other headers")
    return source[start:end]

def compile_ss(block, expected, extra=(), should_fail=False):
    compiler = shutil.which("g++") or shutil.which("clang++")
    assert compiler, "HOST_CXX_UNAVAILABLE"
    unit = ('#include <stdint.h>\nstatic const uint8_t SS = 5;\n'
            + block + f'\nstatic_assert(SS_PIN_DEFAULT == {expected}, "SS_PIN_WRONG");\n'
            + 'int main() { return SS_PIN_DEFAULT; }\n')
    with tempfile.TemporaryDirectory(prefix="a13-h0-f1-native-") as tmp:
        src = Path(tmp) / "ss_test.cpp"
        src.write_text(unit)
        cmd = [compiler, "-std=c++17", "-Werror", "-fsyntax-only", "-I", str(SPI), *extra, str(src)]
        code, output = run(cmd)
    if should_fail:
        assert code != 0 and "SS_PIN_DEFAULT apunta" in output, "UNSAFE_GPIO_NOT_REJECTED:" + output[-700:]
    else:
        assert code == 0, "NATIVE_COMPILATION_FAILED:" + output[-900:]

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--selftest", action="store_true")
    parser.add_argument("--run", action="store_true")
    a = parser.parse_args()
    assert a.selftest != a.run
    fixed = (ROOT / W5100).read_text(encoding="utf-8")
    validate_source(fixed)
    if a.selftest:
        for removed in (F1, ALIAS, GUARD, ERROR):
            try:
                validate_source(fixed.replace(removed, "", 1))
            except AssertionError:
                continue
            raise AssertionError("NEGATIVE_DID_NOT_FAIL: " + removed)
        print("H0_F1_N0_NEGATIVES=PASS_4_OF_4")
        preflight()
        return
    code, original = run(["git", "show", BASE + ":" + W5100])
    assert code == 0, "BASELINE_SOURCE_UNAVAILABLE:" + original[-300:]
    assert F1 not in original, "BASELINE_ALREADY_PATCHED"
    compile_ss(ss_block(original), 10)
    print("H0_E1_NATIVE_BEFORE=PASS_SS_PIN_DEFAULT_10_SOURCE_REAL")
    compile_ss(ss_block(fixed), 5)
    print("H0_F1_N1_AFTER=PASS_SS_PIN_DEFAULT_5_SOURCE_REAL")
    compile_ss(ss_block(fixed), 10,
               extra=("-DPIN_SPI_SS_ETHERNET_LIB=10", "-DCONFIG_IDF_TARGET_ESP32=1"),
               should_fail=True)
    print("H0_F1_N1_UNSAFE_GPIO10=PASS_COMPILATION_REJECTED")
    print("H0_F1_N1=PASS_ALL_NO_HARDWARE")

if __name__ == "__main__":
    main()

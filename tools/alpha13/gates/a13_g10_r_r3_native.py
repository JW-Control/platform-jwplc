#!/usr/bin/env python3
"""G10-R R3: prueba NATIVA del SafeAccess compuesto + snapshot de laboratorio.

Sin USB, red, flash ni archivos productivos modificados. La adquisición SPI
se simula mediante un mutex host NO recursivo, pero se compila el C++ real
del overlay G10-I extendido exclusivamente en el staging de G10-R.
"""
from __future__ import annotations

import argparse
import importlib.util
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[3]
HEADER = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.h"
SOURCE = "JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp"
TEST = ROOT / "tools/alpha13/candidates/g10_r/native_g10_r_lab_snapshot_test.cpp"

TASK_H = """#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef void* TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
#ifdef __cplusplus
}
#endif
"""
SPI_H = """#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
bool jwplcSPI_acquire(uint32_t timeoutMs);
void jwplcSPI_release(void);
void jwplcSPI_deselectAll(void);
#ifdef __cplusplus
}
#endif
"""
EXPECTED = (
    "G10_R_R3_LAB_PENDING_BUSY_DRAIN=PASS",
    "G10_R_R3_LAB_RECYCLE_STALE_GUARD=PASS",
    "G10_R_R3_HOST_SOURCE_BACKED=PASS_NOT_PHYSICAL",
)


def require(ok: bool, message: str) -> None:
    if not ok:
        raise RuntimeError(message)


def module_at(relative: str, label: str):
    p = ROOT / relative
    spec = importlib.util.spec_from_file_location(label, p)
    require(spec is not None and spec.loader is not None, "R3_MODULE_NOT_FOUND_" + label)
    m = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = m
    spec.loader.exec_module(m)
    return m


def native() -> None:
    require(sys.platform.startswith("linux"), "R3_NATIVE_TARGET_LINUX_ONLY")
    compiler = shutil.which("g++") or shutil.which("clang++")
    require(compiler, "R3_NATIVE_CXX_MISSING")
    integrator = module_at("tools/alpha13/gates/a13_g10_integrated.py",
                           "g10r3_integrator_real")
    lab = module_at("tools/alpha13/gates/a13_g10_r_instrumentation.py",
                    "g10r3_snapshot_overlay")
    # No ejecutar integrator.main(): esa ruta escribe evidencias y gates
    # ajenos. Sólo usar su composición source-backed de 14 archivos.
    original = integrator.compose()
    result = lab.compose_lab(original)
    require(len(original) == 14 and len(result) == 14,
            "R3_SOURCE_COMPOSITION_COUNT_DRIFT")
    require(result[HEADER] != original[HEADER] and result[SOURCE] != original[SOURCE],
            "R3_SNAPSHOT_NOT_COMPOSED")
    require(all(result[k] == original[k] for k in original if k not in (HEADER, SOURCE)),
            "R3_LAB_CHANGED_UNRELATED_SOURCES")
    with tempfile.TemporaryDirectory(prefix="jwplc-g10r3-host-") as directory:
        root = Path(directory)
        inc = root / "include"
        (inc / "freertos").mkdir(parents=True)
        (inc / "freertos" / "task.h").write_text(TASK_H, encoding="utf-8")
        (inc / "jwplc_spi_bus.h").write_text(SPI_H, encoding="utf-8")
        hdr = root / "JWPLC_W5500_SafeAccess.h"
        src = root / "JWPLC_W5500_SafeAccess.cpp"
        hdr.write_bytes(result[HEADER].encode("utf-8"))
        src.write_bytes(result[SOURCE].encode("utf-8"))
        executable = root / "lab_snapshot_test"
        cmd = [compiler, "-std=c++17", "-O0", "-Wall", "-Wextra",
               "-Werror", "-I", str(inc), "-I", str(root),
               str(src), str(TEST), "-o", str(executable)]

        def compile_and_run(version: str):
            src.write_bytes(version.encode("utf-8"))
            built = subprocess.run(cmd, capture_output=True, text=True,
                                   timeout=120, errors="replace")
            require(built.returncode == 0 and executable.is_file(),
                    "R3_REAL_SOURCE_NATIVE_COMPILE_FAILED_" +
                    (built.stderr or built.stdout)[-1300:])
            return subprocess.run([str(executable)], capture_output=True,
                                  text=True, timeout=30, errors="replace")

        real = result[SOURCE]
        run = compile_and_run(real)
        require(run.returncode == 0, "R3_REAL_SOURCE_NATIVE_FAILED_" +
                (run.stderr or run.stdout)[-1300:])
        for line in EXPECTED:
            require(line in run.stdout, "R3_SOURCE_ASSERT_MISSING_" + line)
            print(line)

        # Negativos con el MISMO test host y misma fuente compuesta,
        # mutando una línea de instrumentación por vez.
        mutants = [
            ("PENDING_NOT_CAPTURED",
             "out[index].pendingCloseAllocation =\n"
             "            g_pendingClose[index].load(std::memory_order_acquire);",
             "out[index].pendingCloseAllocation = 0;",
             "PENDING_TICKET_NOT_VISIBLE"),
            ("BUS_BUSY_FALSE_CAPTURED",
             "if (!bus) return LabCaptureOutcome::BusBusy;",
             "if (!bus) return LabCaptureOutcome::Captured;",
             "BUS_BUSY_NOT_DISTINGUISHED"),
            ("EPOCH_ZEROED",
             "out[index].epoch = slot.epoch;",
             "out[index].epoch = 0;",
             "INITIAL_SNAPSHOT_LEASE_MISMATCH"),
            ("OWNER_ZEROED",
             "out[index].ownerCookie = slot.owner;",
             "out[index].ownerCookie = 0;",
             "INITIAL_SNAPSHOT_LEASE_MISMATCH"),
        ]
        for label, before, after, rejection in mutants:
            require(real.count(before) == 1,
                    "R3_NATIVE_MUTATION_ANCHOR_DRIFT_" + label)
            trial = compile_and_run(real.replace(before, after, 1))
            require(trial.returncode != 0 and rejection in trial.stderr,
                    "R3_NATIVE_MUTANT_NOT_REJECTED_" + label +
                    "_" + trial.stderr[-600:])
        print("G10_R_R3_SOURCE_MUTANTS=4_PASS_REJECTED")
    print("G10_R_R3_REAL_CPP_NATIVE=PASS_LINUX_HOST_NO_HARDWARE")
    print("G10_R_R3_PRODUCT_MODIFIED=NO")


def selftest() -> None:
    require(TEST.is_file(), "R3_SOURCE_TEST_CPP_MISSING")
    s = TEST.read_text(encoding="utf-8")
    for needle in ("labCaptureSockets", "requestDeferredClose",
                   "serviceDeferredClose", "closeIfCurrent", "BusBusy",
                   "STALE_TICKET_CLOSED_NEW_OWNER"):
        require(needle in s, "R3_NATIVE_TEST_ANCHOR_MISSING_" + needle)
    print("G10_R_R3_NATIVE_GUARD=PASS_STATIC_ONLY")
    print("G10_R_R3_PHYSICAL=NOT_RUN")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    opts = ap.add_mutually_exclusive_group(required=True)
    opts.add_argument("--selftest", action="store_true")
    opts.add_argument("--native", action="store_true")
    args = ap.parse_args()
    try:
        if args.selftest:
            selftest()
        else:
            native()
        return 0
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print("G10_R_R3_NATIVE_REVIEW_STOP=" + str(error))
        return 2


if __name__ == "__main__":
    sys.exit(main())

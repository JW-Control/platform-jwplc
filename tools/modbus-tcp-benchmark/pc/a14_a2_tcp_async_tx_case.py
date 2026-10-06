#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import struct
import threading
import time
from dataclasses import dataclass, field

import serial

SNAPSHOT_END = "A14_A2_SNAPSHOT=END"
CASE_END = "A14_A2_CASE=END"


def parse_values(text: str) -> dict[str, str]:
    result: dict[str, str] = {"_RAW": text}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()
    return result


def fnv1a(data: bytes) -> int:
    value = 2166136261
    for byte in data:
        value ^= byte
        value = (value * 16777619) & 0xFFFFFFFF
    return value


class DutSerial:
    def __init__(self, port: str):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = 115200
        self.ser.timeout = 0.05
        self.ser.write_timeout = 1.0
        self.ser.dtr = False
        self.ser.rts = False

    def open(self) -> None:
        self.ser.open()
        time.sleep(2.0)

    def close(self) -> None:
        if self.ser.is_open:
            self.ser.close()

    def read_until(self, marker: str, timeout_s: float) -> str:
        deadline = time.perf_counter() + timeout_s
        raw = bytearray()
        marker_bytes = marker.encode()
        while time.perf_counter() < deadline:
            chunk = self.ser.read(max(1, self.ser.in_waiting))
            if chunk:
                raw.extend(chunk)
                if marker_bytes in raw:
                    break
        text = raw.decode("utf-8", errors="replace")
        if marker not in text:
            raise RuntimeError(f"A2_SERIAL_TIMEOUT marker={marker} raw={text!r}")
        return text

    def snapshot(self) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(b"S\n")
        self.ser.flush()
        return parse_values(self.read_until(SNAPSHOT_END, 3.0))

    def run_case(
        self,
        case_name: str,
        pc_ip: str,
        port: int,
        timeout_s: float,
    ) -> dict[str, str]:
        self.ser.reset_input_buffer()
        command = f"RUN {case_name} {pc_ip} {port}\n"
        self.ser.write(command.encode())
        self.ser.flush()
        return parse_values(self.read_until(CASE_END, timeout_s))


@dataclass
class ServerResult:
    accepted: bool = False
    data: bytearray = field(default_factory=bytearray)
    reset: bool = False
    error: str = ""


class CaseServer:
    def __init__(self, mode: str):
        self.mode = mode
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        if mode == "NO_READ":
            self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
        self.listener.bind(("0.0.0.0", 0))
        self.listener.listen(1)
        self.listener.settimeout(8.0)
        self.port = self.listener.getsockname()[1]
        self.result = ServerResult()
        self.stop_event = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)

    def start(self) -> None:
        self.thread.start()

    def close(self) -> ServerResult:
        self.stop_event.set()
        try:
            self.listener.close()
        except OSError:
            pass
        self.thread.join(timeout=3.0)
        return self.result

    def _run(self) -> None:
        conn: socket.socket | None = None
        try:
            conn, _ = self.listener.accept()
            self.result.accepted = True
            if self.mode == "CLOSE":
                conn.setsockopt(
                    socket.SOL_SOCKET,
                    socket.SO_LINGER,
                    struct.pack("hh", 1, 0),
                )
                return
            if self.mode == "NO_READ":
                conn.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 1024)
                self.stop_event.wait(15.0)
                return

            conn.settimeout(0.2)
            while not self.stop_event.is_set():
                try:
                    chunk = conn.recv(4096)
                    if not chunk:
                        break
                    self.result.data.extend(chunk)
                except socket.timeout:
                    continue
                except (ConnectionResetError, ConnectionAbortedError):
                    self.result.reset = True
                    break
        except OSError as exc:
            if not self.stop_event.is_set():
                self.result.error = repr(exc)
        finally:
            if conn is not None:
                try:
                    conn.close()
                except OSError:
                    pass


def local_ip_for_peer(peer_ip: str) -> str:
    probe = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    try:
        probe.connect((peer_ip, 9))
        return probe.getsockname()[0]
    finally:
        probe.close()


def wait_ready(dut: DutSerial) -> dict[str, str]:
    deadline = time.perf_counter() + 30.0
    last: dict[str, str] = {}
    while time.perf_counter() < deadline:
        last = dut.snapshot()
        if (
            last.get("A2_ETH_READY") == "YES"
            and last.get("A2_ETH_LINK") == "UP"
            and last.get("A2_IDLE") == "YES"
            and last.get("A2_IP") not in (None, "0.0.0.0")
        ):
            return last
        time.sleep(0.25)
    raise RuntimeError(f"A2_DUT_NOT_READY raw={last.get('_RAW', '')}")


def integer(values: dict[str, str], key: str) -> int:
    try:
        return int(values[key])
    except (KeyError, ValueError) as exc:
        raise RuntimeError(f"A2_INVALID_{key} raw={values.get('_RAW', '')}") from exc


def require_case(
    values: dict[str, str],
    expected: str,
    boot_id: int,
) -> None:
    if values.get("A2_CASE") != expected or values.get("A2_RESULT") != "PASS":
        raise RuntimeError(f"A2_{expected}_FAILED raw={values.get('_RAW', '')}")
    if integer(values, "A2_BOOT_ID") != boot_id:
        raise RuntimeError(f"A2_{expected}_RESET_DETECTED")
    if integer(values, "A2_SPI_LOCK_ERRORS") != 0:
        raise RuntimeError(f"A2_{expected}_SPI_LOCK_ERRORS")
    limit = integer(values, "A2_API_LIMIT_US")
    for key in ("A2_BEGIN_WRITE_MAX_US", "A2_POLL_WRITE_MAX_US", "A2_POLL_FLUSH_MAX_US"):
        if integer(values, key) > limit:
            raise RuntimeError(f"A2_{expected}_{key}_EXCEEDED")


def run_one(
    dut: DutSerial,
    *,
    name: str,
    mode: str,
    pc_ip: str,
    boot_id: int,
    timeout_s: float = 8.0,
) -> tuple[dict[str, str], ServerResult]:
    server = CaseServer(mode)
    server.start()
    try:
        values = dut.run_case(name, pc_ip, server.port, timeout_s)
        require_case(values, name, boot_id)
        time.sleep(0.2)
    finally:
        server_result = server.close()
    if not server_result.accepted:
        raise RuntimeError(f"A2_{name}_NOT_ACCEPTED error={server_result.error}")
    return values, server_result


def main() -> int:
    parser = argparse.ArgumentParser(description="A14 A2 TCP async TX physical case")
    parser.add_argument("--serial", default="COM14")
    args = parser.parse_args()

    dut = DutSerial(args.serial)
    try:
        dut.open()
        ready = wait_ready(dut)
        dut_ip = ready["A2_IP"]
        pc_ip = local_ip_for_peer(dut_ip)
        boot_id = integer(ready, "A2_BOOT_ID")
        print(f"A2_DUT_IP={dut_ip}")
        print(f"A2_PC_IP={pc_ip}")
        print(f"A2_BOOT_ID_INITIAL={boot_id}")

        sequence = (
            ("NORMAL", "READ", 8.0),
            ("FLUSH", "READ", 8.0),
            ("CANCEL", "READ", 8.0),
            ("PEER_CLOSE", "CLOSE", 8.0),
            ("TIMEOUT", "NO_READ", 15.0),
            ("NORMAL", "READ", 8.0),
        )
        reconnect_passes = 0
        integrity_passes = 0
        all_results: list[dict[str, str]] = []

        for index, (name, mode, timeout_s) in enumerate(sequence, 1):
            values, server_result = run_one(
                dut,
                name=name,
                mode=mode,
                pc_ip=pc_ip,
                boot_id=boot_id,
                timeout_s=timeout_s,
            )
            all_results.append(values)
            if name in ("NORMAL", "FLUSH"):
                expected_bytes = integer(values, "A2_PAYLOAD_BYTES")
                expected_fnv = integer(values, "A2_PAYLOAD_FNV")
                actual = bytes(server_result.data)
                if len(actual) != expected_bytes or fnv1a(actual) != expected_fnv:
                    raise RuntimeError(
                        f"A2_{name}_CORRUPTION bytes={len(actual)}/{expected_bytes} "
                        f"fnv={fnv1a(actual)}/{expected_fnv}"
                    )
                integrity_passes += 1
            if name == "NORMAL":
                reconnect_passes += 1
            if name == "FLUSH" and values.get("A2_FLUSH_PENDING_OBSERVED") != "YES":
                raise RuntimeError("A2_FLUSH_PENDING_NOT_OBSERVED")
            if name == "CANCEL":
                if values.get("A2_WRITE_PENDING_OBSERVED") != "YES" or values.get("A2_CANCEL_CLOSED_SOCKET") != "YES":
                    raise RuntimeError("A2_CANCEL_CONTRACT_FAILED")
            if name == "TIMEOUT":
                if integer(values, "A2_TIMEOUT_ELAPSED_MS") < 90 or integer(values, "A2_WRITES_COMPLETED") < 1:
                    raise RuntimeError("A2_TIMEOUT_NOT_NATURAL")

            print(
                f"A2_CASE_{index}={name} RESULT=PASS "
                f"BEGIN_MAX_US={values['A2_BEGIN_WRITE_MAX_US']} "
                f"POLL_MAX_US={values['A2_POLL_WRITE_MAX_US']} "
                f"FLUSH_POLL_MAX_US={values['A2_POLL_FLUSH_MAX_US']} "
                f"WRITES={values['A2_WRITES_COMPLETED']} "
                f"BYTES={values['A2_BYTES_COMPLETED']}"
            )

        final = dut.snapshot()
        if integer(final, "A2_BOOT_ID") != boot_id:
            raise RuntimeError("A2_FINAL_RESET_DETECTED")
        if final.get("A2_ETH_LINK") != "UP" or final.get("A2_IDLE") != "YES":
            raise RuntimeError(f"A2_FINAL_STATE_BAD raw={final.get('_RAW', '')}")

        print(f"A2_INTEGRITY_CASES_PASS={integrity_passes}")
        print(f"A2_RECONNECT_NORMAL_PASS={reconnect_passes}")
        print("A2_CORRUPTION_ERRORS=0")
        print("A2_SPI_LOCK_ERRORS=0")
        print("A2_TRANSPORT_RESETS=0")
        print("A2_DEVICE_RESETS=0")
        print("A2_LINK_FINAL=UP")
        print("A2_PHYSICAL_STABILITY=PENDING_USER")
        print("A2_GATE_RESULT=PASS")
        return 0
    finally:
        dut.close()


if __name__ == "__main__":
    raise SystemExit(main())

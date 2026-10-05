#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import threading
import time
from dataclasses import dataclass

import serial

SNAPSHOT_END = "A14_S1_SNAPSHOT=END"
CASE_END = "A14_S1_CASE=END"
PAYLOAD_BYTES = 1536


def parse_values(text: str) -> dict[str, str]:
    result: dict[str, str] = {"_RAW": text}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()
    return result


def expected_pattern() -> bytes:
    return bytes(((i * 29 + 0xA7) & 0xFF) for i in range(PAYLOAD_BYTES))


class DutSerial:
    def __init__(self, port: str):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = 115200
        self.ser.timeout = 0.1
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
            raise RuntimeError(f"S1_SERIAL_TIMEOUT marker={marker} raw={text!r}")
        return text

    def snapshot(self) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(b"S\n")
        self.ser.flush()
        return parse_values(self.read_until(SNAPSHOT_END, 3.0))

    def soak(
        self,
        pc_ip: str,
        port: int,
        duration_s: int,
    ) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(f"SOAK {pc_ip} {port} {duration_s}\n".encode())
        self.ser.flush()
        return parse_values(self.read_until(CASE_END, duration_s + 30.0))


@dataclass
class StreamResult:
    accepted: bool = False
    bytes_received: int = 0
    mismatches: int = 0
    socket_errors: int = 0
    reset: bool = False
    error: str = ""


class VerifyingServer:
    def __init__(self):
        self.listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.listener.bind(("0.0.0.0", 0))
        self.listener.listen(1)
        self.listener.settimeout(10.0)
        self.port = self.listener.getsockname()[1]
        self.result = StreamResult()
        self.done = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)
        pattern = expected_pattern()
        self.expected_cycle = pattern * 64

    def start(self) -> None:
        self.thread.start()

    def wait(self, timeout_s: float = 10.0) -> StreamResult:
        if not self.done.wait(timeout_s):
            raise RuntimeError("S1_SERVER_EOF_TIMEOUT")
        self.thread.join(timeout=1.0)
        try:
            self.listener.close()
        except OSError:
            pass
        return self.result

    def abort(self) -> None:
        try:
            self.listener.close()
        except OSError:
            pass
        self.done.wait(1.0)

    def _run(self) -> None:
        conn: socket.socket | None = None
        try:
            conn, _ = self.listener.accept()
            self.result.accepted = True
            conn.settimeout(1.0)
            while True:
                try:
                    chunk = conn.recv(65536)
                    if not chunk:
                        break
                    offset = self.result.bytes_received % PAYLOAD_BYTES
                    expected = self.expected_cycle[offset : offset + len(chunk)]
                    if chunk != expected:
                        self.result.mismatches += sum(
                            actual != wanted
                            for actual, wanted in zip(chunk, expected)
                        )
                    self.result.bytes_received += len(chunk)
                except socket.timeout:
                    continue
                except (ConnectionResetError, ConnectionAbortedError):
                    self.result.reset = True
                    self.result.socket_errors += 1
                    break
                except OSError as exc:
                    self.result.socket_errors += 1
                    self.result.error = repr(exc)
                    break
        except OSError as exc:
            self.result.socket_errors += 1
            self.result.error = repr(exc)
        finally:
            if conn is not None:
                try:
                    conn.close()
                except OSError:
                    pass
            self.done.set()


def integer(values: dict[str, str], key: str) -> int:
    try:
        return int(values[key])
    except (KeyError, ValueError) as exc:
        raise RuntimeError(f"S1_INVALID_{key} raw={values.get('_RAW', '')}") from exc


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
            last.get("S1_ETH_READY") == "YES"
            and last.get("S1_ETH_LINK") == "UP"
            and last.get("S1_IDLE") == "YES"
            and last.get("S1_IP") not in (None, "0.0.0.0")
        ):
            return last
        time.sleep(0.25)
    raise RuntimeError(f"S1_DUT_NOT_READY raw={last.get('_RAW', '')}")


def run_stream(
    dut: DutSerial,
    *,
    pc_ip: str,
    duration_s: int,
    boot_id: int,
    label: str,
) -> tuple[dict[str, str], StreamResult]:
    server = VerifyingServer()
    server.start()
    try:
        values = dut.soak(pc_ip, server.port, duration_s)
        stream = server.wait(10.0)
    except Exception:
        server.abort()
        raise

    if values.get("S1_RESULT") != "PASS":
        raise RuntimeError(f"S1_{label}_DUT_FAILED raw={values.get('_RAW', '')}")
    if integer(values, "S1_BOOT_ID") != boot_id:
        raise RuntimeError(f"S1_{label}_DEVICE_RESET")
    if not stream.accepted:
        raise RuntimeError(f"S1_{label}_NOT_ACCEPTED error={stream.error}")
    if stream.mismatches != 0:
        raise RuntimeError(f"S1_{label}_CORRUPTION={stream.mismatches}")
    if stream.socket_errors != 0 or stream.reset:
        raise RuntimeError(f"S1_{label}_SOCKET_ERRORS={stream.socket_errors} reset={stream.reset}")
    if stream.bytes_received != integer(values, "S1_BYTES_COMPLETED"):
        raise RuntimeError(
            f"S1_{label}_BYTE_MISMATCH pc={stream.bytes_received} "
            f"dut={values.get('S1_BYTES_COMPLETED')}"
        )
    if integer(values, "S1_SPI_LOCK_ERRORS") != 0 or integer(values, "S1_TRANSPORT_ERRORS") != 0:
        raise RuntimeError(f"S1_{label}_DUT_ERRORS")
    if values.get("S1_LINK_FINAL") != "UP" or values.get("S1_WRITE_PENDING_OBSERVED") != "YES":
        raise RuntimeError(f"S1_{label}_FINAL_CONTRACT_FAILED")
    limit = integer(values, "S1_API_LIMIT_US")
    if integer(values, "S1_BEGIN_WRITE_MAX_US") > limit or integer(values, "S1_POLL_WRITE_MAX_US") > limit:
        raise RuntimeError(f"S1_{label}_API_CALL_TOO_SLOW")
    return values, stream


def main() -> int:
    parser = argparse.ArgumentParser(description="A14 S1 persistent TCP async TX soak")
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--duration", type=int, default=600)
    parser.add_argument("--recovery-duration", type=int, default=2)
    args = parser.parse_args()
    if args.duration < 600:
        raise RuntimeError("S1_DURATION_MUST_BE_AT_LEAST_600_SECONDS")

    dut = DutSerial(args.serial)
    try:
        dut.open()
        ready = wait_ready(dut)
        dut_ip = ready["S1_IP"]
        pc_ip = local_ip_for_peer(dut_ip)
        boot_id = integer(ready, "S1_BOOT_ID")
        print(f"S1_DUT_IP={dut_ip}")
        print(f"S1_PC_IP={pc_ip}")
        print(f"S1_BOOT_ID_INITIAL={boot_id}")
        print(f"S1_REQUESTED_SOAK_S={args.duration}")

        soak_values, soak_stream = run_stream(
            dut,
            pc_ip=pc_ip,
            duration_s=args.duration,
            boot_id=boot_id,
            label="SOAK",
        )
        traffic_ms = integer(soak_values, "S1_TRAFFIC_DURATION_MS")
        if traffic_ms < args.duration * 1000:
            raise RuntimeError(f"S1_DURATION_SHORT={traffic_ms}")
        throughput_mbps = soak_stream.bytes_received * 8.0 / (traffic_ms * 1000.0)
        print(f"S1_SOAK_TRAFFIC_MS={traffic_ms}")
        print(f"S1_SOAK_WRITES={soak_values['S1_WRITES_COMPLETED']}")
        print(f"S1_SOAK_BYTES={soak_stream.bytes_received}")
        print(f"S1_SOAK_THROUGHPUT_MBPS={throughput_mbps:.6f}")
        print(f"S1_SOAK_BEGIN_MAX_US={soak_values['S1_BEGIN_WRITE_MAX_US']}")
        print(f"S1_SOAK_POLL_MAX_US={soak_values['S1_POLL_WRITE_MAX_US']}")

        recovery_values, recovery_stream = run_stream(
            dut,
            pc_ip=pc_ip,
            duration_s=args.recovery_duration,
            boot_id=boot_id,
            label="RECOVERY",
        )
        print(f"S1_RECOVERY_BYTES={recovery_stream.bytes_received}")
        print("S1_RECOVERY_RECONNECT=PASS")

        final = dut.snapshot()
        if integer(final, "S1_BOOT_ID") != boot_id:
            raise RuntimeError("S1_FINAL_DEVICE_RESET")
        if final.get("S1_ETH_LINK") != "UP" or final.get("S1_IDLE") != "YES":
            raise RuntimeError(f"S1_FINAL_STATE_BAD raw={final.get('_RAW', '')}")

        print("S1_PAYLOAD_INTEGRITY=PASS")
        print("S1_CORRUPTION_ERRORS=0")
        print("S1_SPI_LOCK_ERRORS=0")
        print("S1_TRANSPORT_ERRORS=0")
        print("S1_DEVICE_RESETS=0")
        print("S1_LINK_FINAL=UP")
        print("S1_PHYSICAL_STABILITY=PENDING_USER")
        print("S1_GATE_RESULT=PASS")
        return 0
    finally:
        dut.close()


if __name__ == "__main__":
    raise SystemExit(main())

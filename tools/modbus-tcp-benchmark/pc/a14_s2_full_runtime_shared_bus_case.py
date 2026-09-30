#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import threading
import time
from dataclasses import dataclass

import serial

MASTER_SNAPSHOT_END = "A14_S2_MASTER_SNAPSHOT=END"
MASTER_CASE_END = "A14_S2_MASTER_CASE=END"
SLAVE_SNAPSHOT_END = "A14_S2_SLAVE_SNAPSHOT=END"
SLAVE_CASE_END = "A14_S2_SLAVE_CASE=END"


def parse_values(text: str) -> dict[str, str]:
    result: dict[str, str] = {"_RAW": text}
    for line in text.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            result[key.strip()] = value.strip()
    return result


def integer(values: dict[str, str], key: str) -> int:
    try:
        return int(values[key])
    except (KeyError, ValueError) as exc:
        raise RuntimeError(
            f"S2_INVALID_{key} raw={values.get('_RAW', '')}"
        ) from exc


class SerialPeer:
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

    def command(self, line: str) -> None:
        self.ser.write((line + "\n").encode())
        self.ser.flush()

    def read_until(self, marker: str, timeout_s: float) -> str:
        deadline = time.perf_counter() + timeout_s
        raw = bytearray()
        marker_b = marker.encode()

        while time.perf_counter() < deadline:
            chunk = self.ser.read(max(1, self.ser.in_waiting))
            if chunk:
                raw.extend(chunk)
                if marker_b in raw:
                    break

        text = raw.decode("utf-8", errors="replace")
        if marker not in text:
            raise RuntimeError(
                f"S2_SERIAL_TIMEOUT port={self.ser.port} marker={marker} "
                f"raw={text[-3000:]!r}"
            )
        return text

    def snapshot(self, marker: str) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.command("S")
        return parse_values(self.read_until(marker, 3.0))


@dataclass
class TrafficStats:
    sent: int = 0
    received: int = 0
    mismatches: int = 0
    send_errors: int = 0
    recv_errors: int = 0


class BidirectionalTraffic:
    def __init__(self, host: str, port: int):
        self.host = host
        self.port = port
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 4 * 1024 * 1024)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 4 * 1024 * 1024)
        self.sock.settimeout(1.0)
        self.stats = TrafficStats()
        self._lock = threading.Lock()
        self._stop_send = threading.Event()
        self._done = threading.Event()
        self._target_echo: int | None = None
        self._sender = threading.Thread(target=self._send_loop, daemon=True)
        self._receiver = threading.Thread(target=self._recv_loop, daemon=True)

    @staticmethod
    def byte_at(offset: int) -> int:
        return ((offset & 0xFFFFFFFF) * 29 + 0xA7) & 0xFF

    @classmethod
    def pattern(cls, offset: int, length: int) -> bytes:
        return bytes(cls.byte_at(offset + i) for i in range(length))

    def connect(self) -> None:
        self.sock.connect((self.host, self.port))
        self._sender.start()
        self._receiver.start()

    def stop_sending(self, target_echo: int) -> None:
        self._target_echo = target_echo
        self._stop_send.set()
        try:
            self.sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass

    def wait_drained(self, timeout_s: float = 8.0) -> TrafficStats:
        deadline = time.perf_counter() + timeout_s
        while time.perf_counter() < deadline:
            with self._lock:
                received = self.stats.received
            if self._target_echo is not None and received >= self._target_echo:
                break
            time.sleep(0.02)

        self._done.set()
        try:
            self.sock.close()
        except OSError:
            pass
        self._sender.join(timeout=2.0)
        self._receiver.join(timeout=2.0)
        return self.snapshot()

    def abort(self) -> None:
        self._stop_send.set()
        self._done.set()
        try:
            self.sock.close()
        except OSError:
            pass
        self._sender.join(timeout=1.0)
        self._receiver.join(timeout=1.0)

    def snapshot(self) -> TrafficStats:
        with self._lock:
            return TrafficStats(
                sent=self.stats.sent,
                received=self.stats.received,
                mismatches=self.stats.mismatches,
                send_errors=self.stats.send_errors,
                recv_errors=self.stats.recv_errors,
            )

    def _send_loop(self) -> None:
        offset = 0
        chunk_len = 4096

        while not self._stop_send.is_set():
            payload = self.pattern(offset, chunk_len)
            try:
                self.sock.sendall(payload)
                offset += len(payload)
                with self._lock:
                    self.stats.sent += len(payload)
            except socket.timeout:
                continue
            except OSError:
                if not self._stop_send.is_set():
                    with self._lock:
                        self.stats.send_errors += 1
                return

    def _recv_loop(self) -> None:
        offset = 0

        while not self._done.is_set():
            try:
                chunk = self.sock.recv(65536)
                if not chunk:
                    return

                expected = self.pattern(offset, len(chunk))
                mismatches = sum(
                    actual != wanted
                    for actual, wanted in zip(chunk, expected)
                )
                offset += len(chunk)

                with self._lock:
                    self.stats.received += len(chunk)
                    self.stats.mismatches += mismatches

                if self._target_echo is not None and offset >= self._target_echo:
                    return

            except socket.timeout:
                continue
            except OSError:
                if not self._done.is_set():
                    with self._lock:
                        self.stats.recv_errors += 1
                return


def wait_ready(
    peer: SerialPeer,
    marker: str,
    role: str,
    timeout_s: float = 45.0,
) -> dict[str, str]:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last = peer.snapshot(marker)
        if last.get("S2_ROLE") == role and last.get("S2_READY") == "YES":
            if role == "MASTER":
                if (
                    last.get("S2_ETH_READY") == "YES"
                    and last.get("S2_ETH_LINK") == "UP"
                    and last.get("S2_IP") not in (None, "0.0.0.0")
                ):
                    return last
            else:
                return last
        time.sleep(0.25)

    raise RuntimeError(
        f"S2_{role}_NOT_READY raw={last.get('_RAW', '')}"
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 S2 full-runtime Master/Slave shared-bus runner"
    )
    parser.add_argument("--master", default="COM14")
    parser.add_argument("--slave", default="COM4")
    parser.add_argument("--duration", type=int, default=600)
    args = parser.parse_args()

    if args.duration < 600:
        raise RuntimeError("S2_DURATION_MUST_BE_AT_LEAST_600_SECONDS")

    master = SerialPeer(args.master)
    slave = SerialPeer(args.slave)
    traffic: BidirectionalTraffic | None = None

    try:
        slave.open()
        master.open()

        slave_ready = wait_ready(
            slave,
            SLAVE_SNAPSHOT_END,
            "SLAVE",
        )
        master_ready = wait_ready(
            master,
            MASTER_SNAPSHOT_END,
            "MASTER",
        )

        master_boot = integer(master_ready, "S2_BOOT_ID")
        slave_boot = integer(slave_ready, "S2_BOOT_ID")
        dut_ip = master_ready["S2_IP"]

        print(f"S2_MASTER_IP={dut_ip}")
        print(f"S2_MASTER_BOOT_INITIAL={master_boot}")
        print(f"S2_SLAVE_BOOT_INITIAL={slave_boot}")
        print(f"S2_DURATION_S={args.duration}")

        slave.ser.reset_input_buffer()
        slave.command("START")
        slave_start = slave.read_until("S2_SLAVE_START=PASS", 3.0)
        print("S2_SLAVE_START=PASS")

        master.ser.reset_input_buffer()
        master.command(f"START {args.duration}")
        master_start = master.read_until("S2_MASTER_START=PASS", 3.0)
        print("S2_MASTER_START=PASS")

        traffic = BidirectionalTraffic(dut_ip, 5002)
        traffic.connect()
        print("S2_PC_TCP_CONNECTED=PASS")

        master_result_text = master.read_until(
            MASTER_CASE_END,
            args.duration + 90.0,
        )
        master_values = parse_values(master_result_text)

        if master_values.get("S2_RESULT") != "PASS":
            raise RuntimeError(
                f"S2_MASTER_FAILED raw={master_result_text[-6000:]}"
            )

        if integer(master_values, "S2_BOOT_ID") != master_boot:
            raise RuntimeError("S2_MASTER_RESET")

        echo_target = integer(master_values, "S2_ETH_ECHO_BYTES")
        rx_target = integer(master_values, "S2_ETH_RX_BYTES")

        traffic.stop_sending(echo_target)
        traffic_stats = traffic.wait_drained(10.0)

        if traffic_stats.mismatches != 0:
            raise RuntimeError(
                f"S2_PC_ECHO_CORRUPTION={traffic_stats.mismatches}"
            )

        if traffic_stats.send_errors != 0 or traffic_stats.recv_errors != 0:
            raise RuntimeError(
                "S2_PC_SOCKET_ERRORS "
                f"send={traffic_stats.send_errors} recv={traffic_stats.recv_errors}"
            )

        if traffic_stats.received != echo_target:
            raise RuntimeError(
                f"S2_PC_ECHO_BYTE_MISMATCH pc={traffic_stats.received} "
                f"dut={echo_target}"
            )

        if echo_target != rx_target:
            raise RuntimeError(
                f"S2_DUT_RX_ECHO_MISMATCH rx={rx_target} echo={echo_target}"
            )

        print(f"S2_PC_SENT_BYTES={traffic_stats.sent}")
        print(f"S2_PC_ECHO_BYTES={traffic_stats.received}")
        print("S2_PC_ECHO_INTEGRITY=PASS")

        master.command("STOP")
        master.read_until("S2_MASTER_CLEANUP=PASS", 5.0)
        print("S2_MASTER_CLEANUP=PASS")

        slave.ser.reset_input_buffer()
        slave.command("STOP")
        slave_result_text = slave.read_until(
            SLAVE_CASE_END,
            8.0,
        )
        slave_values = parse_values(slave_result_text)

        if slave_values.get("S2_SLAVE_RESULT") != "PASS":
            raise RuntimeError(
                f"S2_SLAVE_FAILED raw={slave_result_text[-5000:]}"
            )

        if integer(slave_values, "S2_BOOT_ID") != slave_boot:
            raise RuntimeError("S2_SLAVE_RESET")

        # Final boot/link state after cleanup.
        master_final = wait_ready(
            master,
            MASTER_SNAPSHOT_END,
            "MASTER",
            timeout_s=15.0,
        )
        slave_final = wait_ready(
            slave,
            SLAVE_SNAPSHOT_END,
            "SLAVE",
            timeout_s=15.0,
        )

        if integer(master_final, "S2_BOOT_ID") != master_boot:
            raise RuntimeError("S2_MASTER_RESET_AFTER_CLEANUP")
        if integer(slave_final, "S2_BOOT_ID") != slave_boot:
            raise RuntimeError("S2_SLAVE_RESET_AFTER_CLEANUP")

        keys = (
            "S2_DURATION_MS",
            "S2_ETH_RX_BYTES",
            "S2_ETH_ECHO_BYTES",
            "S2_ETH_RX_OPS",
            "S2_ETH_WRITE_OPS",
            "S2_ETH_CORRUPTION_ERRORS",
            "S2_ETH_TRANSPORT_ERRORS",
            "S2_ETH_SPI_LOCK_ERRORS",
            "S2_ETH_BEGIN_WRITE_MAX_US",
            "S2_ETH_POLL_WRITE_MAX_US",
            "S2_ETH_SPI_HOLD_MAX_US",
            "S2_ETH_SPI_HOLD_TOTAL_US",
            "S2_ETH_SPI_HOLD_COUNT",
            "S2_MODBUS_CYCLES_OK",
            "S2_MODBUS_FAILURES",
            "S2_MODBUS_PATTERN_MISMATCHES",
            "S2_MODBUS_MAX_TRANSACTION_US",
            "S2_MODBUS_RX_FRAMES",
            "S2_MODBUS_TX_FRAMES",
            "S2_MODBUS_REQUESTS_OK",
            "S2_MODBUS_CRC_ERRORS",
            "S2_MODBUS_TIMEOUTS",
            "S2_FRAM_OK",
            "S2_FRAM_FAIL",
            "S2_RTC_OK",
            "S2_RTC_FAIL",
            "S2_SD_OK",
            "S2_SD_FAIL",
            "S2_IO_SAMPLES",
            "S2_BUTTON_DOWN_SAMPLES",
            "S2_MAX_LOOP_US",
            "S2_LONG_LOOP_CRITICAL",
            "S2_LINK_FINAL",
        )

        for key in keys:
            if key not in master_values:
                raise RuntimeError(f"S2_MASTER_MISSING_{key}")
            print(f"{key}={master_values[key]}")

        slave_keys = (
            "S2_MODBUS_RX_FRAMES",
            "S2_MODBUS_TX_FRAMES",
            "S2_MODBUS_REQUESTS_OK",
            "S2_MODBUS_CRC_ERRORS",
            "S2_FRAM_OK",
            "S2_FRAM_FAIL",
            "S2_RTC_OK",
            "S2_RTC_FAIL",
            "S2_IO_SAMPLES",
            "S2_BUTTON_DOWN_SAMPLES",
            "S2_MAX_LOOP_US",
            "S2_LONG_LOOP_CRITICAL",
        )

        for key in slave_keys:
            if key not in slave_values:
                raise RuntimeError(f"S2_SLAVE_MISSING_{key}")
            print(f"S2_SLAVE_{key.removeprefix('S2_')}={slave_values[key]}")

        duration_ms = integer(master_values, "S2_DURATION_MS")
        throughput_rx = rx_target * 8.0 / (duration_ms * 1000.0)
        throughput_echo = echo_target * 8.0 / (duration_ms * 1000.0)

        print(f"S2_ETH_RX_MBPS={throughput_rx:.6f}")
        print(f"S2_ETH_ECHO_TX_MBPS={throughput_echo:.6f}")
        print("S2_MASTER_DEVICE_RESETS=0")
        print("S2_SLAVE_DEVICE_RESETS=0")
        print("S2_GATE_DATA=PASS")
        return 0

    finally:
        if traffic is not None:
            traffic.abort()
        master.close()
        slave.close()


if __name__ == "__main__":
    raise SystemExit(main())

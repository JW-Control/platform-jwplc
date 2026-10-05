#!/usr/bin/env python3
from __future__ import annotations

import argparse
import socket
import statistics
import threading
import time
from dataclasses import dataclass

import serial

SNAPSHOT_END = "H4A04A1_SNAPSHOT=END"
CASE_END = "H4A04A1_CASE=END"


def parse_values(text: str) -> dict[str, str]:
    values: dict[str, str] = {}

    for line in text.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        values[key.strip()] = value.strip()

    values["_RAW"] = text
    return values


class DutSerial:
    def __init__(self, port: str, baudrate: int = 115200):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baudrate
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

    def _read_until(
        self,
        marker: str,
        timeout_s: float,
    ) -> str:
        deadline = time.perf_counter() + timeout_s
        raw = bytearray()
        marker_bytes = marker.encode()

        while time.perf_counter() < deadline:
            chunk = self.ser.read(
                max(1, self.ser.in_waiting)
            )
            if chunk:
                raw.extend(chunk)
                if marker_bytes in raw:
                    break

        text = raw.decode(
            "utf-8",
            errors="replace",
        )

        if marker not in text:
            raise RuntimeError(
                f"H4A04A1_SERIAL_TIMEOUT_MARKER={marker} "
                f"RAW={text!r}"
            )

        return text

    def snapshot(self) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(b"S\n")
        self.ser.flush()
        text = self._read_until(
            SNAPSHOT_END,
            2.5,
        )
        return parse_values(text)

    def command_case(
        self,
        command: str,
        timeout_s: float = 5.0,
    ) -> dict[str, str]:
        self.ser.reset_input_buffer()
        self.ser.write(
            (command + "\n").encode()
        )
        self.ser.flush()

        text = self._read_until(
            CASE_END,
            timeout_s,
        )
        return parse_values(text)


@dataclass
class ServerStats:
    accepted: int = 0
    closed: int = 0
    resets: int = 0


class TcpServerHarness:
    def __init__(self):
        self.sock = socket.socket(
            socket.AF_INET,
            socket.SOCK_STREAM,
        )
        self.sock.setsockopt(
            socket.SOL_SOCKET,
            socket.SO_REUSEADDR,
            1,
        )
        self.sock.bind(("0.0.0.0", 0))
        self.sock.listen(8)
        self.sock.settimeout(0.2)

        self.port = self.sock.getsockname()[1]
        self.stats = ServerStats()

        self._stop = threading.Event()
        self._lock = threading.Lock()
        self._thread = threading.Thread(
            target=self._run,
            name="jwplc-async-server",
            daemon=True,
        )

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        self._stop.set()

        try:
            self.sock.close()
        except OSError:
            pass

        self._thread.join(timeout=2.0)

    def snapshot(self) -> ServerStats:
        with self._lock:
            return ServerStats(
                accepted=self.stats.accepted,
                closed=self.stats.closed,
                resets=self.stats.resets,
            )

    def wait_for(
        self,
        *,
        accepted: int,
        closed: int,
        timeout_s: float = 3.0,
    ) -> ServerStats:
        deadline = time.perf_counter() + timeout_s

        while time.perf_counter() < deadline:
            stats = self.snapshot()

            if (
                stats.accepted >= accepted
                and stats.closed >= closed
            ):
                return stats

            time.sleep(0.02)

        stats = self.snapshot()
        raise RuntimeError(
            "H4A04A1_PC_SERVER_TIMEOUT "
            f"EXPECTED_ACCEPTED={accepted} "
            f"EXPECTED_CLOSED={closed} "
            f"ACTUAL={stats}"
        )

    def _run(self) -> None:
        while not self._stop.is_set():
            try:
                conn, _addr = self.sock.accept()
            except socket.timeout:
                continue
            except OSError:
                return

            with self._lock:
                self.stats.accepted += 1

            conn.settimeout(0.2)

            try:
                while not self._stop.is_set():
                    try:
                        data = conn.recv(1024)

                        if not data:
                            with self._lock:
                                self.stats.closed += 1
                            break

                    except socket.timeout:
                        continue
                    except (
                        ConnectionResetError,
                        ConnectionAbortedError,
                    ):
                        with self._lock:
                            self.stats.closed += 1
                            self.stats.resets += 1
                        break
                    except OSError:
                        break
            finally:
                try:
                    conn.close()
                except OSError:
                    pass


def local_ip_for_peer(peer_ip: str) -> str:
    probe = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    try:
        probe.connect((peer_ip, 9))
        return probe.getsockname()[0]
    finally:
        probe.close()


def wait_ready(
    dut: DutSerial,
    timeout_s: float = 30.0,
) -> dict[str, str]:
    deadline = time.perf_counter() + timeout_s
    last: dict[str, str] = {}

    while time.perf_counter() < deadline:
        last = dut.snapshot()

        if (
            last.get("ASYNC_ETH_READY") == "YES"
            and last.get("ASYNC_ETH_LINK") == "UP"
            and last.get("ASYNC_PROBE_IDLE") == "YES"
            and last.get("ASYNC_IP")
            and last.get("ASYNC_IP") != "0.0.0.0"
        ):
            return last

        time.sleep(0.25)

    raise RuntimeError(
        "H4A04A1_DUT_READY_TIMEOUT "
        f"LAST={last.get('_RAW', '')}"
    )


def require_pass(
    values: dict[str, str],
    expected_case: str,
) -> None:
    if values.get("ASYNC_CASE") != expected_case:
        raise RuntimeError(
            "H4A04A1_CASE_NAME_MISMATCH "
            f"EXPECTED={expected_case} "
            f"ACTUAL={values.get('ASYNC_CASE')}"
        )

    if values.get("ASYNC_CASE_RESULT") != "PASS":
        raise RuntimeError(
            f"H4A04A1_{expected_case}_FAILED "
            f"RAW={values.get('_RAW', '')}"
        )

    if int(values.get("ASYNC_SPI_LOCK_ERRORS", "-1")) != 0:
        raise RuntimeError(
            f"H4A04A1_{expected_case}_SPI_LOCK_ERRORS"
        )


def ivalue(
    values: dict[str, str],
    key: str,
) -> int:
    try:
        return int(values[key])
    except (KeyError, ValueError) as exc:
        raise RuntimeError(
            f"H4A04A1_INVALID_{key}"
        ) from exc


def print_run_summary(
    prefix: str,
    values: dict[str, str],
) -> None:
    fields = (
        "ASYNC_CONNECT_BEGIN_US",
        "ASYNC_CONNECT_POLL_COUNT",
        "ASYNC_CONNECT_POLL_MAX_US",
        "ASYNC_CONNECT_TOTAL_MS",
        "ASYNC_CONNECT_PENDING_OBSERVED",
        "ASYNC_FLUSH_BEGIN_US",
        "ASYNC_FLUSH_POLL_COUNT",
        "ASYNC_FLUSH_POLL_MAX_US",
        "ASYNC_FLUSH_TOTAL_MS",
        "ASYNC_FLUSH_PENDING_OBSERVED",
        "ASYNC_STOP_BEGIN_US",
        "ASYNC_STOP_POLL_COUNT",
        "ASYNC_STOP_POLL_MAX_US",
        "ASYNC_STOP_TOTAL_MS",
        "ASYNC_STOP_PENDING_OBSERVED",
        "ASYNC_CANCEL_CALL_US",
        "ASYNC_CANCEL_PENDING_OBSERVED",
        "ASYNC_LOOP_TICKS",
    )

    print(
        prefix
        + " "
        + " ".join(
            f"{key.removeprefix('ASYNC_')}={values.get(key, '')}"
            for key in fields
        )
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description="A14 H4A0.4-A1 TCP async lifecycle runner"
    )
    parser.add_argument("--serial", default="COM14")
    parser.add_argument("--run-cycles", type=int, default=3)
    args = parser.parse_args()

    dut = DutSerial(args.serial)
    server = TcpServerHarness()

    try:
        dut.open()
        ready = wait_ready(dut)

        dut_ip = ready["ASYNC_IP"]
        pc_ip = local_ip_for_peer(dut_ip)

        server.start()

        print(f"H4A04A1_DUT_IP={dut_ip}")
        print(f"H4A04A1_PC_SERVER_IP={pc_ip}")
        print(f"H4A04A1_PC_SERVER_PORT={server.port}")
        print("H4A04A1_DUT_READY=PASS")

        expected_connections = 0
        run_results: list[dict[str, str]] = []

        for index in range(1, args.run_cycles + 1):
            result = dut.command_case(
                f"RUN {pc_ip} {server.port}",
                timeout_s=6.0,
            )
            require_pass(result, "RUN")

            expected_connections += 1
            server.wait_for(
                accepted=expected_connections,
                closed=expected_connections,
                timeout_s=3.0,
            )

            run_results.append(result)

            print_run_summary(
                f"H4A04A1_RUN_RESULT=RUN={index}",
                result,
            )

        cancel_connect = dut.command_case(
            "CANCEL_CONNECT 192.0.2.1 65000",
            timeout_s=4.0,
        )
        require_pass(
            cancel_connect,
            "CANCEL_CONNECT",
        )

        if (
            cancel_connect.get(
                "ASYNC_CANCEL_PENDING_OBSERVED"
            )
            != "YES"
        ):
            raise RuntimeError(
                "H4A04A1_CANCEL_CONNECT_PENDING_NOT_OBSERVED"
            )

        print_run_summary(
            "H4A04A1_CANCEL_CONNECT_RESULT=",
            cancel_connect,
        )

        cancel_stop = dut.command_case(
            f"CANCEL_STOP {pc_ip} {server.port}",
            timeout_s=6.0,
        )
        require_pass(
            cancel_stop,
            "CANCEL_STOP",
        )

        expected_connections += 1
        server.wait_for(
            accepted=expected_connections,
            closed=expected_connections,
            timeout_s=3.0,
        )

        if (
            cancel_stop.get(
                "ASYNC_CANCEL_PENDING_OBSERVED"
            )
            != "YES"
        ):
            raise RuntimeError(
                "H4A04A1_CANCEL_STOP_PENDING_NOT_OBSERVED"
            )

        print_run_summary(
            "H4A04A1_CANCEL_STOP_RESULT=",
            cancel_stop,
        )

        final_stats = server.snapshot()

        connect_begin = [
            ivalue(item, "ASYNC_CONNECT_BEGIN_US")
            for item in run_results
        ]
        connect_poll_max = [
            ivalue(item, "ASYNC_CONNECT_POLL_MAX_US")
            for item in run_results
        ]
        connect_total = [
            ivalue(item, "ASYNC_CONNECT_TOTAL_MS")
            for item in run_results
        ]
        flush_begin = [
            ivalue(item, "ASYNC_FLUSH_BEGIN_US")
            for item in run_results
        ]
        flush_poll_max = [
            ivalue(item, "ASYNC_FLUSH_POLL_MAX_US")
            for item in run_results
        ]
        flush_total = [
            ivalue(item, "ASYNC_FLUSH_TOTAL_MS")
            for item in run_results
        ]
        stop_begin = [
            ivalue(item, "ASYNC_STOP_BEGIN_US")
            for item in run_results
        ]
        stop_poll_max = [
            ivalue(item, "ASYNC_STOP_POLL_MAX_US")
            for item in run_results
        ]
        stop_total = [
            ivalue(item, "ASYNC_STOP_TOTAL_MS")
            for item in run_results
        ]
        loop_ticks = [
            ivalue(item, "ASYNC_LOOP_TICKS")
            for item in run_results
        ]

        print(
            "H4A04A1_CONNECT_BEGIN_MAX_US="
            f"{max(connect_begin)}"
        )
        print(
            "H4A04A1_CONNECT_POLL_MAX_US="
            f"{max(connect_poll_max)}"
        )
        print(
            "H4A04A1_CONNECT_TOTAL_MEDIAN_MS="
            f"{statistics.median(connect_total):.1f}"
        )
        print(
            "H4A04A1_CONNECT_PENDING_SEEN_RUNS="
            f"{sum(item.get('ASYNC_CONNECT_PENDING_OBSERVED') == 'YES' for item in run_results)}"
        )

        print(
            "H4A04A1_FLUSH_BEGIN_MAX_US="
            f"{max(flush_begin)}"
        )
        print(
            "H4A04A1_FLUSH_POLL_MAX_US="
            f"{max(flush_poll_max)}"
        )
        print(
            "H4A04A1_FLUSH_TOTAL_MEDIAN_MS="
            f"{statistics.median(flush_total):.1f}"
        )
        print(
            "H4A04A1_FLUSH_PENDING_SEEN_RUNS="
            f"{sum(item.get('ASYNC_FLUSH_PENDING_OBSERVED') == 'YES' for item in run_results)}"
        )

        print(
            "H4A04A1_STOP_BEGIN_MAX_US="
            f"{max(stop_begin)}"
        )
        print(
            "H4A04A1_STOP_POLL_MAX_US="
            f"{max(stop_poll_max)}"
        )
        print(
            "H4A04A1_STOP_TOTAL_MEDIAN_MS="
            f"{statistics.median(stop_total):.1f}"
        )
        print(
            "H4A04A1_STOP_PENDING_SEEN_RUNS="
            f"{sum(item.get('ASYNC_STOP_PENDING_OBSERVED') == 'YES' for item in run_results)}"
        )

        print(
            "H4A04A1_LOOP_TICKS_MIN="
            f"{min(loop_ticks)}"
        )
        print(
            "H4A04A1_CANCEL_CONNECT_CALL_US="
            f"{ivalue(cancel_connect, 'ASYNC_CANCEL_CALL_US')}"
        )
        print(
            "H4A04A1_CANCEL_STOP_CALL_US="
            f"{ivalue(cancel_stop, 'ASYNC_CANCEL_CALL_US')}"
        )

        print(
            "H4A04A1_PC_ACCEPTED="
            f"{final_stats.accepted}"
        )
        print(
            "H4A04A1_PC_CLOSED="
            f"{final_stats.closed}"
        )
        print(
            "H4A04A1_PC_RESETS="
            f"{final_stats.resets}"
        )
        print("H4A04A1_FUNCTIONAL_PASS=YES")
        return 0

    finally:
        server.close()
        dut.close()


if __name__ == "__main__":
    raise SystemExit(main())

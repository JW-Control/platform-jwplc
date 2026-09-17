import argparse
import socket
import statistics
import time
from dataclasses import dataclass

import serial


SNAPSHOT_END = "ETH14_RAW_SNAPSHOT=END"


@dataclass
class BenchResult:
    mode: str
    duration_s: float
    pc_bytes: int
    dut_bytes: int
    pc_operations: int
    dut_operations: int
    errors: int
    pc_mbps: float
    dut_mbps: float
    loss_operations: int = 0
    loss_percent: float = 0.0
    wrong_size_packets: int = 0
    pass_functional: bool = False


class DutSerial:
    def __init__(
        self,
        port: str,
        baudrate: int = 115200,
    ):
        self.ser = serial.Serial()
        self.ser.port = port
        self.ser.baudrate = baudrate
        self.ser.timeout = 0.05
        self.ser.write_timeout = 1.0

        # Preserve the behavior already validated in the
        # PowerShell physical smoke gates.
        self.ser.dtr = False
        self.ser.rts = False

    def open(self):
        self.ser.open()
        time.sleep(2.0)

    def close(self):
        if self.ser.is_open:
            self.ser.close()

    def snapshot(
        self,
        timeout_s: float = 2.5,
    ) -> dict:
        self.ser.reset_input_buffer()
        self.ser.write(b"S")
        self.ser.flush()

        deadline = time.perf_counter() + timeout_s
        raw = bytearray()

        while time.perf_counter() < deadline:
            chunk = self.ser.read(
                max(1, self.ser.in_waiting)
            )

            if chunk:
                raw.extend(chunk)

                if SNAPSHOT_END.encode() in raw:
                    break

        text = raw.decode(
            "utf-8",
            errors="replace",
        )

        values = {}

        for line in text.splitlines():
            if "=" not in line:
                continue

            key, value = line.split(
                "=",
                1,
            )

            values[key.strip()] = value.strip()

        values["_RAW"] = text

        return values

    def wait_ready(
        self,
        expected_ip: str,
        timeout_s: float = 15.0,
    ) -> dict:
        deadline = time.perf_counter() + timeout_s
        last = {}

        while time.perf_counter() < deadline:
            last = self.snapshot()

            if (
                last.get("RAW_SERVER_READY") == "YES"
                and last.get("ETH_READY") == "YES"
                and last.get("ETH_LINK") == "UP"
                and last.get("IP") == expected_ip
            ):
                return last

            time.sleep(0.25)

        raise RuntimeError(
            "RAW DUT no quedó READY. "
            f"Último snapshot:\n{last.get('_RAW', '')}"
        )


def intval(
    values: dict,
    key: str,
    default: int = 0,
) -> int:
    try:
        return int(values.get(key, default))
    except (TypeError, ValueError):
        return default


def mbps(
    byte_count: int,
    elapsed_s: float,
) -> float:
    if elapsed_s <= 0:
        return 0.0

    return (
        byte_count
        * 8.0
        / elapsed_s
        / 1_000_000.0
    )


def print_snapshot_brief(
    prefix: str,
    s: dict,
):
    keys = (
        "MODE",
        "RX_BYTES",
        "TX_BYTES",
        "RX_OPERATIONS",
        "TX_OPERATIONS",
        "TRANSPORT_ERRORS",
        "LOOP_GAP_AVG_US",
        "LOOP_GAP_MAX_US",
    )

    for key in keys:
        print(
            f"{prefix}_{key}="
            f"{s.get(key, '')}"
        )


def tcp_rx_bench(
    host: str,
    port: int,
    duration_s: float,
    chunk_size: int,
    dut: DutSerial,
) -> BenchResult:
    payload = bytes(
        (i & 0xFF)
        for i in range(chunk_size)
    )

    pc_bytes = 0
    operations = 0

    sock = socket.create_connection(
        (host, port),
        timeout=3.0,
    )

    sock.settimeout(3.0)

    try:
        # R = DUT receives TCP payload.
        sock.sendall(b"R")

        # Give the DUT one scheduling opportunity to consume
        # the control byte and reset its counters.
        time.sleep(0.05)

        start = time.perf_counter()
        deadline = start + duration_s

        while time.perf_counter() < deadline:
            sock.sendall(payload)
            pc_bytes += len(payload)
            operations += 1

        elapsed = time.perf_counter() - start

        try:
            sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass

        # Allow the DUT to consume the final TCP bytes before
        # taking the serial snapshot.
        time.sleep(0.30)

    finally:
        sock.close()

    time.sleep(0.20)

    snap = dut.snapshot()

    dut_bytes = intval(
        snap,
        "RX_BYTES",
    )

    dut_ops = intval(
        snap,
        "RX_OPERATIONS",
    )

    errors = intval(
        snap,
        "TRANSPORT_ERRORS",
    )

    functional = (
        pc_bytes > 0
        and dut_bytes > 0
        and errors == 0
    )

    return BenchResult(
        mode="TCP_RX",
        duration_s=elapsed,
        pc_bytes=pc_bytes,
        dut_bytes=dut_bytes,
        pc_operations=operations,
        dut_operations=dut_ops,
        errors=errors,
        pc_mbps=mbps(
            pc_bytes,
            elapsed,
        ),
        dut_mbps=mbps(
            dut_bytes,
            elapsed,
        ),
        pass_functional=functional,
    )


def tcp_tx_bench(
    host: str,
    port: int,
    duration_s: float,
    receive_size: int,
    dut: DutSerial,
) -> BenchResult:
    pc_bytes = 0
    operations = 0

    sock = socket.create_connection(
        (host, port),
        timeout=3.0,
    )

    sock.settimeout(0.5)

    try:
        # T = DUT transmits TCP payload.
        sock.sendall(b"T")

        start = time.perf_counter()
        deadline = start + duration_s

        while time.perf_counter() < deadline:
            try:
                data = sock.recv(receive_size)
            except socket.timeout:
                continue

            if not data:
                break

            pc_bytes += len(data)
            operations += 1

        elapsed = time.perf_counter() - start

    finally:
        sock.close()

    # Let the DUT observe FIN/CLOSE before snapshot.
    time.sleep(0.35)

    snap = dut.snapshot()

    dut_bytes = intval(
        snap,
        "TX_BYTES",
    )

    dut_ops = intval(
        snap,
        "TX_OPERATIONS",
    )

    errors = intval(
        snap,
        "TRANSPORT_ERRORS",
    )

    functional = (
        pc_bytes > 0
        and dut_bytes > 0
        and errors == 0
    )

    return BenchResult(
        mode="TCP_TX",
        duration_s=elapsed,
        pc_bytes=pc_bytes,
        dut_bytes=dut_bytes,
        pc_operations=operations,
        dut_operations=dut_ops,
        errors=errors,
        pc_mbps=mbps(
            pc_bytes,
            elapsed,
        ),
        dut_mbps=mbps(
            dut_bytes,
            elapsed,
        ),
        pass_functional=functional,
    )


def udp_rx_bench(
    host: str,
    port: int,
    duration_s: float,
    payload_size: int,
    dut: DutSerial,
) -> BenchResult:
    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    # Large host buffers reduce the chance of measuring the
    # Windows socket instead of the DUT.
    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_SNDBUF,
        4 * 1024 * 1024,
    )

    target = (host, port)

    try:
        # URX resets DUT counters and selects UDP_RX.
        sock.sendto(
            b"URX",
            target,
        )

        time.sleep(0.10)

        armed = dut.snapshot()

        if armed.get("MODE") != "UDP_RX":
            raise RuntimeError(
                "DUT no entró en UDP_RX"
            )

        payload = bytearray(payload_size)

        sent_packets = 0
        sent_bytes = 0

        start = time.perf_counter()
        deadline = start + duration_s

        sequence = 0

        while time.perf_counter() < deadline:
            # Sequence marker for PC-side traceability.
            # The current DUT benchmark treats all bytes as
            # opaque payload and counts whole datagrams.
            payload[0:4] = sequence.to_bytes(
                4,
                byteorder="big",
                signed=False,
            )

            sent = sock.sendto(
                payload,
                target,
            )

            if sent == payload_size:
                sent_packets += 1
                sent_bytes += sent

            sequence = (
                sequence + 1
            ) & 0xFFFFFFFF

        elapsed = time.perf_counter() - start

        # Let W5500/DUT drain already-arrived datagrams.
        time.sleep(0.40)

        snap = dut.snapshot()

        # Stop mode only after snapshot so measured counters
        # remain untouched.
        sock.sendto(
            b"STOP",
            target,
        )

    finally:
        sock.close()

    dut_packets = intval(
        snap,
        "RX_OPERATIONS",
    )

    dut_bytes = intval(
        snap,
        "RX_BYTES",
    )

    errors = intval(
        snap,
        "TRANSPORT_ERRORS",
    )

    loss_ops = max(
        0,
        sent_packets - dut_packets,
    )

    loss_pct = (
        (loss_ops * 100.0 / sent_packets)
        if sent_packets > 0
        else 0.0
    )

    functional = (
        sent_packets > 0
        and dut_packets > 0
        and dut_bytes > 0
        and errors == 0
    )

    return BenchResult(
        mode="UDP_RX",
        duration_s=elapsed,
        pc_bytes=sent_bytes,
        dut_bytes=dut_bytes,
        pc_operations=sent_packets,
        dut_operations=dut_packets,
        errors=errors,
        pc_mbps=mbps(
            sent_bytes,
            elapsed,
        ),
        dut_mbps=mbps(
            dut_bytes,
            elapsed,
        ),
        loss_operations=loss_ops,
        loss_percent=loss_pct,
        pass_functional=functional,
    )


def drain_udp_socket(
    sock: socket.socket,
    payload_size: int,
    drain_s: float,
    expected_host: str,
    expected_port: int,
):
    packets = 0
    byte_count = 0
    wrong_size = 0

    unexpected_packets = 0
    unexpected_bytes = 0

    # ETH14 G1A D19: decode sequence tags in tail packets.
    sequence_values = []

    deadline = time.perf_counter() + drain_s

    sock.settimeout(0.02)

    while time.perf_counter() < deadline:
        try:
            packet, sender = sock.recvfrom(
                payload_size + 256
            )
        except socket.timeout:
            continue

        source_valid = (
            sender[0] == expected_host
            and sender[1] == expected_port
        )

        if not source_valid:
            unexpected_packets += 1
            unexpected_bytes += len(packet)

            if unexpected_packets <= 8:
                print(
                    "UDP_TX_UNEXPECTED_SOURCE "
                    f"PHASE=TAIL "
                    f"FROM={sender[0]}:{sender[1]} "
                    f"BYTES={len(packet)}"
                )

            continue

        if len(packet) >= 4:
            sequence_values.append(
                int.from_bytes(
                    packet[0:4],
                    byteorder="big",
                    signed=False,
                )
            )

        packets += 1
        byte_count += len(packet)

        if len(packet) != payload_size:
            wrong_size += 1

            print(
                "UDP_TX_WRONG_SIZE "
                f"PHASE=TAIL "
                f"FROM={sender[0]}:{sender[1]} "
                f"BYTES={len(packet)} "
                f"EXPECTED={payload_size}"
            )

    return (
        packets,
        byte_count,
        wrong_size,
        unexpected_packets,
        unexpected_bytes,
        sequence_values,
    )


def udp_tx_bench(
    host: str,
    port: int,
    duration_s: float,
    payload_size: int,
    dut: DutSerial,
) -> BenchResult:
    sock = socket.socket(
        socket.AF_INET,
        socket.SOCK_DGRAM,
    )

    sock.setsockopt(
        socket.SOL_SOCKET,
        socket.SO_RCVBUF,
        4 * 1024 * 1024,
    )

    # Explicit bind keeps the PC source port deterministic
    # for the duration of this case.
    sock.bind(
        ("0.0.0.0", 0)
    )

    local_port = sock.getsockname()[1]
    target = (host, port)

    pc_packets = 0
    pc_bytes = 0
    wrong_size = 0

    # ETH14 G1A sequence diagnostics.
    # Firmware writes udpTxAttempts as uint32 big-endian
    # in bytes [0..3] without changing payload length.
    sequence_values = []
    sequence_seen = set()
    sequence_duplicate_events = 0
    sequence_decode_errors = 0

    unexpected_packets = 0
    unexpected_bytes = 0

    try:
        # Defensive cleanup from an aborted previous case.
        sock.sendto(
            b"STOP",
            target,
        )

        time.sleep(0.10)

        # UTX resets DUT counters and stores this exact PC
        # source endpoint as UDP destination.
        sock.sendto(
            b"UTX",
            target,
        )

        start = time.perf_counter()
        deadline = start + duration_s

        sock.settimeout(0.20)

        while time.perf_counter() < deadline:
            try:
                packet, sender = sock.recvfrom(
                    payload_size + 256
                )
            except socket.timeout:
                continue

            source_valid = (
                sender[0] == host
                and sender[1] == port
            )

            if not source_valid:
                unexpected_packets += 1
                unexpected_bytes += len(packet)

                if unexpected_packets <= 8:
                    print(
                        "UDP_TX_UNEXPECTED_SOURCE "
                        f"PHASE=MEASURED "
                        f"FROM={sender[0]}:{sender[1]} "
                        f"BYTES={len(packet)}"
                    )

                continue

            sequence = None

            if len(packet) >= 4:
                sequence = int.from_bytes(
                    packet[0:4],
                    byteorder="big",
                    signed=False,
                )

                sequence_values.append(sequence)

                if sequence in sequence_seen:
                    sequence_duplicate_events += 1

                    if sequence_duplicate_events <= 8:
                        print(
                            "UDP_TX_SEQUENCE_DUPLICATE "
                            f"PHASE=MEASURED "
                            f"SEQ={sequence}"
                        )
                else:
                    sequence_seen.add(sequence)
            else:
                sequence_decode_errors += 1

                if sequence_decode_errors <= 8:
                    print(
                        "UDP_TX_SEQUENCE_DECODE_ERROR "
                        f"PHASE=MEASURED "
                        f"BYTES={len(packet)}"
                    )

            pc_packets += 1
            pc_bytes += len(packet)

            if len(packet) != payload_size:
                wrong_size += 1

                print(
                    "UDP_TX_WRONG_SIZE "
                    f"PHASE=MEASURED "
                    f"FROM={sender[0]}:{sender[1]} "
                    f"BYTES={len(packet)} "
                    f"EXPECTED={payload_size} "
                    f"SEQ={sequence if sequence is not None else 'NA'}"
                )

        elapsed = time.perf_counter() - start

        # Stop producer at the end of measured interval.
        sock.sendto(
            b"STOP",
            target,
        )

        # Drain only packets from the expected DUT endpoint.
        (
            tail_packets,
            tail_bytes,
            tail_wrong,
            tail_unexpected_packets,
            tail_unexpected_bytes,
            tail_sequence_values,
        ) = drain_udp_socket(
            sock,
            payload_size,
            0.25,
            host,
            port,
        )

        time.sleep(0.20)

        snap = dut.snapshot()

    finally:
        sock.close()

    dut_packets = intval(
        snap,
        "TX_OPERATIONS",
    )

    dut_bytes = intval(
        snap,
        "TX_BYTES",
    )

    errors = intval(
        snap,
        "TRANSPORT_ERRORS",
    )

    total_pc_packets = (
        pc_packets
        + tail_packets
    )

    total_pc_bytes = (
        pc_bytes
        + tail_bytes
    )

    total_wrong = (
        wrong_size
        + tail_wrong
    )

    total_unexpected_packets = (
        unexpected_packets
        + tail_unexpected_packets
    )

    total_unexpected_bytes = (
        unexpected_bytes
        + tail_unexpected_bytes
    )

    pc_minus_dut_packets = (
        total_pc_packets
        - dut_packets
    )

    loss_ops = max(
        0,
        dut_packets - total_pc_packets,
    )

    loss_pct = (
        (loss_ops * 100.0 / dut_packets)
        if dut_packets > 0
        else 0.0
    )

    print(
        f"UDP_TX_PC_LOCAL_PORT="
        f"{local_port}"
    )

    print(
        f"UDP_TX_EXPECTED_SOURCE="
        f"{host}:{port}"
    )

    print(
        f"UDP_TX_MEASURED_PACKETS="
        f"{pc_packets}"
    )

    sequence_unique = len(sequence_seen)
    sequence_duplicates = (
        len(sequence_values) - sequence_unique
    )

    sequence_zero_count = sum(
        1
        for value in sequence_values
        if value == 0
    )

    sequence_above_dut = sum(
        1
        for value in sequence_values
        if value > dut_packets
    )

    sequence_reorders = sum(
        1
        for previous, current in zip(
            sequence_values,
            sequence_values[1:],
        )
        if current < previous
    )

    if sequence_values:
        sequence_min = min(sequence_values)
        sequence_max = max(sequence_values)
        sequence_range_missing = (
            sequence_max
            - sequence_min
            + 1
            - sequence_unique
        )
    else:
        sequence_min = 0
        sequence_max = 0
        sequence_range_missing = 0

    tail_sequence_unique = len(set(tail_sequence_values))

    if tail_sequence_values:
        tail_sequence_min = min(tail_sequence_values)
        tail_sequence_max = max(tail_sequence_values)
    else:
        tail_sequence_min = 0
        tail_sequence_max = 0

    total_sequence_values = (
        sequence_values
        + tail_sequence_values
    )

    total_sequence_unique = len(
        set(total_sequence_values)
    )

    total_sequence_duplicates = (
        len(total_sequence_values)
        - total_sequence_unique
    )

    if total_sequence_values:
        total_sequence_min = min(
            total_sequence_values
        )
        total_sequence_max = max(
            total_sequence_values
        )
        total_sequence_range_missing = (
            total_sequence_max
            - total_sequence_min
            + 1
            - total_sequence_unique
        )
    else:
        total_sequence_min = 0
        total_sequence_max = 0
        total_sequence_range_missing = 0

    print("UDP_TX_SEQUENCE_SCHEME=ATTEMPT_U32_BE")

    print(
        f"UDP_TX_SEQUENCE_TAIL_COUNT="
        f"{len(tail_sequence_values)}"
    )

    print(
        f"UDP_TX_SEQUENCE_TAIL_UNIQUE="
        f"{tail_sequence_unique}"
    )

    print(
        f"UDP_TX_SEQUENCE_TAIL_MIN="
        f"{tail_sequence_min}"
    )

    print(
        f"UDP_TX_SEQUENCE_TAIL_MAX="
        f"{tail_sequence_max}"
    )

    print(
        "UDP_TX_SEQUENCE_TAIL_VALUES="
        + ",".join(
            str(value)
            for value in tail_sequence_values[:16]
        )
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_COUNT="
        f"{len(total_sequence_values)}"
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_UNIQUE="
        f"{total_sequence_unique}"
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_DUPLICATES="
        f"{total_sequence_duplicates}"
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_MIN="
        f"{total_sequence_min}"
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_MAX="
        f"{total_sequence_max}"
    )

    print(
        f"UDP_TX_SEQUENCE_TOTAL_RANGE_MISSING="
        f"{total_sequence_range_missing}"
    )

    print(
        f"UDP_TX_SEQUENCE_MEASURED_COUNT="
        f"{len(sequence_values)}"
    )

    print(
        f"UDP_TX_SEQUENCE_DECODE_ERRORS="
        f"{sequence_decode_errors}"
    )

    print(
        f"UDP_TX_SEQUENCE_UNIQUE="
        f"{sequence_unique}"
    )

    print(
        f"UDP_TX_SEQUENCE_DUPLICATES="
        f"{sequence_duplicates}"
    )

    print(
        f"UDP_TX_SEQUENCE_ZERO_COUNT="
        f"{sequence_zero_count}"
    )

    print(
        f"UDP_TX_SEQUENCE_REORDERS="
        f"{sequence_reorders}"
    )

    print(
        f"UDP_TX_SEQUENCE_MIN="
        f"{sequence_min}"
    )

    print(
        f"UDP_TX_SEQUENCE_MAX="
        f"{sequence_max}"
    )

    print(
        f"UDP_TX_SEQUENCE_RANGE_MISSING="
        f"{sequence_range_missing}"
    )

    print(
        f"UDP_TX_SEQUENCE_ABOVE_DUT="
        f"{sequence_above_dut}"
    )

    print(
        f"UDP_TX_TAIL_PACKETS="
        f"{tail_packets}"
    )

    print(
        f"UDP_TX_TOTAL_PC_PACKETS="
        f"{total_pc_packets}"
    )

    print(
        f"UDP_TX_TOTAL_PC_BYTES="
        f"{total_pc_bytes}"
    )

    print(
        f"UDP_TX_PC_MINUS_DUT_PACKETS="
        f"{pc_minus_dut_packets}"
    )

    print(
        f"UDP_TX_UNEXPECTED_SOURCE_PACKETS="
        f"{total_unexpected_packets}"
    )

    print(
        f"UDP_TX_UNEXPECTED_SOURCE_BYTES="
        f"{total_unexpected_bytes}"
    )

    print(
        f"UDP_TX_WRONG_SIZE_FROM_DUT="
        f"{total_wrong}"
    )

    functional = (
        pc_packets > 0
        and dut_packets > 0
        and errors == 0
        and total_wrong == 0
        and total_unexpected_packets == 0
        and total_pc_packets <= dut_packets
        and sequence_decode_errors == 0
        and sequence_duplicates == 0
        and sequence_zero_count == 0
        and sequence_above_dut == 0
    )

    return BenchResult(
        mode="UDP_TX",
        duration_s=elapsed,
        pc_bytes=pc_bytes,
        dut_bytes=dut_bytes,
        pc_operations=pc_packets,
        dut_operations=dut_packets,
        errors=errors,
        pc_mbps=mbps(
            pc_bytes,
            elapsed,
        ),
        dut_mbps=mbps(
            dut_bytes,
            elapsed,
        ),
        loss_operations=loss_ops,
        loss_percent=loss_pct,
        wrong_size_packets=total_wrong,
        pass_functional=functional,
    )

def print_result(
    result: BenchResult,
):
    print()
    print(
        "========================================"
    )

    print(
        f" RESULT {result.mode}"
    )

    print(
        "========================================"
    )

    print(
        f"MODE={result.mode}"
    )

    print(
        f"DURATION_S="
        f"{result.duration_s:.6f}"
    )

    print(
        f"PC_BYTES={result.pc_bytes}"
    )

    print(
        f"DUT_BYTES={result.dut_bytes}"
    )

    print(
        f"PC_OPERATIONS="
        f"{result.pc_operations}"
    )

    print(
        f"DUT_OPERATIONS="
        f"{result.dut_operations}"
    )

    print(
        f"PC_MBPS="
        f"{result.pc_mbps:.6f}"
    )

    print(
        f"DUT_MBPS="
        f"{result.dut_mbps:.6f}"
    )

    print(
        f"LOSS_OPERATIONS="
        f"{result.loss_operations}"
    )

    print(
        f"LOSS_PERCENT="
        f"{result.loss_percent:.6f}"
    )

    print(
        f"WRONG_SIZE_PACKETS="
        f"{result.wrong_size_packets}"
    )

    print(
        f"TRANSPORT_ERRORS="
        f"{result.errors}"
    )

    print(
        "FUNCTIONAL_PASS="
        f"{'YES' if result.pass_functional else 'NO'}"
    )

    print(
        "RAW_RESULT "
        f"MODE={result.mode} "
        f"PC_MBPS={result.pc_mbps:.6f} "
        f"DUT_MBPS={result.dut_mbps:.6f} "
        f"LOSS_PERCENT={result.loss_percent:.6f} "
        f"ERRORS={result.errors} "
        f"PASS={'YES' if result.pass_functional else 'NO'}"
    )


def main():
    parser = argparse.ArgumentParser(
        description=(
            "ETH14 raw W5500 transport benchmark"
        )
    )

    parser.add_argument(
        "--host",
        default="192.168.0.31",
    )

    parser.add_argument(
        "--tcp-port",
        type=int,
        default=5001,
    )

    parser.add_argument(
        "--udp-port",
        type=int,
        default=5002,
    )

    parser.add_argument(
        "--serial",
        default="COM14",
    )

    parser.add_argument(
        "--duration",
        type=float,
        default=10.0,
    )

    parser.add_argument(
        "--mode",
        choices=(
            "all",
            "tcp-rx",
            "tcp-tx",
            "udp-rx",
            "udp-tx",
        ),
        default="all",
    )

    parser.add_argument(
        "--tcp-chunk",
        type=int,
        default=4096,
    )

    parser.add_argument(
        "--udp-payload",
        type=int,
        default=1472,
    )

    args = parser.parse_args()

    if args.duration <= 0:
        raise SystemExit(
            "--duration debe ser > 0"
        )

    if args.tcp_chunk <= 0:
        raise SystemExit(
            "--tcp-chunk debe ser > 0"
        )

    if not (
        8 <= args.udp_payload <= 1472
    ):
        raise SystemExit(
            "--udp-payload debe estar entre "
            "8 y 1472 bytes"
        )

    print(
        "========================================"
    )

    print(
        " ETH14 RAW TRANSPORT BENCHMARK"
    )

    print(
        "========================================"
    )

    print(
        f"HOST={args.host}"
    )

    print(
        f"TCP_PORT={args.tcp_port}"
    )

    print(
        f"UDP_PORT={args.udp_port}"
    )

    print(
        f"SERIAL={args.serial}"
    )

    print(
        f"DURATION_S={args.duration}"
    )

    print(
        f"MODE_REQUESTED={args.mode}"
    )

    print(
        f"TCP_CHUNK={args.tcp_chunk}"
    )

    print(
        f"UDP_PAYLOAD={args.udp_payload}"
    )

    dut = DutSerial(
        args.serial
    )

    results = []

    try:
        dut.open()

        ready = dut.wait_ready(
            args.host
        )

        print(
            "DUT_READY=YES"
        )

        print(
            f"DUT_IP={ready.get('IP', '')}"
        )

        selected = []

        if args.mode == "all":
            selected = [
                "tcp-rx",
                "tcp-tx",
                "udp-rx",
                "udp-tx",
            ]
        else:
            selected = [
                args.mode
            ]

        for index, mode in enumerate(
            selected,
            start=1,
        ):
            print()
            print(
                "========================================"
            )

            print(
                f" CASE {index}/{len(selected)} "
                f"{mode.upper()}"
            )

            print(
                "========================================"
            )

            if mode == "tcp-rx":
                result = tcp_rx_bench(
                    args.host,
                    args.tcp_port,
                    args.duration,
                    args.tcp_chunk,
                    dut,
                )

            elif mode == "tcp-tx":
                result = tcp_tx_bench(
                    args.host,
                    args.tcp_port,
                    args.duration,
                    args.tcp_chunk,
                    dut,
                )

            elif mode == "udp-rx":
                result = udp_rx_bench(
                    args.host,
                    args.udp_port,
                    args.duration,
                    args.udp_payload,
                    dut,
                )

            elif mode == "udp-tx":
                result = udp_tx_bench(
                    args.host,
                    args.udp_port,
                    args.duration,
                    args.udp_payload,
                    dut,
                )

            else:
                raise RuntimeError(
                    f"Modo inesperado: {mode}"
                )

            results.append(result)
            print_result(result)

            time.sleep(0.50)

    finally:
        dut.close()

    all_pass = all(
        result.pass_functional
        for result in results
    )

    print()
    print(
        "========================================"
    )

    print(
        " ETH14 RAW BENCH SUMMARY"
    )

    print(
        "========================================"
    )

    for result in results:
        print(
            f"SUMMARY_{result.mode}_"
            f"PC_MBPS="
            f"{result.pc_mbps:.6f}"
        )

        print(
            f"SUMMARY_{result.mode}_"
            f"DUT_MBPS="
            f"{result.dut_mbps:.6f}"
        )

        print(
            f"SUMMARY_{result.mode}_"
            f"LOSS_PERCENT="
            f"{result.loss_percent:.6f}"
        )

        print(
            f"SUMMARY_{result.mode}_"
            f"PASS="
            f"{'YES' if result.pass_functional else 'NO'}"
        )

    print(
        "RAW_BENCH_FUNCTIONAL_PASS="
        f"{'YES' if all_pass else 'NO'}"
    )

    if not all_pass:
        raise SystemExit(2)


if __name__ == "__main__":
    main()
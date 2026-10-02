from __future__ import annotations

import argparse
import csv
import json
import socket
import time
from pathlib import Path

import serial

END = "A14_FINAL_UDP_FAST_SNAPSHOT=END"


def open_serial(port: str) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.05
    ser.write_timeout = 1.0
    ser.dtr = False
    ser.rts = False
    ser.open()
    time.sleep(2.0)
    return ser


def snapshot(ser: serial.Serial, timeout_s: float = 3.0) -> dict[str, str]:
    ser.reset_input_buffer()
    ser.write(b"S")
    ser.flush()
    deadline = time.perf_counter() + timeout_s
    raw = bytearray()
    while time.perf_counter() < deadline:
        chunk = ser.read(max(1, ser.in_waiting))
        if chunk:
            raw.extend(chunk)
            if END.encode() in raw:
                break
    text = raw.decode("utf-8", errors="replace")
    values: dict[str, str] = {}
    for line in text.splitlines():
        if "=" in line:
            k, v = line.split("=", 1)
            values[k.strip()] = v.strip()
    values["_RAW"] = text
    return values


def wait_ready(ser: serial.Serial) -> tuple[str, dict[str, str]]:
    deadline = time.perf_counter() + 45.0
    last: dict[str, str] = {}
    while time.perf_counter() < deadline:
        last = snapshot(ser)
        ip = last.get("IP", "")
        if (
            last.get("UDP_FAST_READY") == "YES"
            and last.get("ETH_READY") == "YES"
            and last.get("ETH_LINK") == "UP"
            and ip
            and ip != "0.0.0.0"
        ):
            return ip, last
        time.sleep(0.25)
    raise RuntimeError("UDP_FAST_READY_TIMEOUT\n" + last.get("_RAW", ""))


def reset(ser: serial.Serial) -> None:
    ser.reset_input_buffer()
    ser.write(b"R")
    ser.flush()
    deadline = time.perf_counter() + 2.0
    raw = bytearray()
    while time.perf_counter() < deadline:
        raw.extend(ser.read(max(1, ser.in_waiting)))
        if b"A14_FINAL_UDP_FAST_RESET=PASS" in raw:
            return
    raise TimeoutError("UDP_FAST_RESET_ACK_TIMEOUT")


def iv(d: dict[str, str], key: str) -> int:
    return int(d.get(key, "0"))


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--serial", default="COM14")
    ap.add_argument("--duration", type=float, default=300.0)
    ap.add_argument("--payload", type=int, default=1016)
    ap.add_argument("--output-root", required=True)
    args = ap.parse_args()

    if args.duration < 60.0:
        raise ValueError("duration debe ser >= 60 s")
    if args.payload != 1016:
        raise ValueError("final campaign fija UDP FAST payload=1016")

    root = Path(args.output_root)
    root.mkdir(parents=True, exist_ok=True)

    ser = open_serial(args.serial)
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    payload = bytes((i * 17) & 0xFF for i in range(args.payload))

    try:
        host, before = wait_ready(ser)
        (root / "preflight_snapshot.txt").write_text(
            before.get("_RAW", ""), encoding="utf-8"
        )

        reset(ser)
        time.sleep(0.10)

        sent_packets = 0
        sent_bytes = 0
        start = time.perf_counter()
        deadline = start + args.duration

        while time.perf_counter() < deadline:
            n = sock.sendto(payload, (host, 5002))
            if n == len(payload):
                sent_packets += 1
                sent_bytes += n

        elapsed = time.perf_counter() - start
        time.sleep(0.30)
        after = snapshot(ser)
        (root / "final_snapshot.txt").write_text(
            after.get("_RAW", ""), encoding="utf-8"
        )

    finally:
        sock.close()
        ser.close()

    dut_bytes = iv(after, "RX_BYTES")
    dut_packets = iv(after, "RX_PACKETS")
    errors = iv(after, "TRANSPORT_ERRORS")
    spi_errors = iv(after, "SPI_LOCK_ERRORS")
    dut_mbps = dut_bytes * 8.0 / elapsed / 1_000_000.0
    pc_mbps = sent_bytes * 8.0 / elapsed / 1_000_000.0
    loss_packets = max(0, sent_packets - dut_packets)
    loss_pct = (100.0 * loss_packets / sent_packets) if sent_packets else 0.0
    occupancy_pct = (
        100.0 * iv(after, "SPI_HOLD_TOTAL_US") / (elapsed * 1_000_000.0)
        if elapsed > 0.0 else 0.0
    )

    row = {
        "mode": "UDP_RX_FAST",
        "duration_s": elapsed,
        "payload_bytes": args.payload,
        "pc_packets": sent_packets,
        "dut_packets": dut_packets,
        "pc_bytes": sent_bytes,
        "dut_bytes": dut_bytes,
        "pc_mbps": pc_mbps,
        "dut_mbps": dut_mbps,
        "loss_packets": loss_packets,
        "loss_percent": loss_pct,
        "transport_errors": errors,
        "spi_lock_errors": spi_errors,
        "spi_hold_avg_us": iv(after, "SPI_HOLD_AVG_US"),
        "spi_hold_max_us": iv(after, "SPI_HOLD_MAX_US"),
        "spi_occupancy_pct": occupancy_pct,
        "loop_gap_avg_us": iv(after, "LOOP_GAP_AVG_US"),
        "loop_gap_max_us": iv(after, "LOOP_GAP_MAX_US"),
    }

    (root / "udp_rx_fast.json").write_text(
        json.dumps(row, indent=2), encoding="utf-8"
    )
    with (root / "SUMMARY.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(row.keys()))
        w.writeheader()
        w.writerow(row)

    print(
        "A14_FINAL_UDP_FAST "
        f"DUT_MBPS={dut_mbps:.6f} PC_MBPS={pc_mbps:.6f} "
        f"LOSS_PCT={loss_pct:.6f} SPI_OCCUPANCY_PCT={occupancy_pct:.3f} "
        f"ERRORS={errors} SPI_ERRORS={spi_errors}"
    )

    return 0 if errors == 0 and spi_errors == 0 and dut_packets > 0 else 2


if __name__ == "__main__":
    raise SystemExit(main())

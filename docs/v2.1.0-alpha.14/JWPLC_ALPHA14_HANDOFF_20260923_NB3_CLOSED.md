# JWPLC Alpha14 — Handoff post-NB3

Fecha: 2026-09-23

## CURRENT STATE

```text
BRANCH=v2.1.0-alpha.14/feature/modbus-tcp
LAST_USER_VERIFIED_HEAD=5aa4bc9011c9b6359a103bdc593c0afd3a92ca52
EFFECTIVE_SPI_HZ=26000000
TRACKED_DIRTY_COUNT=11
STAGED_COUNT=0
NB3_GLOBAL_STATUS=CLOSED_PASS
```

NB3 global audit:

```text
NB3_BOUNDED_W5100_PRIMITIVES=CLOSED_PASS
NB3_UDP_SEND_COOPERATIVE_ENGINE=CLOSED_PASS
NB3_UDP_PARSE_PACKET_HARDENING=CLOSED_PASS
NB3_TCP_LEGACY_SEND_BOUNDS=CLOSED_PASS
NB3_DNS_ASYNC_SEND_PATH=CLOSED_PASS
A14_NB3_GLOBAL_CLOSURE_AUDIT=PASS
```

Final raw compile:

```text
FINAL_RAW_COMPILE_EXIT=0
FINAL_RAW_REPO_ETHERNET_LIBRARY_USED=True
FINAL_RAW_BIN_COUNT=4
```

## CRITICAL WORKTREE NOTE

El candidato NB3 todavía vive en el working tree local.

```text
11 tracked dirty
0 staged
```

No usar `git reset --hard`, `git restore .` ni descartar el árbol antes de consolidarlo.

Dirty esperado:

```text
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/Dns.h
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetClient.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/EthernetUdp.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/jwplc_ethernet_async_tx.h
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/socket.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.cpp
JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/utility/w5100.h
tools/modbus-tcp-benchmark/firmware/eth14_raw_transport_server/eth14_raw_transport_server.ino
```

## Key physical evidence

```text
DNS begin hold: 6033 -> 1571 us (-74%)
UDP TX median: 5.142489 Mbps = 99.3% baseline
UDP RX median: 11.108036 Mbps = 97.3% baseline
parsePacket partial drain: 63 us, NEXT recovered
TCP legacy backpressure: write returned 0 at 249397 us with timeout=250 ms
SPI lock errors in directed gates: 0
```

Known TCP_RX review:

```text
HOLD_MAX_US=5238
preferred target=5000 us
hard limit=10000 us
status=REVIEW_KNOWN_8_CHUNK_RX
```

## Pending Alpha14 — do not lose

```text
P0 commit/snapshot the 11 dirty NB3 files
P1 resolve D24 OTHER_RAW_CALL_COUNT=1
P2 decide 30 MHz test vs explicit freeze of 26 MHz
P3 instrument + optimize UDP RX after NB3
P4 TX asymmetry optimization after frequency decision
P5 final realistic full-runtime qualification on final candidate
P6 explicit RTU + TCP simultaneous validation
P7 all-peripheral soak/interference matrix
P8 Arduino CLI + Arduino IDE + physical upload final
P9 PR/CI/merge/PreRelease/index/isolated install/checklist/docs
```

30 MHz remains untested. 40 MHz remains outside the agreed G2 scope unless explicitly reopened.

The historical OpenPLC/Modbus pending remains:

```text
RTU + TCP simultaneo=PENDING
JWPLC_ModbusTCP public library=DOES_NOT_EXIST
OpenPLC=EXTERNAL_OPTIONAL
```

## Next gate

```text
NEXT_GATE=NB3-H_SOURCE_SNAPSHOT_AND_COMMIT
```

Do not begin UDP RX tuning before consolidating the already-validated candidate.

After that:

```text
NEXT_DECISION=G2_30MHZ_OR_FREEZE_26MHZ
THEN=UDP_RX_PERF_G1_INSTRUMENT_ONLY
```

## Working method

```text
ONE_GATE_AT_A_TIME=YES
GATE_AS_CODE=YES
VERSIONED_TOOLING=YES
INLINE_GIANT_SCRIPT=NO
LOG_FIRST=YES
```

Classify failures as harness/product/hardware/environment before touching firmware.

Do not remove normal peripherals for performance.
Do not inflate timeouts to hide starvation.
Do not combine SPI frequency + algorithm changes.
Do not assume OTA/final FlashFreq/definitive bootloader.

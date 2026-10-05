# Alpha14 — G3A candidato INT interno Modbus TCP — PREP 2026-10-01

## Punto de partida

G2-R1 cerró con evidencia repetible favorable al uso de INTn del W5500 como
guía del scheduler TCP.

En CONTROLLED, la mediana pasó de 65.886 % a 16.555 % de ocupación SPI
(-49.331 pp), con reducción de 90.205 % en holds/status y throughput
prácticamente idéntico (-0.001 %).

En SATURATED, INT también redujo ocupación y holds, pero la mediana de
throughput sólo mejoró +1.489 %, por debajo del umbral previamente fijado de
+2 %. Por tanto no se declara ganancia de ceiling RAW.

Reconnect/liveness volvió a pasar.

## Decisión de arquitectura

INT no se integra transparentemente dentro de `EthernetClient`.

El candidato vive en el scheduler interno de `JWPLC_ModbusTCP`, que sí conoce:

- si existe un cliente activo;
- qué socket W5500 le pertenece;
- el timeout de una trama parcial;
- cuándo una conexión se cierra o debe reaceptarse;
- cuándo una pasada RX puede omitirse sin cambiar la API Arduino.

La API pública de `JWPLC_ModbusTCP` no cambia.

## Hardware

Se incorpora al perfil hardware del JWPLC Basic v2:

```text
JWPLC_ETH_INT_PIN=15
W5500_INTN_ACTIVE=LOW
```

GPIO15 queda reservado internamente para INTn del W5500. Los aliases genéricos
heredados del core ESP32 se conservan por compatibilidad, pero no se interpreta
GPIO15 como E/S libre del usuario en esta placa.

## Candidato G3

El código productivo contiene ahora:

```text
JWPLC_MODBUS_TCP_INT_GUIDED_RX=0
```

por defecto.

Por tanto el commit del candidato no cambia todavía el comportamiento normal
del package.

Cuando el flag vale 1:

1. se arma `RECV | DISCON | TIMEOUT` en el socket activo;
2. la ISR sólo marca pending y nunca accede a SPI;
3. sin pending ni INT LOW se evita adquirir el shared SPI;
4. existe un fallback acotado de 10 ms para impedir un stall indefinido si se
   perdiera un wake;
5. tras el servicio se limpia el evento y se consulta una vez `Sn_RX_RSR`;
6. si queda RX o INT continúa LOW, pending se rearma;
7. disconnect/fatal frame desarma la máscara del socket;
8. si el mecanismo INT no puede configurarse, Modbus TCP continúa mediante
   polling.

La búsqueda/aceptación inicial del server permanece en la ruta existente para
reducir el scope del cambio.

## G3A físico

G3A deja atrás el RAW TCP de G2 y prueba Modbus TCP real:

```text
FC=03
QUANTITY=125 registers
TARGET=1000 req/s
DURATION=30 s por corrida
RUNS=2 por variante
ORDER=POLLING,INT_GUIDED,INT_GUIDED,POLLING
FRESH_UPLOAD_PER_CASE=YES
```

Ambas variantes activan los hooks diagnósticos del backend Ethernet para medir
status/available. El flag productivo de INT sólo cambia entre 0 y 1 en el
build de cada variante.

## Criterios

Cada corrida debe completar 30000/30000 requests válidos, sin timeout,
transport error, protocol error ni bus-lock timeout.

Para considerar el candidato listo para H3E-R:

```text
POLLING_REQ_S_MEDIAN >= 995
INT_REQ_S_MEDIAN >= 995
STATUS_CALL_REDUCTION >= 50 %
AVAILABLE_CALL_REDUCTION >= 40 %
P95_INT <= max(P95_POLLING * 1.20, P95_POLLING + 200 us)
P99_INT <= max(P99_POLLING * 1.30, P99_POLLING + 500 us)
```

El gate no promueve el default.

Si termina:

```text
G3A_PRODUCT_CANDIDATE_READY_FOR_H3ER=YES
```

el siguiente gate será G3B: H3E-R con TCP 1000 req/s + RTU 50 Hz + DataLog +
Display/TFT + FRAM + RTC + I/O + botonera.

Sólo después de esa regresión full-runtime se decidirá si
`JWPLC_MODBUS_TCP_INT_GUIDED_RX` puede pasar a default ON.

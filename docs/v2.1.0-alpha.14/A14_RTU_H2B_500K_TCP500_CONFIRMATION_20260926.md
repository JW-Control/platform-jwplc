# Alpha14 — RTU-H2B — Confirmacion 500 kbaud + TCP500

Fecha: 2026-09-26

## Objetivo

Confirmar bajo carga Ethernet sostenida los gaps candidatos de H2 y elegir la
zona de trabajo para el futuro perfil JWPLC_FAST/AUTO.

No se recompila ni se vuelve a subir firmware: se reutiliza el firmware H2
recien validado fisicamente.

## Configuracion fija

```txt
BAUD=500000
CONFIG=8N1
MOTOR=ASYNC
TX=QUEUED
TCP_TARGET=500 req/s
TCP_FC03_QUANTITY=125
FULL_RUNTIME=ACTIVE
RS485=A/B/GND
W5500=26 MHz
DURATION_PER_CASE=60 s
```

## Gaps

| Gap | Papel |
|---:|---|
| 500 us | baseline H1/H2 |
| 150 us | candidato con margen amplio |
| 100 us | candidato equilibrado |
| 75 us | candidato cercano a 3.5 caracteres fisicos a 500 kbaud |
| 50 us | extremo experimental limpio de H2 |

## Criterios por punto

- TCP >= 99 % del target;
- cero errores/timeout/protocolo TCP;
- cero fallos/timeout/CRC RTU;
- igualdad Master/Slave de requests procesados;
- SD/perifericos limpios;
- baud efectivo 500000;
- motor ASYNC;
- TX queued;
- gap efectivo correcto.

Solo 500 us es control obligatorio. Si un gap agresivo falla, el sweep continua
y queda caracterizado.

## Salida

El runner reporta para cada punto:

- TCP req/s y cumplimiento;
- TCP AVG/P95/P99;
- RTU Hz;
- fallos/timeout/CRC;
- estado de runtime;
- ganancia RTU vs 500 us;
- delta de latencia TCP vs 500 us.

Tambien informa el menor gap limpio y el punto RTU mas rapido, pero esos datos
no seleccionan automaticamente el default.

## Decision posterior

La seleccion JWPLC_FAST/AUTO para 500 kbaud priorizara:

1. estabilidad;
2. margen temporal;
3. convivencia TCP;
4. rendimiento RTU.

El minimo absoluto de H2/H2B no se adopta automaticamente como valor de
producto.

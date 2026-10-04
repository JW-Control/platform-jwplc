# Alpha14.3 — Diagnóstico ARP / neighbor rebuild

Fecha: 2026-09-13

## Objetivo

Determinar si los timeouts de reconexión TCP observados después de varios minutos de `FULL_RUNTIME_REALISTIC` podían explicarse por una entrada ARP/neighbor ausente, envejecida o reconstruida lentamente en el PC.

## Metodología

Se ejecutó el diagnóstico desde PowerShell elevado en Windows, sin reflashear ni resetear el JWPLC.

Precondición:

- JWPLC accesible en `192.168.0.31`;
- ping inicial correcto;
- FC03 inicial correcto;
- entrada neighbor existente y `Reachable`;
- MAC esperada con prefijo JWPLC `02-4A-57`.

Luego se eliminó de forma explícita la entrada neighbor de `192.168.0.31` y se probó inmediatamente una conexión TCP/FC03 sin ping previo. Después se verificó la reconstrucción de la MAC y se repitió el ciclo de borrado/reconstrucción seis veces.

## Evidencia física

Estado inicial:

```text
IP=192.168.0.31
MAC=02-4A-57-2F-56-28
STATE=Reachable
EXPECTED_MAC_PREFIX_CHECK=PASS
```

Después de eliminar la entrada neighbor:

```text
MAC=00-00-00-00-00-00
STATE=Unreachable
ARP_DELETE=PASS
```

Primer TCP inmediatamente después del borrado:

```text
TCP_CONNECT=PASS
CONNECT_MS=6.562
FC03=PASS
```

La entrada neighbor se reconstruyó inmediatamente como:

```text
MAC=02-4A-57-2F-56-28
STATE=Reachable
```

Los seis ciclos adicionales de `delete -> TCP -> neighbor` pasaron sin fallos. Los tiempos de conexión estuvieron entre aproximadamente 4.9 ms y 29.9 ms.

Resumen:

```text
ARP_REBUILD_CYCLES=6
TCP_FAILURES_AFTER_FORCED_ARP_DELETE=0
PING_FAILURES_AFTER_FORCED_ARP_DELETE=0
MAC_MISMATCH_COUNT=0
FIRST_TCP_EXIT=0
PING_EXIT=0
SECOND_TCP_EXIT=0
A14_3_ARP_REBUILD=PASS_IMMEDIATE
WORKTREE_AFTER=PASS
REFLASH=NO
DEVICE_RESET=NO
PRODUCTION_SOURCE_CHANGES=0
```

## Clasificación

```text
A14_3_ARP_REBUILD=PASS_PHYSICAL
ARP_REBUILD_LATENCY=NOT_REPRODUCED
DUPLICATE_IP_OR_MAC_MISMATCH=NOT_OBSERVED
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
```

## Interpretación

La ausencia de una entrada ARP/neighbor en Windows no reproduce el patrón de tres timeouts observado durante el runtime envejecido. El primer SYN/FC03 posterior al borrado fuerza y completa correctamente la resolución ARP.

Por tanto, una reconstrucción ARP normal en el PC deja de ser la hipótesis principal. La evidencia tampoco muestra cambio de MAC ni indicios de IP duplicada durante esta prueba.

La causa del outage envejecido sigue sin identificarse. Dado que T1 DHCP forzado también pasó sin pérdida de ping/TCP y que el renew real del router ocurre en una escala de días, el siguiente A/B debe separar el uso de DHCP de la configuración IP estática bajo el mismo `FULL_RUNTIME_REALISTIC` envejecido.

## Siguiente gate

Ejecutar `FULL_RUNTIME_REALISTIC` con IP estática, mantener aproximadamente 480 s sin tráfico de red y probar el primer reaccept TCP. El resultado se comparará con la reproducción ya obtenida usando DHCP bajo runtime envejecido.

El long run de 1000 req/s permanece en `HOLD` hasta clasificar esta condición.
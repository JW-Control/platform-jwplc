# Alpha14.3 — diagnóstico TCP reaccept

Fecha: 2026-09-13

## Estado

`A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED`

La prueba diagnóstica de reconexión reprodujo el problema observado durante los benchmarks de rendimiento: tras declarar el firmware `SERVER_READY=YES` y `CLIENT_CONNECTED=NO`, los primeros intentos TCP hacia `192.168.0.31:502` pueden expirar antes de que el servidor vuelva a aceptar conexiones normalmente.

## Evidencia física

Probe: 12 ciclos de conexión + FC03, `CONNECT_TIMEOUT_S=2.0`, idle inicial de 10 s.

Resultados:

- ciclos totales: 12;
- ciclos PASS: 9;
- fallos de conexión: 3;
- fallos de protocolo: 0;
- mínimo de conexión en los ciclos exitosos: 0.429 ms;
- promedio de conexión en los ciclos exitosos: 5.982 ms;
- máximo de conexión en los ciclos exitosos: 21.541 ms.

Secuencia inicial:

- ciclo 1: TCP timeout ~2015.9 ms, ping posterior FAIL;
- ciclo 2: TCP timeout ~2009.3 ms, ping posterior FAIL;
- ciclo 3: TCP timeout ~2010.7 ms, ping posterior PASS;
- ciclo 4: TCP PASS en ~11.1 ms + FC03 PASS;
- ciclos 5..12: TCP + FC03 PASS.

En los tres fallos el snapshot del firmware seguía reportando:

- `SERVER_READY=YES`;
- `CLIENT_CONNECTED=NO`;
- `FULL_RUNTIME_READY=YES`.

## Interpretación actual

El problema no se limita necesariamente al listener Modbus TCP. En los dos primeros fallos también falló ICMP, por lo que existe una ventana en la que el estado lógico/cacheado del runtime indica Ethernet/servidor listo mientras la interfaz W5500 todavía no responde de forma fiable a nivel IP.

El tercer fallo es distinto: ICMP ya respondió pero TCP/502 todavía expiró. Esto sugiere que puede haber dos etapas de recuperación: primero conectividad IP y luego disponibilidad efectiva del socket `LISTEN`.

El flag de `serverReady()` actualmente depende de `_serverEnabled`, `_serverListening` y `JWPLC_Ethernet.isReady()`. Es necesario contrastar ese estado lógico con los estados físicos de los sockets W5500 antes de cerrar el pendiente.

## Decisión

- No ejecutar todavía el long-run `FULL_RUNTIME_REALISTIC @ 1000 req/s`.
- Mantener `A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED`.
- Siguiente diagnóstico de red: instrumentar estados físicos de sockets W5500 (`CLOSED`, `LISTEN`, `ESTABLISHED`, `CLOSE_WAIT`, `TIME_WAIT`, etc.) y estado efectivo de Ethernet durante la ventana de fallo.
- En paralelo, revisar la latencia observada de microSD antes del long-run, ya que el perfil integrado mostró operaciones de ~28–29 ms.

## Integridad

El probe fue diagnóstico únicamente:

- `SOURCE_CHANGES=0`
- `FIRMWARE_CHANGES=0`
- `WORKTREE_AFTER=PASS`
- `MBLOCK_POC_TOUCHED=NO`

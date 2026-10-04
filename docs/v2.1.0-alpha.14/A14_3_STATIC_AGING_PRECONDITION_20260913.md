# Alpha14.3 — Precondición del A/B STATIC vs DHCP

Fecha: 2026-09-13

## Resultado

`A14_3_STATIC_AGED_REACCEPT=NOT_RUN_PRECONDITION`

La prueba de aging de 480 s en modo estático **no llegó a ejecutarse**. El firmware temporal compiló y se cargó sin warnings, pero la reconfiguración DHCP -> STATIC se intentó demasiado pronto, al inicio de `setup()`.

## Evidencia observada

Durante `wait_ready()` el snapshot permaneció en:

- `ETH_MODE=DHCP`
- `ETH_READY=YES`
- `ETH_IP=192.168.0.31`
- `SERVER_READY=NO`
- `FULL_RUNTIME_READY=NO`
- `FRAM_READY=NO`
- `FRAM_FAILS` creciente

El timeout final ocurrió antes de iniciar el aging.

## Interpretación

El core ejecuta `initPeripherals()` y luego `setup()`. La tarea `jwplcSystemTask`, que da servicio periódico a Ethernet mediante `jwplcEthernetTickCallback() -> JWPLC_Ethernet.service()`, permanece bloqueada por `initSemaphore` hasta que `setup()` termina.

Por lo tanto, al intentar capturar IP/gateway y convertir a STATIC al comienzo de `setup()`, la negociación DHCP cooperativa del autoload aún puede no haber terminado. Si la precondición de IP/gateway resulta inválida, el sketch temporal retorna antes de:

- preparar FRAM para el benchmark,
- configurar TFT del perfil,
- abrir el servidor Modbus TCP.

Tras retornar `setup()`, `jwplcSystemTask` se habilita y DHCP puede completar normalmente, lo que explica que los snapshots posteriores muestren `ETH_MODE=DHCP` e IP válida aunque el servidor y la FRAM del harness nunca hayan sido inicializados.

Los `FRAM_FAILS` observados no constituyen una regresión de FRAM: son consecuencia de haber retornado antes de inicializar `framReady` en el harness temporal.

## Decisión

No se extrae ninguna conclusión del A/B STATIC vs DHCP a partir de este intento.

Antes de repetir los 480 s se debe validar una precondición corta:

1. dentro de `setup()`, avanzar cooperativamente Ethernet con `JWPLC_Ethernet.service()` hasta obtener DHCP operativo;
2. capturar IP/DNS/gateway/subnet válidos;
3. reconfigurar a STATIC con esos mismos valores;
4. confirmar `ETH_MODE=STATIC`, `ETH_READY=YES`, IP esperada;
5. continuar el `setup()` original y comprobar `FRAM_READY=YES`, `FULL_RUNTIME_READY=YES`, `SERVER_READY=YES`;
6. sólo entonces ejecutar el aging de 480 s sin reflashear.

## Estado relacionado

- `A14_3_DHCP_T1_CONNECTIVITY=PASS_NO_OUTAGE_DURING_FORCED_RENEW`
- `A14_3_ARP_REBUILD=PASS_IMMEDIATE`
- `A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED`
- `LONG_RUN=ON_HOLD`

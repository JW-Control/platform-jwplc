# Alpha14 — Keepalive L2 periódico evita la ventana de reaccept

Fecha: `2026-09-13`

## Objetivo

Validar si una transmisión Ethernet saliente mínima y periódica desde el JWPLC evita la pérdida temporal de conectividad observada después de varios minutos de inactividad, sin borrar la entrada neighbor del PC y sin reiniciar el dispositivo.

## Condiciones

```text
PROFILE=FULL_RUNTIME_REALISTIC
IP_MODE=STATIC
TARGET_IP=192.168.0.31
AGING_S=480
KEEPALIVE_PERIOD_S=120
EXPECTED_KEEPALIVES=3
KEEPALIVE_TRANSPORT=UDP
KEEPALIVE_TARGET=GATEWAY_192.168.0.1
REFLASH=NO
DEVICE_RESET=NO
PRODUCTION_SOURCE_CHANGES=0
```

El firmware temporal ya cargado exponía el comando Serial `U`, que envía un datagrama UDP mínimo hacia el gateway. El helper no forma parte del código productivo.

## Precondición

```text
INITIAL_ETH_MODE=STATIC
INITIAL_ETH_READY=YES
INITIAL_SERVER_READY=YES
INITIAL_FULL_RUNTIME_READY=YES
INITIAL_PERIPHERAL_FAILURE_COUNT=0
```

Se verificó también una petición Modbus TCP FC03 antes de iniciar el aging:

```text
WARM_TCP=PASS
WARM_FC03=PASS
WARM_CONNECT_MS=3.131
```

## Keepalives

Los tres keepalives previstos se enviaron correctamente a los 120, 240 y 360 segundos:

```text
KEEPALIVE_1=PASS
KEEPALIVE_2=PASS
KEEPALIVE_3=PASS
KEEPALIVES_PASS=3/3
```

Duraciones observadas del envío dentro del JWPLC:

```text
KEEPALIVE_1_SEND_US=1105
KEEPALIVE_2_SEND_US=750
KEEPALIVE_3_SEND_US=883
```

La entrada neighbor del PC permaneció con la MAC correcta y en estado envejecido durante la ventana; el keepalive no la refrescó directamente:

```text
MAC=02-4A-57-2F-56-28
NEIGHBOR_STATE_AFTER_KEEPALIVES=STALE
```

## Resultado después de 480 s

Al terminar el aging:

```text
AGED_ETH_READY=YES
AGED_SERVER_READY=YES
AGED_FULL_RUNTIME_READY=YES
AGED_PERIPHERAL_FAILURE_COUNT=0
```

El primer TCP posterior entró a la primera, sin borrar ARP/neighbor y sin ping previo:

```text
FIRST_TCP_CONNECTED=YES
FIRST_TCP_CONNECT_MS=22.520
FIRST_FC03_PASS=YES
```

Dos conexiones posteriores también pasaron:

```text
FOLLOWUP_TID_2=PASS
FOLLOWUP_TID_3=PASS
FOLLOWUP_PASS=2/2
```

Estado final:

```text
FINAL_ETH_READY=YES
FINAL_SERVER_READY=YES
FINAL_FULL_RUNTIME_READY=YES
FINAL_PERIPHERAL_FAILURE_COUNT=0
```

## Clasificación

```text
A14_3_PERIODIC_L2_KEEPALIVE=PASS_PREVENTED_REACCEPT_OUTAGE
MITIGATION_HYPOTHESIS=PERIODIC_OUTBOUND_L2_REFRESH_EFFECTIVE
DHCP_ROOT_CAUSE=NO
MODBUS_PROTOCOL_ROOT_CAUSE=NO
FULL_RUNTIME_FAILURE=NO
ROOT_LAYER_SUSPECT=NETWORK_FDB_OR_UNKNOWN_UNICAST_AGING
```

## Interpretación

Este resultado complementa dos evidencias anteriores:

1. tras `480 s` sin tráfico, el reaccept falló también en IP estática;
2. borrar la entrada neighbor del PC y obligar una resolución ARP nueva rescató la conexión inmediatamente;
3. una única transmisión saliente del JWPLC después del aging también rescató el primer TCP sin modificar el neighbor del PC;
4. mantener una transmisión saliente cada `120 s` evitó completamente la ventana de reaccept durante el mismo aging de `480 s`.

La hipótesis principal pasa a ser aging de forwarding/FDB/unknown-unicast en la infraestructura de red usada durante la prueba. El W5500, el listener Modbus TCP y el runtime completo permanecen operativos durante el evento.

## Decisión provisional

Se acepta `120 s` como período de mitigación físicamente validado para el siguiente prototipo de implementación productiva.

La implementación productiva deberá ser:

- cooperativa;
- compatible con DHCP y configuración estática;
- sin bloquear el runtime;
- protegida por el mutex SPI compartido;
- sin consumir permanentemente un socket W5500;
- con puerto local efímero;
- con posibilidad de desactivar la mitigación;
- sin convertir un fallo del keepalive en pérdida de `READY` de Ethernet.

## Pendientes obligatorios posteriores

```text
PRODUCTION_L2_KEEPALIVE=NOT_IMPLEMENTED_YET
PRODUCTION_L2_KEEPALIVE_PHYSICAL_VALIDATION=PENDING
FINAL_FULL_RUNTIME_1000RPS_60S=PENDING_MANDATORY
FINAL_FULL_RUNTIME_1000RPS_30MIN=PENDING_MANDATORY
LONG_RUN_1000RPS=ON_HOLD
```

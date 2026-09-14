# Alpha14.3 — Diagnóstico de conectividad durante T1 DHCP

Fecha: 2026-09-13

## Objetivo

Comprobar si la pérdida temporal de conectividad observada tras varios minutos de runtime estaba asociada al mantenimiento DHCP T1 (`renew`).

La hipótesis surgió porque el fallo envejecido apareció después de aproximadamente 480 s y la implementación DHCP dispone de un lease por defecto de 900 s cuando el servidor no informa otro valor, lo que produciría un T1 por defecto de 450 s. Era necesario separar correlación temporal de causalidad.

## Metodología

Se construyó un sketch temporal con `JWPLC_ETHERNET_ENABLE_TEST_HOOKS=1` usando los hooks ya existentes de Alpha6 para:

- leer los timers T1/T2 reales entregados por el router;
- verificar conectividad Modbus TCP antes de intervenir;
- forzar T1 a 5 s y T2 a 60 s;
- observar el modo de mantenimiento, estado pendiente y código diagnóstico durante el renew;
- ejecutar en paralelo ping y una transacción FC03 cada ~0.5 s;
- verificar que Ethernet, IP y Modbus TCP siguieran operativos durante y después del renew.

No se modificó código productivo ni librerías del package.

## Resultado físico

Compilación:

```text
BUILD_UPLOAD_EXIT=0
WARNING_COUNT=0
WORKTREE_AFTER=PASS
LIBRARY_SOURCE_CHANGES=0
PRODUCTION_SOURCE_CHANGES=0
PRECOMPILED_OVERRIDE_REQUIRED=NO
```

Timers reales iniciales:

```text
INITIAL_RENEW_SEC=302398
INITIAL_REBIND_SEC=529198
INITIAL_ETH_READY=YES
INITIAL_LOCAL_IP=192.168.0.31
WARM_FC03=PASS
```

Los valores convergen inmediatamente después del renew a aproximadamente:

```text
RENEW=302400 s
REBIND=529200 s
```

Estos valores son compatibles con un lease de aproximadamente 604800 s (7 días), con T1≈50 % y T2≈87.5 %.

Por tanto, el fallo observado tras 480 s no puede corresponder al T1 natural del lease recibido actualmente.

## T1 forzado

Se forzó:

```text
FORCE_T1_5S=PASS
LEASE_RENEW_SEC=5
LEASE_REBIND_SEC=60
```

El renew comenzó correctamente:

```text
MODE=1
PENDING=YES
DIAG=DHC
```

La ventana de mantenimiento se observó aproximadamente entre 5.5 s y 7.6 s desde el inicio de la prueba.

Durante toda la transición:

```text
PING_FAILURES=0
TCP_FAILURES=0
SAW_RENEW_MODE=YES
SAW_MAINT_PENDING=YES
SAW_DHC=YES
FINAL_ETH_READY=YES
FINAL_LOCAL_IP=192.168.0.31
```

Resultado:

```text
A14_DHCP_T1_CONNECTIVITY=PASS_NO_OUTAGE_DURING_FORCED_RENEW
```

## Conclusión

La hipótesis de que el fallo envejecido esté causado directamente por el mantenimiento DHCP T1 queda descartada para el entorno físico probado.

Evidencias clave:

1. El T1 natural real es del orden de 302400 s, no ~450 s.
2. El fallo envejecido aparece tras ~480 s, muy lejos del T1 real.
3. Un renew T1 forzado y observado explícitamente no produjo ni un solo fallo de ping o Modbus TCP.
4. Ethernet permaneció `READY` y conservó `192.168.0.31` durante y después del renew.

Clasificación:

```text
DHCP_T1_AS_ROOT_CAUSE=NOT_SUPPORTED
DHCP_T1_FORCED_CONNECTIVITY=PASS_PHYSICAL
A14_3_TCP_REACCEPT_LATENCY=REVIEW_CONFIRMED
ROOT_CAUSE=STILL_OPEN
```

## Nueva hipótesis prioritaria

La secuencia del fallo envejecido es más compatible con una condición de vecindad L2/ARP o disponibilidad de ruta local tras inactividad:

- tres conexiones TCP iniciales hacen timeout;
- los dos primeros intentos tampoco responden a ping;
- en el tercero ping ya se recupera;
- el cuarto intento TCP conecta de inmediato;
- durante todo ese tiempo el W5500 conserva IP válida y un socket físico `LISTEN:502`.

Esto no demuestra aún un problema ARP, pero lo convierte en el siguiente candidato a aislar antes de volver a los benchmarks largos.

## Siguiente gate

Caracterizar la tabla de vecinos/ARP del PC y la respuesta del JWPLC después de inactividad, intentando reproducir o acelerar la condición mediante eliminación controlada de la entrada ARP/neighbor sin modificar firmware productivo.

El long run de 1000 req/s continúa en `HOLD` hasta cerrar esta causa de conectividad intermitente.

# A14.2 — Backend TCP async — Compile gate

Fecha: `2026-09-11`

## Objetivo

Confirmar que la nueva API cooperativa de `EthernetClient` compila contra el package local del JWPLC Basic sin depender todavía de la state machine Modbus TCP Client.

## API validada

```text
beginConnectAsync(IP, port)
pollConnectAsync()
connectAsyncInProgress()
cancelConnectAsync()
```

## Evidencia

```text
FQBN=jwplc_local:esp32:jwplcbasic
PLATFORM=jwplc_local:esp32 2.1.0-dev
ASYNC_API_COMPILE_EXIT=0
A14_2_ASYNC_BACKEND_SOURCE=PASS
A14_2_ASYNC_BACKEND_COMPILE=PASS
```

La resolución de librerías mostró `JWPLC_Ethernet 1.0.0` desde el árbol local `platform-jwplc/JWPLC/2.1.0/libraries`.

## Nota sobre el primer intento

Un primer intento del harness PowerShell no constituyó una ejecución válida del gate: la variable `$probe` ya estaba tipada como `Byte[]` por una prueba anterior y PowerShell intentó convertir el texto del sketch a bytes. El archivo temporal terminó conteniendo `0` y Arduino falló en la primera línea.

```text
FIRST_ATTEMPT=HARNESS_ERROR
PRODUCT_CODE_FAILURE=NO
```

Se corrigió únicamente el harness usando una variable nueva y escritura UTF-8 sin BOM. El probe corregido compiló con exit code `0`.

## Próximo gate

```text
A14_2_ASYNC_BACKEND_RUNTIME
```

Debe probar en hardware real que `beginConnectAsync()` retorna sin esperar el establecimiento completo, que `pollConnectAsync()` progresa por llamadas sucesivas y que el loop continúa ejecutándose durante la conexión.

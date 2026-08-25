# Alpha5 - validación de contención SPI durante arranque

Fecha: 2026-08-24.

Rama: `v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

## Objetivo

Repetir sobre el estado casi final de Alpha5 el diagnóstico de contención SPI realizado en Alpha4, con especial atención al `SPI lock timeout` observado de forma transitoria durante algunas pruebas recientes.

El bus SPI compartido contiene:

- TFT.
- W5500 Ethernet.
- microSD.
- FRAM.

El probe evita consultar durante las muestras APIs de estado Ethernet que por sí mismas toman el mutex SPI (`statusString()`, `hardwareStatus()`, `linkStatus()`, `localIP()`), para no contaminar la medición.

Sketch:

```txt
tools/build-speed-benchmark/sketches/14_spi_startup_probe/14_spi_startup_probe.ino
```

## Escenario A - RJ45 desconectado

Build/upload:

```txt
FQBN: jwplc_local:esp32:jwplcbasic
BUILD/UPLOAD EXIT CODE: 0
```

Condiciones:

- microSD insertada.
- cable Ethernet desconectado.
- autoload normal del package.
- `JW_SD` y `SD` en estado precompilado adoptado.
- `JWPLC_Display` y `JW_RTC` desde fuente por decisión de compatibilidad.

### Entrada a loop

```txt
Display ready al entrar a loop: NO
SD ready al entrar a loop: YES
```

Durante STARTUP Ethernet todavía no había intentado inicialización:

```txt
ETH attempted=NO ready=NO error=OK
```

Las 8 muestras STARTUP fueron correctas:

```txt
FRAM OK/FAIL: 8/0
FRAM max us: 138
SD OK/FAIL: 8/0
SD max us: 32
ETH SPI lock timeout observado: 0
```

### Transición de inicialización

Alrededor de 1 s se inicializó el Display y Ethernet comenzó su intento de arranque sin enlace físico.

Muestras representativas del primer run:

```txt
[STABLE 1] t=1000 ms | ETH attempted=NO ready=NO error=OK       | FRAM=OK 22232 us | SD=OK 17 us
[STABLE 2] t=1043 ms | ETH attempted=SI ready=NO error=Link OFF | FRAM=OK 18616 us | SD=OK 15 us
[STABLE 3] t=1082 ms | ETH attempted=SI ready=NO error=Link OFF | FRAM=OK 38 us    | SD=OK 12 us
[STABLE 4] t=1103 ms | ETH attempted=SI ready=NO error=Link OFF | FRAM=OK 15151 us | SD=OK 10 us
```

Resumen del primer run:

```txt
FRAM OK/FAIL: 10/0
FRAM max us: 22232
SD OK/FAIL: 10/0
SD max us: 17
ETH SPI lock timeout observado: 0
```

Una reapertura posterior del monitor confirmó nuevamente 10/10 operaciones FRAM y 10/10 SD, con máximo FRAM de 18617 us y sin timeout Ethernet.

### Redraw de Display

```txt
FRAM OK/FAIL: 10/0
FRAM max us: 38
SD OK/FAIL: 10/0
SD max us: 13
ETH SPI lock timeout observado: 0
```

Resultado global:

```txt
SPI_STARTUP_PROBE=PASS
SPI_STARTUP_PROBE=DONE
```

## Comparación con Alpha4

Antes de corregir Alpha4, FRAM agotaba aproximadamente 50 ms y SD aproximadamente 100 ms repetidamente durante el intento Ethernet; los fallos desaparecían sólo cuando finalizaba la retención del bus asociada al `delay(560)` heredado del backend.

Después del fix de Alpha4, la primera operación FRAM llegó a esperar aproximadamente 27.9 ms sin fallar, comportamiento considerado correcto porque el mutex seguía activo y el bus estaba legítimamente ocupado.

En Alpha5, escenario sin RJ45:

- no se reproduce ningún FAIL de FRAM;
- no se reproduce ningún FAIL de SD;
- no se observa `SPI lock timeout` de Ethernet;
- la espera FRAM máxima observada es 22.232 ms en el primer run y 18.617 ms en la repetición;
- SD permanece muy por debajo de su timeout;
- los redraws del Display no provocan contención problemática.

## Conclusión parcial

**Escenario sin cable Ethernet: PASS.**

No hay evidencia de que haya reaparecido la retención artificial de aproximadamente 560 ms corregida en Alpha4.

Las esperas FRAM de aproximadamente 15-22 ms alrededor de la transición Display/Ethernet son compatibles con ocupación legítima y terminan holgadamente antes del timeout configurado de FRAM (50 ms). SD permanece igualmente dentro de margen.

El `SPI lock timeout` transitorio observado anteriormente no se reproduce en este probe controlado sin RJ45. No se atribuye todavía una causa definitiva a aquel evento aislado.

## Pendiente inmediato

Repetir el mismo probe con RJ45 conectado para cubrir la ruta W5500 + enlace + DHCP y confirmar que tampoco existe contención problemática cuando Ethernet alcanza estado operativo.

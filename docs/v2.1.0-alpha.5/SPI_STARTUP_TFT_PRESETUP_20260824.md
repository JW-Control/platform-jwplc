# Alpha5 — cierre de contención SPI durante arranque del Display

Fecha: 2026-08-24

## Problema

Durante los probes de coexistencia SPI se observaron timeouts de FRAM y
esperas prolongadas de microSD durante el arranque.

La causa reproducible quedó asociada a la inicialización física del TFT
ST7789 ejecutada después de `setup()`.

`jwplcDisplayBeginCallback()` adquiría el mutex SPI global y ejecutaba
`tft.init()` dentro de la sección crítica.

La secuencia de inicialización del ST7789 contiene esperas obligatorias,
por lo que otros usuarios del bus podían alcanzar sus timeouts mientras
el Display conservaba el mutex.

## Cambio adoptado

La inicialización física del Display pasa a ejecutarse desde
`initPeripherals()`, antes de `setup()` y mientras el arranque de
periféricos todavía está serializado.

Además, `jwplcDisplayBeginCallback()` reconoce mediante `g_tftReady`
que la TFT ya fue inicializada y evita repetir `tft.init()` cuando el
runtime invoque posteriormente el callback.

No se modificaron:

- los delays requeridos por el ST7789;
- el mutex SPI;
- los timeouts de FRAM o SD;
- la API pública del Display;
- la lógica Ethernet;
- las frecuencias SPI.

## Core precompilado validado

El archive fue generado desde `cores/jwcontrol` usando el perfil completo
de `jwplcbasic`.

SHA256 anterior:

`3d8ac877281a8b87ef7c244be6e47097508a6c09cd34a834d085d6b8dd278b92`

SHA256 validado/adoptado:

`7c2c0149fc19e5f363a46d49c9a805db268b2ab72ae0a4ebf2b679e781a2d669`

La generación fuente fue corroborada mediante `compile_commands.json`,
donde `peripherals_init.cpp` apareció exactamente una vez.

## Validación física

### 1. Ethernet sin enlace

Log serial:

`20260824_013503_28_tft_pre_setup_normal_basic_serial.log`

Resultados:

- Display listo al entrar a `loop()`: YES
- SD lista al entrar a `loop()`: YES
- STARTUP FRAM: 8/0
- STARTUP SD: 8/0
- STABLE FRAM: 10/0
- STABLE SD: 10/0
- REDRAW FRAM: 10/0
- REDRAW SD: 10/0
- timeout SPI Ethernet: 0
- `SPI_STARTUP_PROBE=PASS`

### 2. Ethernet Link ON con IP estática

Log serial:

`20260824_014334_29_tft_pre_setup_static_link_serial.log`

Resultados:

- Ethernet attempted: SI
- Ethernet ready: SI
- error: OK
- FRAM: 16/0
- FRAM máximo: 88 us
- SD: 16/0
- SD máximo: 35 us
- timeout SPI Ethernet: 0
- `SPI_STATIC_IP_ISOLATION=PASS`

### 3. Ethernet Link ON con DHCP real

Log serial:

`20260824_015202_30_tft_pre_setup_dhcp_link_serial.log`

Resultados:

- Display listo al entrar a `loop()`: YES
- Ethernet pasa a attempted=SI / ready=SI / error=OK
- STARTUP FRAM: 8/0
- STARTUP SD: 8/0
- STABLE FRAM: 10/0
- STABLE máximo FRAM: 47 us
- STABLE SD: 10/0
- STABLE máximo SD: 17 us
- REDRAW FRAM: 10/0
- REDRAW SD: 10/0
- timeout SPI Ethernet: 0
- `SPI_STARTUP_PROBE=PASS`

## Conclusión

El piloto TFT pre-setup queda ADOPTADO para Alpha5.

La inicialización tardía del TFT era una fuente real de contención SPI.
Después del cambio no se reproducen fallos de FRAM ni microSD en las
condiciones ensayadas:

- Ethernet sin Link;
- Ethernet con Link e IP estática;
- Ethernet con Link y DHCP.

DHCP queda descartado como causa necesaria del fallo original.

## Observación no bloqueante

En STARTUP se observan accesos FRAM aislados de aproximadamente 29 ms y
16 ms antes de que Ethernet haya iniciado.

No producen fallos ni alcanzan el timeout de 50 ms. En las ventanas
STABLE y REDRAW los accesos vuelven al orden de decenas de microsegundos.

Se registra como observación y no se amplía el alcance de Alpha5 para
modificar mutex o timeouts sin una reproducción funcional del problema.

## Estado

`ALPHA5_SPI_STARTUP_COEXISTENCE=PASS`

`ALPHA5_TFT_PRESETUP=ADOPTED`
# JWPLC Alpha4 - Gate físico local

Fecha de validación: 2026-08-11

Rama: `v2.1.0-alpha.4/feature/build-speed-cache`

## 1. Objetivo

Validar físicamente el JWPLC Basic después de las optimizaciones P1-P8,
manteniendo el autoload normal del package y sin retirar periféricos para
reducir tiempos de compilación.

Este gate cubre:

- Display / TFT.
- RTC.
- FRAM.
- microSD.
- Botonera.
- 8 entradas digitales.
- 8 salidas digitales por relé.

Ethernet funcional y RS-485 / Modbus RTU se validan mediante gates
separados.

## 2. Sketch de validación

Archivo:

    tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino

El sketch utiliza los periféricos expuestos por el autoload normal del
package JWPLC.

## 3. Botonera del JWPLC Basic

Durante la preparación inicial se detectó que se había usado por error un
mapa de botones correspondiente a otro producto.

El gate final utiliza los seis botones físicos reales:

- UP
- DOWN
- LEFT
- RIGHT
- CANCEL
- OK

Internamente CANCEL corresponde a `BTN_ESC`.

Las constantes utilizadas son:

    BTN_UP
    BTN_DOWN
    BTN_LEFT
    BTN_RIGHT
    BTN_ESC
    BTN_OK

## 4. Primer resultado físico

La primera ejecución produjo:

    ALPHA4_DISPLAY_READY=PASS
    ALPHA4_RTC=PASS
    ALPHA4_FRAM=FAIL
    ALPHA4_SD=FAIL
    ALPHA4_BUTTONS=PASS
    ALPHA4_INPUTS=PASS
    ALPHA4_OUTPUTS=PASS
    ALPHA4_DISPLAY_VISUAL=PASS

    ALPHA4_LOCAL_PHYSICAL_GATE=FAIL

FRAM reportó:

    Size bytes: 8192
    Read #1: FAIL
    Read #2: FAIL
    Lecturas iguales: SI
    ALPHA4_FRAM=FAIL

microSD reportó:

    No se pudo abrir para escritura: SPI lock timeout
    ALPHA4_SD=FAIL

El resultado "Lecturas iguales: SI" de FRAM no representaba una lectura
válida porque ambas operaciones habían fallado.

La coincidencia de FRAM y microSD apuntó a contención del bus SPI
compartido.

## 5. Bus SPI compartido

En JWPLC Basic comparten SPI:

- TFT.
- W5500 Ethernet.
- microSD.
- FRAM.

Pines principales:

    MOSI    GPIO23
    MISO    GPIO19
    SCK     GPIO18

    TFT CS  GPIO33
    SD CS   GPIO32
    FRAM CS GPIO13
    ETH CS  GPIO5

El core protege el bus mediante:

    jwplcSPI_acquire()
    jwplcSPI_release()

La corrección adoptada mantiene este mutex y la exclusión entre
periféricos.

## 6. Diagnóstico

Un primer probe ejecutado antes de la inicialización del Display confirmó
que FRAM y microSD eran físicamente operativos:

    FRAM OK   : 30
    FRAM FAIL : 0
    FRAM max  : 133 us

    SD OK     : 30
    SD FAIL   : 0
    SD max    : 1029 us

    SPI_CONTENTION_PROBE=ALL_PASS

Después se corrigió el probe para realizar las pruebas desde `loop()`,
porque `jwplcSystemTask` comienza su operación normal después de que
`setup()` retorna.

Con el sistema completamente iniciado se reprodujo la falla.

Fase inmediatamente posterior al Display:

    FRAM OK   : 26
    FRAM FAIL : 4
    FRAM max  : 51003 us

    SD OK     : 27
    SD FAIL   : 3
    SD max    : 99999 us

Muestras representativas:

    [A 1] FRAM=FAIL 49304 us | SD=FAIL 99992 us
    [A 2] FRAM=FAIL 50003 us | SD=FAIL 99999 us
    [A 3] FRAM=FAIL 50003 us | SD=FAIL 99999 us
    [A 4] FRAM=FAIL 51003 us | SD=OK 28406 us
    [A 5] FRAM=OK 81 us      | SD=OK 380 us

Durante funcionamiento normal:

    FRAM OK   : 30
    FRAM FAIL : 0
    FRAM max  : 78 us

    SD OK     : 30
    SD FAIL   : 0
    SD max    : 425 us

Con redraws forzados del Display:

    FRAM OK   : 20
    FRAM FAIL : 0
    FRAM max  : 7586 us

    SD OK     : 20
    SD FAIL   : 0
    SD max    : 449 us

Esto descartó un mutex permanentemente bloqueado y concentró el problema
en la secuencia de arranque.

## 7. Identificación de Ethernet como propietario del SPI

El probe de arranque mostró antes de la corrección:

    [1] t=0 ms
    ETH attempted=SI ready=NO error=OK
    FRAM=FAIL 50139 us
    SD=FAIL 99993 us

    [2] t=170 ms
    ETH attempted=SI ready=NO error=OK
    FRAM=FAIL 49734 us
    SD=FAIL 99997 us

    [3] t=340 ms
    ETH attempted=SI ready=NO error=OK
    FRAM=FAIL 49734 us
    SD=FAIL 99997 us

    [4] t=510 ms
    ETH attempted=SI ready=NO error=OK
    FRAM=FAIL 49734 us
    SD=OK 28321 us
    ETH error=Link OFF

    [5] t=609 ms
    ETH attempted=SI ready=NO error=Link OFF
    FRAM=OK 87 us
    SD=OK 381 us

Los timeouts desaparecían exactamente cuando terminaba el intento de
inicialización Ethernet.

## 8. Causa raíz

El backend vendorizado `JWPLC_Ethernet_W5x00_Backend`, basado en Arduino
Ethernet 2.0.2, conservaba dentro de `W5100Class::init()`:

    delay(560);

Esta espera proviene del soporte genérico para shields que pueden utilizar
CAT811/MAX811 durante el reset.

JWPLC Basic ya controla explícitamente el reset físico del W5500:

    RESET LOW  durante 10 ms
    RESET HIGH
    espera de 80 ms
    adquisición del mutex SPI
    inicialización Ethernet

Por ello, la espera adicional de 560 ms era redundante en JWPLC Basic.

Además, se ejecutaba mientras Ethernet ya mantenía adquirido el mutex SPI,
bloqueando FRAM y microSD sin que durante esa espera se realizara una
transferencia SPI útil.

## 9. Corrección adoptada

Se eliminó únicamente el `delay(560)` del backend JWPLC y se añadió una
nota en el fuente explicando la diferencia respecto al upstream.

No se modificaron:

- APIs públicas.
- Mutex SPI.
- Exclusión entre periféricos.
- Timeout de FRAM.
- Timeout de microSD.
- Autoload de Ethernet.
- Autoload de Display.
- Autoload de FRAM.
- Autoload de microSD.
- Arquitectura general del core.

No se retiró ningún periférico del autoload.

Desde esta corrección, el backend debe considerarse:

    Arduino Ethernet 2.0.2 + patch específico JWPLC

y no una copia byte-identical del upstream.

## 10. Archive Ethernet regenerado

Archive:

    JWPLC/2.1.0/libraries/JWPLC_Ethernet_W5x00_Backend/src/esp32/libJWPLC_Ethernet_W5x00_Backend.a

Antes:

    Bytes:
    1116812

    SHA-256:
    C1637EBBE782C24AA8449CCFDD4D04EA3E4B416ED3DDB4C8A885890C24C273E3

Después:

    Bytes:
    1116456

    SHA-256:
    F3D4A7922F6ECB916F91B51D13E0AF02DA194529CB1A36D52332C938EACC2097

La regeneración se realizó con
`Build-JWPLCPrecompiledLibraries.ps1`.

El build fuente y la verificación posterior con `precompiled=full`
terminaron correctamente.

## 11. Verificación dirigida posterior al fix

El mismo probe utilizado para reproducir la falla produjo después del fix:

    [1] t=0 ms
    ETH attempted=SI ready=NO error=Link OFF
    FRAM=OK 27855 us
    SD=OK 629 us

    [2] t=49 ms
    FRAM=OK 78 us
    SD=OK 389 us

    [3] t=82 ms
    FRAM=OK 3701 us
    SD=OK 363 us

    [4] t=118 ms
    FRAM=OK 83 us
    SD=OK 420 us

    [5] t=150 ms
    FRAM=OK 87 us
    SD=OK 398 us

La primera lectura de FRAM esperó aproximadamente 28 ms porque el bus
estaba realmente ocupado.

Ese comportamiento es correcto:

- el mutex sigue activo;
- no hay transferencias simultáneas;
- FRAM espera por el bus;
- FRAM obtiene el bus dentro de su timeout;
- microSD también accede correctamente.

Se eliminó únicamente la retención artificial de aproximadamente 560 ms.

## 12. Gate físico local final

Después de regenerar el archive Ethernet y recompilar el gate integral:

    ALPHA4_DISPLAY_READY=PASS
    ALPHA4_RTC=PASS
    ALPHA4_FRAM=PASS
    ALPHA4_SD=PASS
    ALPHA4_BUTTONS=PASS
    ALPHA4_INPUTS=PASS
    ALPHA4_OUTPUTS=PASS
    ALPHA4_DISPLAY_VISUAL=PASS

    ALPHA4_LOCAL_PHYSICAL_GATE=PASS

FRAM:

    Size bytes: 8192
    Read #1: OK
    Read #2: OK
    Lecturas iguales: SI

microSD:

    Readback: JWPLC_ALPHA4_SD_OK
    Remove: OK

Los seis botones físicos fueron validados:

    UP
    DOWN
    LEFT
    RIGHT
    CANCEL
    OK

Las ocho entradas `I0_0` a `I0_7` cambiaron y regresaron correctamente a
su estado inicial.

Las ocho salidas `Q0_0` a `Q0_7` fueron verificadas físicamente una por
una.

La TFT fue confirmada visualmente.

Durante la confirmación visual el gate solicitó deliberadamente:

    PWR -> verde
    RUN -> verde
    BUS -> verde
    ERR -> rojo
    ETH -> verde

El LED ERR rojo en esta prueba fue forzado por el propio gate y no
representa una falla automática.

## 13. Conclusión

Resultado:

    ALPHA4_LOCAL_PHYSICAL_GATE=PASS

Las optimizaciones P1-P8 mantienen operativos los periféricos cubiertos
por este gate.

La falla inicial no fue causada directamente por la precompilación.

La causa raíz fue una espera genérica de 560 ms heredada del backend
Ethernet que mantenía innecesariamente ocupado el mutex SPI durante el
arranque del W5500.

La corrección:

- mantiene el mutex;
- mantiene la exclusión SPI;
- mantiene las APIs existentes;
- mantiene el autoload;
- no aumenta artificialmente los timeouts;
- no elimina periféricos;
- elimina únicamente una espera redundante para JWPLC Basic.

El gate físico local de Alpha4 queda cerrado.

## 14. Pendientes físicos

Este resultado no cierra todavía el gate físico global de Alpha4.

Quedan pendientes:

- Ethernet funcional sobre W5500.
- RS-485.
- Modbus RTU.

El gate físico global sólo podrá cerrarse después de validar también esos
bloques.

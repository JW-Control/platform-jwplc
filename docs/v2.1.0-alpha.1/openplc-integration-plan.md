# Plan de integración OpenPLC para JWPLC Basic

Fecha: 2026-05-23  
Etapa: `alpha32-openplc-integration`  
Versión interna sugerida: `v2.1.0-alpha.1`  
Base estable: `JWPLC Basic v2.0.0`

## Objetivo

Integrar OpenPLC Editor v4 con el JWPLC Basic sin modificar el package Arduino estable `platform-jwplc v2.0.0` y sin romper el uso normal del JWPLC Basic desde Arduino IDE.

## Alcance validado

La integración se realizó del lado de OpenPLC Editor / Baremetal Arduino, mediante archivos externos al package JWPLC:

- `editor/arduino/examples/Baremetal/hals.json`
- `editor/arduino/src/hal/jwplcbasic.cpp`
- `editor/arduino/examples/Baremetal/ModbusSlave.cpp`
- `editor/arduino/examples/Baremetal/ModbusSlave.h`
- imagen preview de placa usada por `hals.json`

El package `platform-jwplc` no fue modificado.

## Decisión principal

OpenPLC queda integrado como target opcional del OpenPLC Editor, no como parte obligatoria del runtime normal Arduino del JWPLC Basic.

Esto mantiene limpia la versión estable `v2.0.0` y evita introducir dependencias o comportamiento OpenPLC en sketches Arduino normales.

## Decisiones técnicas

### 1. `hardwareInit()` en `jwplcbasic.cpp`

Se deja vacío o solo documentado.

Motivo:

- El core `jwcontrol` ya ejecuta `initPeripherals()` antes de `setup()`.
- Las E/S industriales ya son inicializadas por el package.
- No se debe repetir `pinMode()`.
- No se debe reinicializar el TCA6424A.
- No se debe tocar `EN_IO` desde OpenPLC.

Decisión:

```cpp
void hardwareInit()
{
    // La inicialización de E/S del JWPLC Basic v2.x ya la realiza el core jwcontrol.
}
```

### 2. Pines industriales como `uint16_t`

Los pines industriales del JWPLC Basic usan valores virtuales de 16 bits, por ejemplo `I0_0`, `Q0_0`, etc.

Decisión:

```cpp
uint16_t pinMask_DIN[]  = { PINMASK_DIN };
uint16_t pinMask_AIN[]  = { PINMASK_AIN };
uint16_t pinMask_DOUT[] = { PINMASK_DOUT };
uint16_t pinMask_AOUT[] = { PINMASK_AOUT };
```

No usar `uint8_t`, porque truncaría los valores virtuales.

### 3. Lectura y escritura de E/S

OpenPLC interactúa con las E/S usando las funciones Arduino ya soportadas por el package:

```cpp
digitalRead(pin);
digitalWrite(pin, value);
```

### 4. Modbus TCP

OpenPLC Baremetal trae implementación propia de Modbus TCP en `ModbusSlave.cpp/.h`.

Para JWPLC Basic se requiere una adaptación porque el Ethernet físico es W5500 por SPI.

Ruta incorrecta a evitar:

```cpp
ETH.begin();
```

Ruta correcta para JWPLC Basic:

```cpp
#if defined(JWPLC_BASIC)
#include <JWPLC_Ethernet.h>
#endif
```

y dentro de la inicialización Ethernet:

```cpp
JWPLC_Ethernet.useDHCP();
JWPLC_Ethernet.begin(mac);
mb_server.begin();
```

## Estado de RTU/TCP

- RTU solo: funcional.
- TCP solo: funcional.
- TCP + RTU simultáneos: TCP funciona; RTU no trabaja correctamente en la prueba actual.

Esto queda como pendiente o limitación conocida.

## Qué no se modificó

No se modificó:

- `platform-jwplc`
- `boards.txt`
- `platform.txt`
- `Arduino.h`
- `peripherals_init.cpp`
- `jwplc_peripherals.cpp`
- particiones
- FlashFreq
- bootloader
- autoload de periféricos

## Estado

Integración funcional OpenPLC/JWPLC Basic validada.

Pendiente para cierre documental final:

- Registrar archivos modificados finales.
- Guardar proyecto de ejemplo OpenPLC.
- Documentar limitación RTU + TCP simultáneo.
- Dejar `JWPLC_MBTCP_DEBUG` desactivado.
- Definir estrategia final de MAC por defecto.

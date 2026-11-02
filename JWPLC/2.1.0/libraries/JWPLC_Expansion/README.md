# JWPLC_Expansion

Convierte un JWPLC Basic en **módulo de expansión DI8/DO8** de otro JWPLC que corre
[`JWPLC_Runtime`](../JWPLC_Runtime/README.md), por RS-485 (Modbus RTU). Versión **0.1.0** (package 2.1.0-alpha.16).

```cpp
#include <JWPLC_Expansion.h>

void setup()
{
    JWPLC_ExpansionSlave.begin(); // configuración de su FRAM, o de fábrica: ID 247, 115200 8N1
}

void loop()
{
}
```

## Cómo se usa

1. Subir `Expansion_Esclavo_Basic` al equipo esclavo y conectarlo solo al bus RS-485 del maestro.
2. En el maestro: *Configuración › Expansión › Agregar módulo*. El maestro le asigna ID, estado seguro de cada
   salida y vigilancia; el esclavo lo guarda en su FRAM.
3. En el programa del maestro sus E/S aparecen como `1.I1`…`1.I8` y `1.Q1`…`1.Q8`.

Pantalla del esclavo: el título dice «Esclavo ID n» (o «Esclavo fabrica»), RUN indica que el maestro está presente
y ERR «FS» que se aplicó el estado seguro porque el maestro dejó de escribir. Volver a fábrica: mantener ESC + OK
3 s al encender.

## Configuración fija desde el sketch

El maestro la lee y no la puede cambiar:

```cpp
JWPLC_ExpansionSlave.setSafeState(1, SAFE_ON);   // Q1 encendida si falta el maestro
JWPLC_ExpansionSlave.setWatchdog(2000);           // 2 s sin maestro (1000..5000 ms)
JWPLC_ExpansionSlave.begin(3, 115200, SERIAL_8N1);
```

Estado: `id()`, `locked()`, `linked()`, `safeActive()` y `outputs()` (bit 0 = Q1).

Se distribuye en código fuente (sin `.a`). Protocolo y pruebas: `docs/v2.1.0-alpha.16/A16_RT2_EXPANSION_DISENO_20261101.md`.

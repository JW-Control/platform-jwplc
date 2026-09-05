# Alpha10 - Validación Arduino IDE y gate físico

Fecha: 2026-09-05.

## Objetivo

Cerrar la validación local/física de `v2.1.0-alpha.10` después de retirar el guard de shadowing de `JWPLC_Ethernet`, usando el flujo real de Arduino IDE y el autoload normal del package.

El cambio de Alpha10 está limitado a library discovery/build speed:

```text
PUBLIC_API_CHANGED=NO
RUNTIME_IMPLEMENTATION_CHANGED=NO
PRECOMPILED_ARCHIVES_CHANGED=NO
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

## Sketch utilizado

```text
tools/build-speed-benchmark/sketches/06_alpha4_local_physical_gate/06_alpha4_local_physical_gate.ino
```

El nombre histórico `ALPHA4` se conserva porque el mismo gate físico ya validado se reutiliza sin modificarlo sólo para cambiar etiquetas.

Características relevantes del gate:

- no incluye headers JWPLC manualmente;
- usa el autoload normal del package;
- prueba Display;
- prueba RTC;
- prueba FRAM mediante lectura no destructiva;
- prueba microSD mediante write/read/verify/remove;
- prueba los seis botones;
- prueba las 8 entradas digitales;
- prueba las 8 salidas/relés;
- requiere confirmación visual del TFT.

La compilación y el upload se realizaron desde Arduino IDE sobre JWPLC Basic. El Monitor Serie se observó a 115200 baud.

## Resultado físico

Salida final recibida:

```text
========================================
 RESULTADO GATE FISICO LOCAL ALPHA4
========================================
ALPHA4_DISPLAY_READY=PASS
ALPHA4_RTC=PASS
ALPHA4_FRAM=PASS
ALPHA4_SD=PASS
ALPHA4_BUTTONS=PASS
ALPHA4_INPUTS=PASS
ALPHA4_OUTPUTS=PASS
ALPHA4_DISPLAY_VISUAL=PASS

ALPHA4_LOCAL_PHYSICAL_GATE=PASS

Ethernet y RS-485/Modbus quedan en gates separados.
```

Conclusión:

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_AUTOLOAD_PHYSICAL_GATE=PASS
DISPLAY=PASS
RTC=PASS
FRAM=PASS
MICROSD=PASS
BUTTONS=PASS
DIGITAL_INPUTS=PASS
DIGITAL_OUTPUTS=PASS
TFT_VISUAL=PASS
UNEXPECTED_RESET_OR_FREEZE_OBSERVED=NO
```

## Ethernet y RS-485 / Modbus RTU

El gate físico reutilizado declara explícitamente que Ethernet y RS-485/Modbus se validan por separado.

Alpha10 no cambia runtime Ethernet, SPI, RS-485 ni Modbus RTU. Por ello no se repite el stress físico de 10 minutos ni el banco Master/Slave ya cerrados en alphas anteriores.

La regresión de compilación del candidato sí cubrió ambos subsistemas:

```text
Ethernet_Diagnostics=PASS
RemoteIO_Slave_RTU=PASS
UNDEFINED_REFERENCE_HITS=0
```

Evidencia física heredada conservada:

- Ethernet W5500, DHCP/IP, coexistencia SPI/TFT/FRAM/microSD y stress: Alpha6/Alpha7;
- RS-485/Modbus RTU, Remote I/O y recuperación: Alpha7/Alpha9.

Por alcance:

```text
ETHERNET_RUNTIME_CHANGED=NO
MODBUS_RTU_RUNTIME_CHANGED=NO
ETHERNET_RUNTIME_RETEST_ALPHA10=NOT_REQUIRED_SCOPE_UNCHANGED
MODBUS_RTU_RUNTIME_RETEST_ALPHA10=NOT_REQUIRED_SCOPE_UNCHANGED
```

## Estado

```text
ALPHA10_ARDUINO_IDE_VALIDATION=PASS
ALPHA10_LOCAL_PHYSICAL_GATE=PASS
ALPHA10_PHYSICAL_VALIDATION=PASS_WITH_SCOPED_INHERITED_ETH_RTU_EVIDENCE
```

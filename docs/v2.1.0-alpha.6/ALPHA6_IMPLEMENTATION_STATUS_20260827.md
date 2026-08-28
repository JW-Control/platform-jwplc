# Alpha6 — estado de implementación posterior al gate físico

Fecha: 2026-08-27

Branch:

```text
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

## Propósito

Este documento registra el estado del código después del gate físico del 27 de agosto.
No reemplaza `PHYSICAL_VALIDATION_20260827.md`: aquella evidencia corresponde al firmware
que fue compilado y probado físicamente antes de los cambios descritos aquí.

Por tanto, en este documento se distingue de forma estricta entre:

- **IMPLEMENTADO**: cambio presente en la rama y revisado estáticamente;
- **VALIDADO**: cambio comprobado mediante compilación/subida/prueba correspondiente;
- **PENDIENTE DE VALIDACIÓN**: implementación lista, pero todavía sin gate posterior.

---

## Checkpoint de entrada

Antes de esta sesión, la rama remota quedó en:

```text
912c5bb test(alpha6): añadir acceptance Ethernet no bloqueante
```

El gate físico previo ya había demostrado:

- LINK ON sin DHCP sin congelar TCA/RTC/SPI;
- IP estática + HTTP real;
- 10 minutos de stress SPI con W5500/TFT/FRAM/microSD;
- 591 solicitudes HTTP reales durante el stress;
- cero fallos de mutex, W5500, LINK, FRAM y microSD;
- `PCB_FINAL_ACCEPTANCE=PASS`.

Ver detalle en:

```text
docs/v2.1.0-alpha.6/PHYSICAL_VALIDATION_20260827.md
```

---

## 1. Mantenimiento DHCP cooperativo

Commit de implementación:

```text
830dd4d feat(ethernet): hacer cooperativo renew y rebind DHCP
```

Estado:

```text
IMPLEMENTADO
PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

### Arquitectura

El autoload continúa llamando:

```text
jwplcEthernetTickCallback()
    -> JWPLC_Ethernet.service()
```

Cuando el runtime está `READY` y usa DHCP, `service()` ya no necesita invocar el
`Ethernet.maintain()` síncrono upstream. En su lugar llama un paso corto de:

```text
Ethernet.maintainAsync()
```

El backend mantiene separadas:

- adquisición DHCP inicial;
- renovación T1;
- rebind T2.

La renovación/rebind usa la misma máquina cooperativa de envío/consulta DHCP, avanzando
como máximo un paso corto por llamada.

### Compatibilidad

Se conserva:

```cpp
JWPLC_Ethernet.maintain();
Ethernet.maintain();
```

como rutas síncronas legacy para código que las invoque explícitamente.

**Decisión:** el runtime/autoload no utiliza esas rutas síncronas.

### Fallo de mantenimiento

Un fallo de renew/rebind no invalida inmediatamente el lease ya programado en W5500.
El runtime:

- conserva `READY` mientras el LINK siga presente;
- registra `JWPLC_ETH_DHCP_FAILED`;
- muestra diagnóstico `DHC`;
- programa reintento cooperativo;
- no reinicia automáticamente el resto del PLC.

### Pendiente

Falta validar con DHCP real:

- lease inicial;
- renew T1;
- rebind T2;
- tiempo máximo de servicio;
- ausencia de pausas de TCA/RTC/SPI durante mantenimiento.

---

## 2. Diagnóstico visual ETH

Commit de implementación:

```text
aaae713 feat(display): mostrar diagnósticos BUS y ETH independientes
```

Estado:

```text
IMPLEMENTADO
PENDIENTE DE VALIDACIÓN VISUAL EN TFT
```

El rectángulo existente del indicador `ETH` conserva su posición y tamaño. No se movió
el layout de I/O ni RTC. Dentro del mismo indicador se dibuja un código de tres caracteres.

### Códigos ETH

| Código | Significado |
|---|---|
| `---` | Ethernet operativo |
| `INI` | no iniciado / inicializando |
| `PHY` | W5500 y PHY disponibles, red aún no lista |
| `LNK` | sin LINK físico |
| `DHC` | DHCP inicial, renew/rebind o fallo DHCP |
| `HW` | W5500 no detectado |
| `IP` | configuración IP inválida |
| `SPI` | problema de SPI/mutex |
| `DIS` | Ethernet no disponible/deshabilitado |

### Color ETH

- `---`: verde;
- `DIS`: gris;
- `INI`, `PHY`, `LNK`: negro/inactivo con código visible;
- `DHC` durante negociación: inactivo;
- `DHC` durante renew/rebind con lease vigente: verde;
- `DHC` tras fallo DHCP: rojo;
- `HW`, `IP`, `SPI`: rojo.

---

## 3. Diagnóstico visual BUS

Estado:

```text
IMPLEMENTADO
PENDIENTE DE VALIDACIÓN VISUAL Y MODBUS RTU
```

El indicador BUS combina el estado físico/runtime de RS-485 y, únicamente cuando
corresponde, los errores de Modbus RTU.

### Códigos BUS

| Código | Significado |
|---|---|
| `---` | RS-485 disponible, sin error |
| `DIS` | RS-485 deshabilitado/no disponible |
| `INI` | RS-485 todavía no iniciado |
| `SER` | Serial/RS-485 inválido o error interno |
| `SID` | Slave ID Modbus inválido |
| `MAP` | mapa de registros inválido |
| `TMO` | timeout Modbus |
| `CRC` | CRC Modbus incorrecto |
| `EXC` | excepción Modbus |
| `RSP` | respuesta Modbus inválida |
| `OVF` | overflow de buffer |
| `FUN` | función Modbus no soportada |

### Regla RS-485 crudo

`JWPLC_MODBUS_NOT_STARTED` y `JWPLC_MODBUS_DISABLED` no se interpretan como fallo de BUS
si RS-485 está listo. Esto permite usar `JWPLC_RS485` directamente sin obligar al usuario
a iniciar Modbus RTU.

### Color BUS

- `DIS`: gris;
- `INI`: negro/inactivo;
- código de error: rojo;
- `---` sin actividad reciente: negro/inactivo;
- `---` con actividad RS-485 reciente: verde.

---

## 4. ERR continúa separado

Regla confirmada en implementación:

```text
RUN = estado de ejecución
ERR = aplicación / usuario
BUS = diagnóstico de comunicaciones seriales
ETH = diagnóstico Ethernet
```

Los cálculos automáticos de BUS y ETH no escriben `g_errLed`.

Un `DHC`, `SPI`, `CRC`, `TMO`, etc. no debe encender por sí solo `ERR`.

El usuario conserva las APIs existentes:

```cpp
JWPLC_Display.setErrLed(true);
JWPLC_Display.setErrLed(false);
```

---

## 5. Tooling de selección Ethernet

Commit:

```text
11e688f chore(ethernet): adaptar tooling a librería unificada
```

Estado:

```text
IMPLEMENTADO
PENDIENTE DE EJECUCIÓN LOCAL
```

Se incorpora:

```text
tools/build-speed-benchmark/Verify-JWPLCUnifiedEthernetSelection.ps1
```

El verificador comprueba que:

- exista una sola librería Arduino `JWPLC_Ethernet` para W5500;
- no reaparezca `JWPLC_Ethernet_W5x00_Backend`;
- `JWPLC_Ethernet.h` use `JWPLC_W5x00_Ethernet.h` interno;
- no reaparezca el marker legacy;
- no reaparezca el `.a` del antiguo backend separado;
- Arduino Builder seleccione `JWPLC_Ethernet` del package;
- no seleccione `Ethernet` del sketchbook ni una homónima externa.

El nombre legacy:

```text
Verify-JWPLCBundledEthernetSelection.ps1
```

se conserva como wrapper para no romper llamadas antiguas.

El script:

```text
Vendor-JWPLCEthernetW5x00Backend.ps1
```

queda bloqueado deliberadamente: ejecutarlo ya no puede recrear una segunda librería
Arduino. La trazabilidad upstream permanece bajo:

```text
JWPLC_Ethernet/third_party/arduino-ethernet-2.0.2/
```

Los benchmarks y documentos históricos de Alpha4/P5 no se reescribieron masivamente.

---

## 6. Estado de validación

| Punto | Implementación | Validación posterior |
|---|---:|---:|
| Ethernet unificado | Sí | PASS previo |
| DHCP inicial cooperativo | Sí | PASS previo N |
| IP estática + HTTP | Sí | PASS previo L |
| Stress SPI 10 min | Sí | PASS previo |
| Renew DHCP cooperativo | Sí | Pendiente |
| Rebind DHCP cooperativo | Sí | Pendiente |
| Router DHCP real | Soportado | Pendiente |
| Desconexión/reconexión RJ45 | Soportado por retry | Pendiente |
| Código visual ETH | Sí | Pendiente |
| Código visual BUS | Sí | Pendiente |
| ERR independiente | Sí | Pendiente de regresión visual |
| Verificador librería unificada | Sí | Pendiente de ejecución |
| Arduino CLI tras cambios nuevos | — | Pendiente |
| Arduino IDE tras cambios nuevos | — | Pendiente |
| `.a` unificado / precompilación | No | Deliberadamente pendiente |

---

## 7. Lo que NO se declara cerrado

No se considera todavía validado ni cerrado:

- renew/rebind físico;
- DHCP real de router sobre el nuevo commit;
- recuperación física de LINK;
- representación visual de todos los códigos;
- regresión Arduino IDE/CLI posterior a estos commits;
- precompilación de `JWPLC_Ethernet`;
- Alpha6 completa;
- PR/release final.

El siguiente paso es ejecutar `ALPHA6_VALIDATION_PLAN_20260827.md` sobre hardware.

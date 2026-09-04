# Alpha8 - Ejemplos numerados para taller JWPLC Basic

Fecha: 2026-09-04

## Objetivo

Dejar una serie corta, ordenada y comentada de ejemplos para enseñar y probar los periféricos propios del **JWPLC Basic** desde Arduino IDE.

Los ejemplos específicos del hardware JWPLC se ubican en librerías `JWPLC_*`. Las librerías genéricas `JW_*` continúan siendo reutilizables fuera del JWPLC Basic y no reciben ejemplos dependientes del hardware de la placa.

Los directorios de taller usan prefijo de dos dígitos:

```text
01.
02.
03.
...
```

Esto los mantiene al inicio y en orden dentro de `Archivo > Ejemplos` de Arduino IDE. Los ejemplos históricos de diagnóstico, stress, juegos y validación se conservan sin reemplazarlos.

---

## JWPLC_GlobalPeripherals

Aquí se agrupan los periféricos globales cuya instancia/configuración pertenece al JWPLC Basic.

| Ejemplo | Objetivo |
|---|---|
| `01.DigitalIO_Basic` | Lectura/escritura de las 8 E/S y vista `JWPLC_IO`. |
| `02.Buttons_Basic` | `pressed()`, `released()`, `isDown()` y limpieza de pendientes. |
| `03.RTC_Basic` | `JWPLC_Time` cacheado y temperatura mediante `JWPLC_RTC`. |
| `04.FRAM_Basic` | Persistencia con `readBlock()/writeBlock()` y texto C. |
| `05.microSD_Basic` | Estado, escritura y lectura usando `JWPLC_SD`/`JWPLCFile`. |
| `06.Runtime_Cache_Status` | Snapshots `JWPLC_IO` y `JWPLC_Time` sin transacciones extra. |

Decisión de arquitectura:

```text
JW_RTC / JW_FRAM / JW_SD / JW_MatrixButtons
= drivers genéricos reutilizables

JWPLC_GlobalPeripherals
= instancias, autoload y ejemplos específicos del JWPLC Basic
```

---

## JWPLC_Display

| Ejemplo | Objetivo |
|---|---|
| `01.Display_IDLE_Status` | RUN, ERR, BUS/ETH auto y redraw del IDLE. |
| `02.Display_HMI_Fields` | VALUE, TEXT, BOOL y BAR con dirty redraw. |
| `03.Display_HMI_Pages` | Páginas USER y datos cacheados de I/O/RTC. |
| `04.Display_TFT_Direct` | Dibujo Adafruit directo dentro de callbacks protegidos. |

Los ejemplos Alpha8 usan la API pública estilo objeto `JWPLC_Display.*` y mantienen `IDLE_WAKE_DISABLED` como comportamiento seguro por defecto.

---

## JWPLC_Ethernet

| Ejemplo | Objetivo |
|---|---|
| `01.Ethernet_DHCP_Basic` | Autoload DHCP y consulta de estado/IP. |
| `02.Ethernet_StaticIP_Basic` | Configuración IPv4 estática antes del autoload. |
| `03.Ethernet_Diagnostics` | Estado, código diagnóstico y errores del W5500. |

Los ejemplos normales no llaman `begin()`/`maintain()` porque el autoload actual usa `JWPLC_Ethernet.service()` de forma cooperativa.

---

## JWPLC_RS485

| Ejemplo | Objetivo |
|---|---|
| `01.RS485_Send` | Inicio 115200 8N1 y envío periódico. |
| `02.RS485_Echo` | Lectura y eco de bytes. |
| `03.RS485_Status` | Telemetría TX/RX y estado del transporte. |

El JWPLC Basic actual usa transceptor con autodirección; el sketch no controla DE/RE manualmente.

---

## JWPLC_ModbusRTU

La serie compacta usa la misma configuración para facilitar el taller:

```text
Slave ID = 2
RTU      = 115200 8N1
Master local ID = 247
```

| Ejemplo | Objetivo |
|---|---|
| `01.ModbusRTU_Slave_Holding` | Slave con HR0..HR7 y estadísticas. |
| `02.ModbusRTU_Master_Read` | Master cooperativo FC03. |
| `03.ModbusRTU_Master_Write` | Master cooperativo FC06 sobre HR1. |

`02` y `03` están pensados para probarse contra `01` cargado en un segundo JWPLC Basic.

Los ejemplos Remote I/O y gates de validación existentes se mantienen como material avanzado.

---

## Principios usados en los ejemplos

- comentarios dentro del sketch explicando la función probada;
- ejemplos compactos y modificables durante el taller;
- API pública estilo objeto cuando existe;
- no crear una segunda instancia de periféricos ya gestionados por el package;
- no iniciar un segundo task de botonera;
- no retirar periféricos del autoload normal;
- `JWPLC_IO`/`JWPLC_Time` para lecturas repetitivas cacheadas;
- Master Modbus cooperativo como ruta recomendada;
- dibujo TFT directo sólo en contexto protegido por el Display.

## Gate de cierre

Antes de declarar Alpha8 cerrado, todos los ejemplos numerados deben compilar con:

```text
jwplc_local:esp32:jwplcbasic
```

y los ejemplos compatibles deben conservar el comportamiento físico ya validado en Alpha8.

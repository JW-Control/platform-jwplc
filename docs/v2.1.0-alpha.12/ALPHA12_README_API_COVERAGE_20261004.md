# Alpha12 — Auditoría de README y cobertura de API pública

Fecha: 2026-10-04

## Objetivo

Los README de librería deben servir como:

1. guía sencilla para un usuario;
2. referencia suficiente para implementar un sketch sin leer el header;
3. fuente confiable para una IA que necesite usar la API;
4. separación explícita entre API recomendada, compatibilidad, avanzada e
   implementación interna.

## READMEs auditados

```text
README.md
JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md
JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md
JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md
JWPLC/2.1.0/libraries/JWPLC_TFT/README.md
JWPLC/2.1.0/libraries/JWPLC_Display/README.md
```

## Cobertura contra headers

Se cruzaron los nombres de métodos públicos del header contra el README
correspondiente.

| Área | Métodos/nombres auditados | Faltantes |
|---|---:|---:|
| JWPLC_ModbusRTU | 81 | 0 |
| JWPLC_ModbusTCP Server | 32 | 0 |
| JWPLC_ModbusTCP Client | 29 | 0 |
| JWPLC_TFT | 34 | 0 |
| JWPLC_Ethernet | 34 | 0 |
| EthernetClient | 33 | 0 |
| EthernetServer | 4 | 0 |
| EthernetUDP | 20 | 0 |
| Ethernet compatibility object | 28 | 0 |
| JWPLC_Display | 58 | 0 |

Resultado:

```text
README_PUBLIC_API_NAME_COVERAGE=PASS
README_PUBLIC_API_MISSING_COUNT=0
```

## Criterio de ejemplos

Cada función del contrato de usuario tiene al menos un ejemplo mínimo de
invocación en su README.

Las funciones se agrupan por propósito para evitar un README plano e ilegible:

```text
INICIALIZACION
CONFIGURACION
ESTADO
LECTURAS
ESCRITURAS
MAPAS
DIAGNOSTICO
COMPATIBILIDAD
AVANZADA
```

## Modbus RTU

El README documenta:

- todas las variantes `begin()`;
- ASYNC / SYNC;
- API unificada `read...()/write...()`;
- variantes `request...()`;
- variantes `...Sync()`;
- estado Master;
- mapas Slave completos;
- fail-safe/activity;
- frame gap;
- diagnóstico;
- helpers CRC;
- knobs avanzados públicos claramente marcados.

La recomendación para código nuevo es:

```text
motor(ASYNC)
+ API unificada read...()/write...()
+ task()
```

## Modbus TCP

El README documenta por separado:

- Server;
- Client;
- todos los FC soportados;
- mapas Server;
- todas las requests Client;
- state machine;
- errores/exceptions;
- diagnóstico.

Los hooks `jwplcSchedulerProfile*()` están documentados como
**condicionales de qualification** y no como API normal.

## Ethernet / TCP / UDP

El README documenta:

- `JWPLC_Ethernet`;
- `EthernetClient`;
- `EthernetServer`;
- `EthernetUDP`;
- objeto `Ethernet` de compatibilidad;
- async connect/write/flush/stop;
- UDP async endPacket;
- fast RX TCP/UDP.

Las rutas async/fast están marcadas como **AVANZADAS**.

La API normal recomendada mantiene la semántica Arduino:

```text
EthernetClient
EthernetServer
EthernetUDP
```

`DhcpClass` aparece en el header por implementación, pero se declara
explícitamente:

```text
DhcpClass=INTERNAL_NOT_USER_CONTRACT
```

Una IA no debe recomendar instanciarlo directamente.

Los hooks de test/profile se documentan como condicionales y no disponibles en
el build normal.

## JWPLC_TFT

El README documenta todas las primitivas públicas:

- inicialización/estado;
- panel/dimensiones;
- batching;
- screen/rectangles;
- circles;
- lines/pixel;
- cursor;
- text state;
- text measurement;
- `write()`;
- colores;
- tipos de panel;
- acceso desde `JWPLC_Display`.

También fija:

```text
TFT_ESPI_USER_DEPENDENCY=NO
Adafruit_ST7789&_PUBLIC_BACKEND=NO
JWPLC_TFTClass&_PUBLIC_BACKEND=YES
```

## JWPLC_Display

El README fue sincronizado con P6/P7 y ampliado para cubrir toda la API pública:

- estado;
- IDLE;
- refresh;
- páginas;
- fields;
- PixelMaps;
- values;
- invalidación;
- indicadores;
- acceso TFT.

La compatibilidad gráfica queda decidida/documentada:

```text
OLD_EXPLICIT_TYPE=Adafruit_ST7789&
NEW_EXPLICIT_TYPE=JWPLC_TFTClass&
RECOMMENDED_PATTERN=auto &tft = JWPLC_Display.tft();
```

## README raíz

El README raíz fue actualizado con:

- estado Alpha12;
- mejoras RTU/TCP/UDP/TFT;
- quick starts;
- freeze de precompilados;
- benchmark final;
- decisiones app-only/bootloader/config;
- links a referencias completas de API.

El marcador:

```text
<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.11 -->
```

se conserva **intencionalmente** hasta publicación porque el workflow
`auto-release-jwplc-on-readme.yml` lo usa como trigger/version source.

Por tanto:

```text
ROOT_README_CONTENT_ALPHA12=UPDATED
ROOT_README_RELEASE_MARKER=KEEP_ALPHA11_UNTIL_PUBLICATION
```

## Texto obsoleto eliminado

Se verificó que los README actuales no conserven como estado vigente:

```text
SOURCE_FIRST=YES
PRECOMPILED_FINAL_ALPHA12=PENDING_REGEN
DISPLAY_ALPHA12_ARCHIVE_REGEN=PENDING
RAW_BACKEND_EXPLICIT_ADAFRUIT_TYPE_COMPATIBILITY=REVIEW
```

Tampoco quedan como estado actual los hashes Alpha11 reemplazados por los
artifacts Alpha12 congelados.

## Resultado

```text
ALPHA12_README_API_AUDIT=PASS
README_PUBLIC_API_NAME_COVERAGE=PASS
README_PUBLIC_API_MISSING_COUNT=0
README_EXAMPLES_POLICY=ALL_SUPPORTED_PUBLIC_FUNCTIONS
README_LOW_LEVEL_CLASSIFICATION=PASS
ROOT_README_ALPHA12_CONTENT=PASS
RELEASE_MARKER_CHANGE=DEFERRED_UNTIL_PUBLICATION
```

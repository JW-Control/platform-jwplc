# Pull Request - JWPLC Basic v2.0.0

## Título sugerido

```txt
Release v2.0.0: package estable inicial para JWPLC Basic
```

## Ramas

```txt
Base: main
Compare: develop/beta1-package-validation
```

---

## Resumen

Este PR prepara la publicación estable inicial de **JWPLC Basic v2.0.0** como package Arduino instalable desde Boards Manager.

La versión `v2.0.0` toma como base el trabajo validado en:

```txt
v2.0.0-alpha.31
v2.0.0-beta.1
```

`alpha31` fue la validación técnica integral del package instalado desde cero.  
`beta1` fue publicada como etapa de package validation, con ajustes documentales, validación Arduino CLI y preparación del canal público/dev.

---

## Estado

```txt
Versión objetivo: 2.0.0
Tipo: release estable inicial
Package: JW Control ESP32 Boards
FQBN principal: jwplc:esp32:jwplcbasic
```

---

## Decisión de flujo para este ciclo

Durante este ciclo sí se publicó:

```txt
v2.0.0-beta.1
```

Sin embargo, se deja documentado que `alpha31` ya cubrió gran parte del rol práctico de una beta, porque fue validada con:

- instalación limpia desde Boards Manager;
- borrado previo de instalación local;
- instalación y manejo desde otra PC;
- Arduino IDE;
- Arduino CLI;
- subida real por USB;
- monitor serial;
- placas principales;
- librerías bundled;
- periféricos principales;
- Modbus RTU master/slave;
- coexistencia SPI Ethernet + SD + FRAM + Display.

### Política acordada para próximos ciclos

Para futuros ciclos de mantenimiento, no será obligatorio publicar siempre una beta si la última alpha de verificación ya fue validada con el mismo rigor.

Flujo permitido para ciclos futuros:

```txt
alphas técnicas -> última alpha de verificación -> release estable
```

Beta queda como etapa opcional cuando se requiera validación pública o de terceros.

RC también queda como etapa opcional, solo si se necesita un candidato final adicional.

---

## Cambios principales hacia v2.0.0

### 1. Package estable inicial

Se prepara el ZIP estable:

```txt
jwplc-esp32-2.0.0.zip
```

Tag esperado:

```txt
v2.0.0
```

---

### 2. Canal público y canal dev

Se formaliza la separación de índices:

#### Canal público

```txt
JWPLC/package_jwplc_index.json
```

Uso:

```txt
Usuarios finales.
```

Contenido recomendado desde v2.0.0:

```txt
2.0.0
```

y futuras versiones estables o betas públicas si se decide publicarlas.

#### Canal dev / interno

```txt
JWPLC/package_jwplc_index_dev.json
```

Uso:

```txt
Validación interna.
```

Contenido recomendado:

```txt
alphas + betas + releases estables
```

Decisión:

```txt
El usuario final no debe ver decenas de alphas en Boards Manager.
```

---

### 3. README final

El README ya fue actualizado durante beta1 para representar la etapa de validación de package instalable.

Para `v2.0.0`, el README debe quedar ajustado a estado estable:

```txt
Estado de release estable v2.0.0
```

Debe mantener:

- instalación desde Boards Manager;
- URL del package index público;
- FQBN recomendado;
- configuración fija;
- periféricos soportados;
- links a librerías;
- ejemplos recomendados;
- advertencias de OpenPLC/OTA/bootloader/app-only/autoload.

---

### 4. Arduino CLI validado

Durante beta1 se validó Arduino CLI con:

```txt
arduino-cli 1.0.2
```

Pruebas ejecutadas:

```bat
arduino-cli core update-index
arduino-cli core list
arduino-cli board listall jwplc
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic .
arduino-cli compile --fqbn jwplc:esp32:jwplcbasiccore .
arduino-cli compile --fqbn jwplc:esp32:esp32 .
arduino-cli upload -p COM14 --fqbn jwplc:esp32:jwplcbasic .
arduino-cli monitor -p COM14 -c baudrate=115200
```

Resultado:

```txt
OK
```

---

### 5. Configuración estable validada

Para `JWPLC Basic` y `JWPLC Basic Core`:

| Parámetro | Valor |
|---|---|
| CPU | 240 MHz |
| Flash size | 4 MB |
| Flash frequency | 40 MHz |
| Flash mode | DIO |
| Bootloader base | QIO |
| Partition scheme | `huge_app` |
| Upload speed | 921600 |
| LoopCore | default del core |
| EventsCore | default del core |
| App maximum size | 3145728 bytes |

FQBN recomendado:

```txt
jwplc:esp32:jwplcbasic
```

---

## Librerías incluidas

| Librería | Versión |
|---|---:|
| `JW_MatrixButtons` | 1.0.4 |
| `JW_FRAM` | 1.0.2 |
| `JW_RTC` | 1.0.2 |
| `JW_SD` | 1.0.2 |
| `JWPLC_Display` | 1.0.0 |
| `JWPLC_GlobalPeripherals` | 1.0.0 |
| `JWPLC_Ethernet` | 1.0.0 |
| `JWPLC_RS485` | 1.0.0 |
| `JWPLC_ModbusRTU` | 1.0.0 |

---

## Validación acumulada

| Área | Resultado |
|---|---|
| Instalación limpia desde Boards Manager | OK |
| Instalación y manejo desde otra PC | OK |
| Arduino IDE | OK |
| Arduino CLI | OK |
| `ESP32 Board` | OK |
| `JWPLC Basic` | OK |
| `JWPLC Basic Core` | OK |
| Sketch vacío | OK |
| Sketch Serial mínimo | OK |
| Subida por USB | OK |
| Monitor serial | OK |
| Librerías bundled desde `Arduino15/packages/jwplc` | OK |
| I/O TCA6424A | OK |
| Display | OK |
| Botonera | OK |
| RTC | OK |
| FRAM | OK |
| microSD | OK |
| Ethernet W5500 | OK |
| RS-485 | OK |
| Modbus RTU | OK |
| Coexistencia SPI Ethernet + SD + FRAM + Display | OK |

---

## No incluido en v2.0.0

Esta release estable inicial no incluye:

- OpenPLC integrado real.
- OTA.
- FlashFreq 80 MHz.
- `bootloader.bin` precompilado definitivo.
- Precompilación de librerías internas.
- Precarga global de caché.
- Eliminación de periféricos del autoload normal.
- Rediseño multicore.
- Coredump formal como feature documentada de usuario.
- Partición recovery.
- Hardware de 8 MB/16 MB.

---

## Pendientes futuros

- Integración real OpenPLC.
- OTA por Wi-Fi/Ethernet/microSD.
- Evaluación de particiones recovery.
- Evaluación de hardware con 8 MB/16 MB.
- Precompilación de librerías si el beneficio real lo justifica.
- Documentación formal de coredump.
- Mejoras incrementales de ejemplos y troubleshooting.

---

## Checklist de PR

- [x] Alpha31 validada técnicamente.
- [x] Beta1 publicada como package validation.
- [x] Arduino CLI validado.
- [x] README actualizado.
- [x] Links de librerías revisados.
- [x] Ejemplos recomendados revisados.
- [ ] ZIP estable `jwplc-esp32-2.0.0.zip` generado.
- [ ] SHA-256 calculado.
- [ ] Size calculado.
- [ ] `package_jwplc_index.json` público actualizado solo con canal estable/público.
- [ ] `package_jwplc_index_dev.json` creado/actualizado con histórico dev.
- [ ] GitHub Release `v2.0.0` publicado.
- [ ] Instalación limpia de `2.0.0` validada desde Boards Manager.
- [ ] Arduino IDE validado sobre `2.0.0`.
- [ ] Arduino CLI validado sobre `2.0.0`.

---

## Decisión

Este PR prepara la publicación estable inicial:

```txt
v2.0.0
```

Si la instalación limpia de `2.0.0` no presenta bloqueantes, esta versión queda aprobada como release estable inicial de JWPLC Basic v2.0.0.

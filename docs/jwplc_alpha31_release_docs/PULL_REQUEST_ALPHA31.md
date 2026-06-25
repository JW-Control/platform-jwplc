# Pull Request - JWPLC Basic v2.0.0-alpha.31

## Título sugerido

```txt
Alpha31: release readiness, librerías JW actualizadas y preparación para beta1
```

## Ramas

```txt
Base: main
Compare: develop/alpha31-release-readiness
```

---

## Resumen

Este PR integra el trabajo de **JWPLC Basic v2.0.0-alpha.31**, enfocado en **release readiness**.

El objetivo de alpha31 es validar el package completo instalado desde cero y dejar el proyecto preparado para avanzar a `beta1-package-validation`, sin abrir features grandes ni modificar decisiones cerradas en alpha30.

---

## Cambios principales

### 1. Validación técnica preliminar

Se revisaron los archivos principales del package:

- `boards.txt`
- `platform.txt`
- `package_jwplc_index.json`
- `variants/jwplcbasic/pins_is.h`
- `cores/jwcontrol/Arduino.h`
- `cores/jwcontrol/jwplc_peripherals.h`
- `cores/jwcontrol/jwplc_peripherals.cpp`
- `cores/jwcontrol/peripherals_init.cpp`

Conclusión:

```txt
No se detectaron bloqueantes técnicos por revisión estática.
```

---

### 2. Configuración fija mantenida

Se mantiene la configuración definida en alpha30 para `JWPLC Basic` y `JWPLC Basic Core`:

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

### 3. Snapshot de librerías JW actualizado

Se integró un snapshot actualizado de librerías externas/distribuibles dentro del package:

| Librería | Versión incluida |
|---|---:|
| `JW_MatrixButtons` | `1.0.4` |
| `JW_FRAM` | `1.0.2` |
| `JW_RTC` | `1.0.2` |
| `JW_SD` | `1.0.2` |

Destino dentro del package:

```txt
JWPLC/JWPLC-2.0.0/libraries/
```

Estas librerías quedan incluidas como **bundled libraries** del package JWPLC, para que estén disponibles al instalar el package desde Boards Manager.

---

### 4. Nuevo flujo de sincronización de librerías

Se validó el flujo de sincronización desde:

```txt
JW-Control/JW-Libraries
```

hacia:

```txt
JW-Control/platform-jwplc
```

mediante rama generada:

```txt
sync/jw-libraries-alpha31
```

y PR hacia:

```txt
develop/alpha31-release-readiness
```

El snapshot queda registrado en:

```txt
docs/jw-libraries-snapshot.md
```

---

### 5. Documentación alpha31

Se agregaron documentos de trabajo para alpha31:

```txt
docs/alpha31-release-readiness-review.md
docs/alpha31-validation-checklist.md
docs/jw-libraries-snapshot.md
```

Estos documentos registran:

- alcance de alpha31;
- revisión técnica;
- checklist de instalación limpia;
- checklist IDE/CLI;
- pruebas de ejemplos;
- pruebas de periféricos;
- criterio de avance a beta1;
- snapshot de librerías incluidas.

---

## Validación realizada

### Compilación local

Se realizó una prueba local copiando el contenido actualizado del repo en la carpeta local de Arduino.

Resultado:

```txt
Compilación OK.
```

La compilación confirmó el uso de las librerías JW actualizadas desde la carpeta del package:

| Librería | Versión |
|---|---:|
| `JW_RTC` | `1.0.2` |
| `JW_FRAM` | `1.0.2` |
| `JW_SD` | `1.0.2` |
| `JW_MatrixButtons` | `1.0.4` |

También se confirmó:

```txt
Maximum is 3145728 bytes.
Upload speed: 921600.
```

Resultado de memoria observado:

```txt
Sketch usa 404497 bytes (12%) del espacio de programa.
Variables globales usan 27620 bytes (8%) de RAM dinámica.
```

---

## No incluido en alpha31

Este PR no integra ni asume:

- OpenPLC real;
- OTA;
- FlashFreq 80 MHz;
- publicación de `bootloader.bin` precompilado;
- precompilación de librerías internas;
- precarga global de caché;
- eliminación de periféricos del autoload normal;
- cambios de arquitectura mayor.

---

## Riesgos / notas conocidas

### Prioridad de librerías Arduino

Arduino puede seleccionar librerías instaladas en el sketchbook del usuario por encima de algunas librerías incluidas en el package.

En la prueba local se observó que algunas dependencias externas como Adafruit/Ethernet pueden resolverse desde:

```txt
Documents/Arduino/libraries/
```

Esto no bloquea alpha31, pero debe tenerse en cuenta durante pruebas limpias.

Las librerías JW críticas sí fueron tomadas desde la carpeta del package JWPLC.

---

## Pendientes posteriores al merge

Después de mergear este PR:

1. Generar `jwplc-esp32-2.0.0-alpha.31.zip`.
2. Publicar GitHub Pre-Release `v2.0.0-alpha.31`.
3. Calcular SHA-256 y size del ZIP.
4. Actualizar `package_jwplc_index.json`.
5. Borrar instalación local de `jwplc`.
6. Instalar desde Boards Manager.
7. Validar instalación limpia real.
8. Compilar placas principales.
9. Compilar ejemplos principales.
10. Validar periféricos principales.
11. Decidir paso a `beta1-package-validation`.

---

## Checklist de PR

- [x] Configuración fija alpha30 respetada.
- [x] `boards.txt` sin cambios no planificados.
- [x] `platform.txt` sin cambios no planificados.
- [x] Core y variantes sin cambios no planificados.
- [x] Librerías JW actualizadas dentro del package.
- [x] Documentación alpha31 agregada.
- [x] Compilación local OK.
- [x] ZIP alpha31 generado.
- [x] Pre-Release alpha31 publicado.
- [x] `package_jwplc_index.json` actualizado.
- [x] Instalación limpia desde Boards Manager validada.

---

## Decisión

Este PR deja alpha31 lista para publicación como **Pre-Release**, luego de generar ZIP, checksum, size y actualizar el package index.

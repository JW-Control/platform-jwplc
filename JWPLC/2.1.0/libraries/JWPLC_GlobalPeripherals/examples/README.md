# Ejemplos JWPLC_GlobalPeripherals

Para empezar un proyecto real, usar la plantilla estándar:

```text
00.Plantilla_Proyecto_JWPLC   pestañas fijas A…K + LEEME.md + docs/ (ayuda sin internet)
```

Ver su `LEEME.md`: Archivo → Guardar como… con el nombre del proyecto y llenar `A_Config.h`,
`docs/MAPA_IO.md` y `B_Tags.h`.

Serie recomendada para taller JWPLC Basic:

```text
01.DigitalIO_Basic
02.Buttons_Basic
03.RTC_Basic
04.FRAM_Basic
05.microSD_Basic
06.IO_Estado_Escaneo      (antes 06.Runtime_Cache_Status)
07.Buzzer_Basic
```

Curso paso a paso: `Curso_JWPLC_Basic/` (lecciones 01 a 11).

Estos ejemplos usan las **instancias globales del JWPLC Basic**. Por diseño no se copiaron a `JW_RTC`, `JW_FRAM`, `JW_SD` ni `JW_MatrixButtons`, porque esas librerías `JW_*` continúan siendo drivers genéricos reutilizables.

Los ejemplos históricos no numerados (`DigitalIO_Block*`, `JWPLC_IO_BlockMirror`) se retiraron en 2.1.0-alpha.17: `01.DigitalIO_Basic` ya muestra las E/S en bloque con `digitalReadBlock(I0_X)` y `digitalWriteBlock(Q0_X, bits)`.

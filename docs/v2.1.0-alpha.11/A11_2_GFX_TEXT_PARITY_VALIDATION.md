# Alpha11 — Validación física A11-2 GFX Text Parity

Fecha: 2026-09-05

## Objetivo

Validar que el renderer RAW del JWPLC HMI Designer reproduce visualmente la celda clásica de Adafruit GFX utilizada por el JWPLC Basic físico.

## Muestra canónica

```text
Texto       : TEMP: 25.6 C
X           : 20
Y           : 20
textSize    : 2
Foreground  : RED / 0xF800
Background  : WHITE / 0xFFFF
Bounds GFX  : 144 x 16 px
```

## Evidencia

Se compararon:

1. captura del Designer con la muestra canónica;
2. fotografía del JWPLC Basic físico ejecutando `A11_2_GFX_Text_Parity.ino`.

En ambas se observa el mismo patrón de caracteres para:

```text
TEMP: 25.6 C
```

incluyendo:

- forma de los glifos clásicos;
- espaciado horizontal de la celda 6x8 escalada a 2x;
- foreground rojo;
- background blanco de la celda GFX;
- ausencia de padding simétrico artificial;
- mismo orden y composición de caracteres.

La fotografía física contiene perspectiva, rotación y artefactos propios de cámara/panel, por lo que no se usa para una comparación raster matemática 1:1. Para el gate Alpha11 se considera suficiente la coincidencia visual de la muestra canónica contra el mismo contrato GFX y dimensiones lógicas del Designer.

## Resultado

```text
A11_2_GFX_CLASSIC_FONT=PASS
A11_2_TEXT_SIZE_2X=PASS
A11_2_FOREGROUND_BACKGROUND=PASS
A11_2_CELL_GEOMETRY_144X16=PASS
A11_2_RAW_NO_SYMMETRIC_PADDING=PASS
A11_2_PHYSICAL_VISUAL_MATCH=PASS
A11_2_GFX_TEXT_PARITY=PASS
```

## Decisiones conservadas

```text
GFX_RAW_TOOL=DIAGNOSTIC_ONLY
RAW_GFX_PADDING_ARTIFICIAL=NO
JWPLC_DISPLAY_RUNTIME_CHANGE=NO
JWPLC_UI_FIELD_PADDING_CHANGE=NO
```

A11-3 puede comenzar sobre los fields declarativos reales (`VALUE`, `TEXT`, `BOOL`, `BAR`) y debe reproducir la geometría actual de `JWPLC_UIField` separadamente del modo GFX RAW.

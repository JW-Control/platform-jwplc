# Alpha11 — A11-5 Paridad física integral

Fecha: 2026-09-06

## Objetivo

Validar que una HMI diseñada en JWPLC HMI Designer conserve la misma salida observable en las cuatro representaciones del flujo Alpha11:

```text
1. Canvas del Designer
2. Vista previa 1:1
3. LIVE Web Serial
4. Sketch compilado desde JWPLC_HMI_Generated.h
```

A11-5 no busca agregar nuevas funciones. Busca demostrar paridad visual y de comportamiento sobre lo ya cerrado.

## Precondiciones

```text
A11_3A_TEXT_FIELD=PASS
A11_3B_VALUE_FIELD=PASS
A11_3C_BOOL_FIELD=PASS
A11_3D_BAR_FIELD=PASS
A11_3E_MULTI_FIELD_PAGES=PASS
A11_4_CODEGEN=PASS
A11_BUTTON_ROBUSTNESS=PASS_PHYSICAL
A11_LIVE_TRANSPORT=FROZEN_ALPHA11
```

## Composición recomendada

Usar al menos 3 páginas y mezclar todos los tipos:

```text
Página 01 · Principal
  TEXT
  VALUE
  BOOL
  BAR

Página 02 · Proceso
  TEXT
  VALUE
  BOOL
  BAR

Página 03 · Diagnóstico
  TEXT
  VALUE
  BOOL
  BAR
```

No es obligatorio usar 12 fields si una composición más compacta demuestra todas las variantes; sí es obligatorio cubrir los cuatro tipos, más de una página y estilos diferentes.

## Variantes que deben cubrirse

### TEXT

```text
INLINE
STACKED
LEFT / CENTER / RIGHT
valueSize >= 2
distinto labelSize
frame ON/OFF
colores distintos
```

### VALUE

```text
integerDigits
 decimalDigits
signed true/false
leadingZeros true/false
unidad presente/ausente
INLINE / STACKED
LEFT / CENTER / RIGHT
```

### BOOL

```text
FALSE / TRUE
textos personalizados
INLINE / STACKED
alineaciones
frame ON/OFF
```

### BAR

```text
min/max no triviales
valor intermedio
valor mínimo
valor máximo
unidad
STACKED
frame ON/OFF
colores distintos
```

## Páginas e indicador

Debe verificarse:

```text
NN/TT_PAGE_NUMBER=PARITY
SELECT_BLACK_WHITE=PARITY
CONTENT_WHITE_BLACK=PARITY
LEFT_RIGHT_PAGE_CHANGE=PASS
OK_ENTERS_CONTENT=PASS
ESC_RETURNS_SELECT=PASS
NO_PENDING_OK_REENTRY=PASS
```

## Gate visual

Para cada página comparar:

```text
GEOMETRY_XY=PARITY
DECLARATIVE_BOUNDS=PARITY
TEXT_BASELINE=PARITY
LABEL_VALUE_GAP=PARITY
ALIGNMENT=PARITY
FRAME=PARITY
RGB565_COLORS=PARITY
BACKGROUND_CLEAR=PARITY
PAGE_INDICATOR=PARITY
```

Tolerancia esperada:

```text
DECLARATIVE_GEOMETRY_TOLERANCE=0_PX
```

Si una diferencia depende exclusivamente de la captura/cámara física, debe distinguirse de una diferencia real de framebuffer/TFT.

## LIVE vs sketch generado

Se debe comprobar que una composición enviada por LIVE y la misma composición compilada desde `JWPLC_HMI_Generated.h` producen el mismo resultado físico.

```text
LIVE_PHYSICAL=REFERENCE
GENERATED_SKETCH_PHYSICAL=MUST_MATCH
DIRECT_TFT_CALLS=NO
PUBLIC_API_ONLY=YES
```

## Refresh / estabilidad

Durante el gate:

```text
INDICATOR_FLICKER=0
UNEXPECTED_FIELD_FLICKER=0
BUTTON_LOCKUPS=0
ESC_FAILURES=0
PENDING_INPUT_REENTRY=0
```

No se requiere Serial ni `delay()` de usuario para estabilizar la botonera.

## Evidencia mínima

Guardar:

```text
- captura del Designer por página;
- captura de vista previa 1:1;
- foto/captura del JWPLC en LIVE;
- foto/captura del JWPLC ejecutando el sketch generado;
- notas de cualquier diferencia encontrada.
```

## Criterio de cierre

```text
A11_5_TEXT_PARITY=PASS
A11_5_VALUE_PARITY=PASS
A11_5_BOOL_PARITY=PASS
A11_5_BAR_PARITY=PASS
A11_5_PAGE_PARITY=PASS
A11_5_LIVE_VS_GENERATED=PASS
A11_5_NO_FLICKER=PASS
A11_5_BUTTON_STABILITY=PASS
A11_5_PHYSICAL_PARITY=PASS
NEXT=A11_6_SKETCH_INTEGRATION
```

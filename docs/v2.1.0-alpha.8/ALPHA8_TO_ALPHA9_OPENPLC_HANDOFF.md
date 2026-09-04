# Transferencia Alpha8 -> Alpha9 — HMI Arduino hacia OpenPLC

Fecha: 2026-09-04

## Propósito

Este documento fija la frontera entre el cierre de Alpha8 y el inicio de Alpha9.

```text
Alpha8 = TFT + botonera + HMI Arduino eficiente
Alpha9 = exposición de esa HMI hacia OpenPLC/Ladder
```

Alpha8 no debe reabrirse para introducir integración OpenPLC salvo un bloqueante de publicación.

## Estado que Alpha9 puede asumir

### Display

Objeto público:

```cpp
JWPLC_Display
```

APIs HMI disponibles:

```cpp
JWPLC_Display.setFields(...);
JWPLC_Display.clearFields();
JWPLC_Display.fieldCount();

JWPLC_Display.setValue(...);
JWPLC_Display.setText(...);
JWPLC_Display.setBool(...);
JWPLC_Display.setBar(...);

JWPLC_Display.setUserPage(...);
JWPLC_Display.userPage();

JWPLC_Display.setUserRefreshMode(...);
JWPLC_Display.requestUserRefresh();

JWPLC_Display.invalidateField(...);
JWPLC_Display.invalidateAllFields();

JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

Tipos de campo:

```text
JWPLC_UI_FIELD_VALUE
JWPLC_UI_FIELD_TEXT
JWPLC_UI_FIELD_BOOL
JWPLC_UI_FIELD_BAR
```

Refresh:

```text
USER_REFRESH_ON_DEMAND
USER_REFRESH_PERIODIC
```

### Botonera

Objeto público:

```cpp
JWPLC_Buttons
```

IDs:

```text
BTN_LEFT
BTN_UP
BTN_RIGHT
BTN_ESC
BTN_OK
BTN_DOWN
```

APIs principales:

```cpp
JWPLC_Buttons.pressed(...);
JWPLC_Buttons.released(...);
JWPLC_Buttons.isDown(...);
JWPLC_Buttons.eventCount();
JWPLC_Buttons.getEvent(...);
```

El Display no consume los latches de aplicación para su navegación interna.

### Runtime cacheado

```cpp
JWPLC_IO
JWPLC_Time
```

Ejemplos:

```cpp
JWPLC_IO.inputs();
JWPLC_IO.outputs();
JWPLC_IO.input(0);
JWPLC_IO.output(0);

JWPLC_Time.hour();
JWPLC_Time.minute();
JWPLC_Time.second();
JWPLC_Time.valid();
```

Estas fachadas no generan nuevas transacciones físicas.

## Restricciones que Alpha9 debe preservar

1. No eliminar periféricos del autoload normal.
2. No hacer que OpenPLC sea obligatorio para sketches Arduino.
3. No romper la API pública Alpha8.
4. No volver a hacer que Display consuma `pressed()/released()` de aplicación.
5. No introducir redibujados completos periódicos si dirty refresh ya cubre el caso.
6. No hacer lecturas SPI/I2C redundantes desde la capa gráfica cuando existen snapshots.
7. No asumir OTA.
8. No fijar FlashFreq universal final.
9. No adoptar `bootloader.bin` definitivo.
10. Mantener compatibilidad Arduino IDE/CLI.

## Frontera propuesta para OpenPLC

Alpha9 debe tratar la HMI como una capa ya existente y exponer un bridge hacia ella, no duplicar un segundo motor gráfico.

Modelo conceptual:

```text
OpenPLC / Ladder
      |
      v
Bridge Alpha9
      |
      +--> JWPLC_Display.setValue(...)
      +--> JWPLC_Display.setBool(...)
      +--> JWPLC_Display.setBar(...)
      +--> JWPLC_Display.setUserPage(...)
      |
      +<-- JWPLC_Buttons / eventos
      +<-- JWPLC_IO / JWPLC_Time si corresponde
```

El bridge debe permanecer opcional desde la perspectiva Arduino.

## Preguntas que Alpha9 debe cerrar

- cómo declarar campos HMI desde el flujo OpenPLC/VPP;
- cómo mapear variables Ladder a `fieldId`;
- si el mapa se genera en build-time o se registra en runtime;
- cómo exponer páginas;
- cómo exponer botones como variables/eventos Ladder;
- política de actualización Ladder -> HMI;
- política HMI/button -> Ladder;
- manejo de tipos numéricos/bool/text/bar;
- límites de número de campos/páginas visibles para el editor;
- compatibilidad con VPP/HAL existente;
- impacto de scan-time;
- recuperación si OpenPLC no define HMI;
- comportamiento cuando el sketch/runtime Arduino no usa OpenPLC.

## No asumir al arrancar Alpha9

```text
OpenPLC integrado al package Arduino = NO
OTA definida                         = NO
FlashFreq universal final           = NO
bootloader.bin definitivo           = NO
```

## Evidencia que Alpha9 hereda

```text
ALPHA8_NO_SPONTANEOUS_AUTOWAKE=PASS
ALPHA8_HMI_HARDWARE_GATE=PASS
ALPHA8_DISPLAY_DOES_NOT_STEAL_APP_LATCH=PASS
ALPHA8_HELD_BUTTON_REGRESSION=PASS
ALPHA8_COMPILER_COUNT_PARITY=PASS
ALPHA8_LAZYLINK_PRECOMPILED=PASS
ALPHA8_EMPTY_HMI_ENGINE_LINKED=NO
```

## Base recomendada para iniciar Alpha9

Preferencia:

```text
v2.1.0-alpha.8 publicada y validada
```

No iniciar Alpha9 desde un branch intermedio de Alpha8 si la publicación todavía puede modificar el package.

Si por agenda se prepara trabajo exploratorio antes del release, no mezclar esos commits con el PR técnico de Alpha8.

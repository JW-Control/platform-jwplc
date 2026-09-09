# Alpha11 A11-6 — Seguimiento UX de escritorio e integración Arduino IDE

Fecha: 2026-09-06

## Gates confirmados por usuario en Windows

```text
DESKTOP_APP_MODE=PASS_USER_WINDOWS
DESIGNER_FULL_LOAD=PASS_USER_WINDOWS
PROJECT_OPEN_SAVE_UI=PASS_USER_WINDOWS
SKETCH_LINK_UI=PASS_USER_WINDOWS
HEADER_UPDATE_UI=PASS_USER_WINDOWS
LIVE_AVAILABLE=PASS_USER_WINDOWS
```

También se confirmó visualmente que `JWPLC_HMI_Generated.h` puede actualizarse dentro del sketch vinculado.

## Responsive / ventana compacta

Observación física de uso: al ocupar aproximadamente media pantalla, la distribución de tres columnas del Designer reducía demasiado el área central y producía recorte horizontal.

Decisión Alpha11:

```text
DESKTOP_RESPONSIVE_LAYOUT=YES
COMPACT_BREAKPOINT=1180px
COMPACT_LEFT_PANEL=VISIBLE
COMPACT_EDITOR=PRIMARY
COMPACT_INSPECTOR=OVERLAY_ON_DEMAND
COMPACT_TOOLBAR=HORIZONTAL_SCROLL
LOGICAL_TFT_SIZE_UNCHANGED=320x170
```

El renderer y las coordenadas HMI no cambian. Sólo cambia la distribución del shell de escritorio.

En modo compacto aparece un botón `Inspector` que abre/cierra el panel derecho como overlay. El canvas conserva scroll propio y el botón `Ajustar` continúa disponible.

## Identidad del sketch vinculado

Después de vincular un sketch debe ser visible su identidad sin depender únicamente de una notificación temporal.

Contrato:

```text
LINK_BUTTON_TEXT=Sketch: <nombre>
WINDOW_TITLE=<nombre> · JWPLC HMI Designer — Alpha11
STATUSBAR_SKETCH=VISIBLE
```

El botón sigue siendo accionable para cambiar la vinculación.

## Ubicación de `.jwhmi`

Convención recomendada:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

`.jwhmi` es un archivo de proyecto del Designer y no es una unidad de compilación Arduino. Mantenerlo junto al sketch facilita versionado, respaldo y regeneración del header.

Alpha11 no modifica automáticamente el `.ino`.

## Botón dentro de Arduino IDE 2

Objetivo del usuario: poder abrir JWPLC HMI Designer desde Arduino IDE sin navegar por GitHub ni por `tools/`.

Decisión para Alpha11:

```text
ARDUINO_IDE_DIRECT_TOOLBAR_BUTTON=NOT_REQUIRED_ALPHA11
ARDUINO_IDE_FORK=NO
UNSUPPORTED_IDE_PATCHING=NO
STANDALONE_DESKTOP_APP=PRIMARY
DESKTOP_SHORTCUT=YES
START_MENU_SHORTCUT=YES
PWA_INSTALL=YES
```

Arduino IDE 2 está basado en Eclipse Theia, pero la instalación/distribución de extensiones de terceros no forma parte de un flujo estable que debamos acoplar al package JWPLC. Una integración que dependa de modificar internamente el IDE o de una extensión no soportada rompería la prioridad de compatibilidad Arduino IDE.

Por ello, Alpha11 mantiene el Designer como aplicación independiente y un launcher de un clic. Para distribución pública debe empaquetarse como artefacto instalable/portable de release, de modo que el usuario no tenga que navegar por el repositorio.

Una integración IDE directa puede reevaluarse en un alpha futuro sólo si existe una vía soportada y estable.

## Gate pendiente

```text
1. Pull del nuevo responsive shell.
2. Abrir Designer a pantalla completa.
3. Reducir a aproximadamente media pantalla.
4. Confirmar que el editor sigue usable y el panel derecho no comprime el canvas.
5. Abrir/cerrar Inspector compacto.
6. Vincular sketch y confirmar nombre visible en botón/título.
7. Confirmar que LIVE continúa disponible.
```

Si pasa:

```text
A11_6_RESPONSIVE_LAYOUT=PASS
A11_6_DESKTOP_UX=PASS
NEXT=A11_6_FINAL_INTEGRATION_AND_ALPHA11_CLOSE
```

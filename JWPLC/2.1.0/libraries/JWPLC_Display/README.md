# JWPLC_Display

Librería del package **JWPLC ESP32** para la TFT ST7789 integrada del **JWPLC Basic**.

Estado documentado: **v2.1.0-alpha.12 en cierre**.

La API HMI de usuario validada en Alpha11 se conserva. Alpha12 cambia la arquitectura interna de renderizado/runtime para mejorar coexistencia y rendimiento bajo carga.

`JWPLC_Display` integra:

1. pantalla automática `IDLE` del sistema;
2. pantalla `USER`;
3. HMI declarativa `JWPLC_UI`;
4. navegación multipágina;
5. PixelMap RGB565 y `PACKED_SPAN16`;
6. dirty refresh / on-demand refresh;
7. acceso gráfico avanzado mediante `JWPLC_TFT`.

La API pública recomendada usa el objeto global:

```cpp
JWPLC_Display
```

La API histórica `JWPLCDisplay::` y algunos aliases/getters continúan por compatibilidad, pero no son la forma recomendada para código nuevo.

---

## Inicialización automática

En `JWPLC Basic` la TFT forma parte del autoload normal.

El sketch no debe:

- crear otra instancia `Adafruit_ST7789` para la TFT integrada;
- reinicializar sus pines;
- llamar un `begin()` paralelo;
- apropiarse del SPI compartido sin la coordinación del runtime.

Consulta básica:

```cpp
if (JWPLC_Display.isReady())
{
    // TFT lista
}
```

Alpha11 estabiliza además el arranque manteniendo `TFT_RST` controlado durante autoload y dibujando el primer IDLE inmediatamente después de inicializar la TFT.

---

## Configuración IDLE / USER

### Wake desde IDLE

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
JWPLC_Display.setIdleWakeButton(BTN_OK);
```

Modos:

```text
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

### Retorno a IDLE

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setIdleReturnButton(BTN_ESC);
```

Modos:

```text
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_BUTTON_ONLY
IDLE_RETURN_DISABLED
```

Para retorno por tiempo:

```cpp
JWPLC_Display.setIdleTimeoutMs(15000);
```

Entrada/salida manual:

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

---

## Refresh

Refresh IDLE:

```cpp
JWPLC_Display.setIdleRefreshPeriodMs(50);
```

Refresh USER:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
JWPLC_Display.requestUserRefresh();
```

O periódico:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_PERIODIC);
JWPLC_Display.setUserRefreshPeriodMs(50);
```

Modos:

```text
USER_REFRESH_ON_DEMAND
USER_REFRESH_PERIODIC
```

---

## HMI Designer Alpha11

JWPLC HMI Designer genera:

```text
JWPLC_HMI_Generated.h
```

El header contiene la capa de presentación:

```text
HMIPageId
HMIFieldId
variables HMI
JWPLC_UIField[]
PixelMaps
jwplcHMISetup()
jwplcUIUpdate()
```

El `.ino` conserva la lógica del programa.

Ejemplo:

```cpp
#include <JWPLC_Display.h>
#include <JWPLC_HMI_Generated.h>

void setup()
{
    jwplcHMISetup();

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
    JWPLC_Display.setIdleWakeButton(BTN_OK);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
}

void loop()
{
    // lógica de proceso
}
```

Proyecto recomendado:

```text
MiProyecto/
├─ MiProyecto.ino
├─ MiProyecto.jwhmi
└─ JWPLC_HMI_Generated.h
```

---

## Fields declarativos

Tipos V1:

```text
TEXT
VALUE
BOOL
BAR
```

Límite actual:

```text
JWPLC_UI_MAX_FIELDS=32
```

Helpers:

```cpp
JWPLC_UITextField(...)
JWPLC_UIValueField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

Registro manual/avanzado:

```cpp
JWPLC_Display.setFields(fields, count);
JWPLC_Display.clearFields();
```

El flujo normal con Designer genera este registro automáticamente.

---

## Actualizar valores

API recomendada general:

```cpp
JWPLC_Display.setValue(FIELD_TEMP, temperatura);
JWPLC_Display.setValue(FIELD_RUN, true);
JWPLC_Display.setValue(FIELD_STATUS, "READY");
```

`setValue()` dispone de overloads para valores numéricos, `bool` y `const char *`.

Para barra:

```cpp
JWPLC_Display.setBar(FIELD_LOAD, 75.0f);
```

También existen por compatibilidad/especialización:

```cpp
JWPLC_Display.setText(...);
JWPLC_Display.setBool(...);
```

pero el autocompletado Alpha11 prioriza `setValue()` para no presentar múltiples caminos equivalentes al usuario.

---

## Navegación multipágina

Página actual:

```cpp
JWPLC_Display.setUserPage(0);
```

El Designer configura el conteo total y genera IDs simbólicos.

Semántica física Alpha11:

```text
PAGE_SELECT
  LEFT / RIGHT -> cambiar página
  OK           -> entrar a contenido

PAGE_CONTENT
  LEFT / RIGHT / UP / DOWN / OK -> aplicación
  ESC                            -> selector
```

El indicador visible usa formato:

```text
NN/TT
```

El retorno desde CONTENT limpia input pendiente para evitar reingresos fantasma.

---

## PixelMap

Alpha11 soporta PixelMaps estáticos agrupados por página.

Formatos:

```text
RGB565_RUN
PACKED_SPAN16
```

Registro manual/avanzado:

```cpp
JWPLC_Display.setPixelMaps(maps, count);
JWPLC_Display.setPackedPixelMaps(maps, count);
```

El Designer elige automáticamente `PACKED_SPAN16` cuando resulta más compacto y la paleta cabe en 16 colores.

Visibilidad runtime:

```cpp
JWPLC_Display.setPixelMapVisible(PIXELMAP_INDEX, true);
JWPLC_Display.setPixelMapVisible(PIXELMAP_INDEX, false);
```

---

## Botonera y Display

Display y aplicación son consumidores independientes.

El sketch puede usar:

```cpp
JWPLC_Buttons.pressed(BTN_OK);
JWPLC_Buttons.released(BTN_ESC);
JWPLC_Buttons.isDown(BTN_UP);
```

sin añadir `delay()` ni Serial para estabilizar el runtime.

Alpha11 validó físicamente loops cerrados y corrigió la priorización/servicio interno necesario para que la interfaz no dependa de pausas artificiales del usuario.

---

## Runtime cerrado y `digitalWrite()`

También se validó el caso habitual:

```cpp
void loop()
{
    bool q0 = /* condición */;
    digitalWrite(Q0_0, q0);
}
```

No es necesario escribir sólo cuando cambia ni añadir `delay(1)`.

El core Alpha11 usa shadow de salida TCA6424A para que una escritura redundante del mismo estado no genere una transacción I2C innecesaria, y mantiene el servicio periódico de IO/RTC/Display.

```text
ALPHA11_USER_DELAY_REQUIRED=NO
ALPHA11_DIGITALWRITE_REPEATED_STATE=PASS_PHYSICAL
```

---

## Pantalla IDLE

La pantalla base muestra información del sistema, incluyendo:

- `PWR`;
- `RUN`;
- `ERR`;
- `BUS`;
- `ETH`;
- entradas `I0.0..I0.7`;
- salidas `Q0.0..Q0.7`;
- RTC cuando está disponible.

El runtime usa snapshots internos y no obliga al sketch a repetir lecturas físicas sólo para mantener el IDLE.

---

## Indicadores

RUN:

```cpp
JWPLC_Display.setRunLed(true);
```

ERR recomendado:

```cpp
JWPLC_Display.setErrCode("A01");
```

BUS automático:

```cpp
JWPLC_Display.setBusLedAuto(true);
```

ETH automático:

```cpp
JWPLC_Display.setEthLedAuto(true);
```

---

## Acceso gráfico avanzado

Forma recomendada:

```cpp
auto &tft = JWPLC_Display.tft();
```

`display()` continúa como alias:

```cpp
auto &tft2 = JWPLC_Display.display();
```

En Alpha12 ambos retornan:

```cpp
JWPLC_TFTClass &
```

y no `Adafruit_ST7789&`.

La API recomendada desde Alpha11 ya usaba `auto &`, por lo que ese patrón se
mantiene compatible si las primitivas utilizadas existen en `JWPLC_TFT`.

Un sketch que haya declarado explícitamente:

```cpp
Adafruit_ST7789 &tft = JWPLC_Display.tft();
```

requiere migración. Este caso se considera compatibilidad avanzada/raw y queda
registrado en el checklist Alpha12 antes del freeze.

La HMI generada por Designer no necesita acceso directo al backend.

---

## Autocompletado Arduino IDE Alpha11

La extensión JWPLC HMI para Arduino IDE 2.3.4 ofrece sugerencias contextuales.

Al escribir:

```cpp
JWPLC_Display.
```

se muestra una lista curada de API recomendada.

Dentro de setters se proponen valores válidos:

```text
setIdleWakeMode(    -> IDLE_WAKE_*
setIdleWakeButton(  -> BTN_*
setIdleReturnMode(  -> IDLE_RETURN_*
setIdleReturnButton(-> BTN_*
setUserRefreshMode( -> USER_REFRESH_*
```

Los getters/aliases compatibles no se eliminan de la API; simplemente no se priorizan cuando pueden confundir al usuario.

---

## Coexistencia SPI

La TFT comparte SPI con:

- W5500;
- FRAM;
- microSD.

El runtime utiliza el mutex SPI global JWPLC.

Reglas:

1. usar snapshots/cache cuando sea posible;
2. evitar operaciones SPI largas durante dibujo;
3. actualizar regiones dirty;
4. dejar que `JWPLC_Display` gestione el bus para la HMI normal.

---

## Precompilación en el cierre Alpha12

Durante el desarrollo y la consolidación Alpha12, `JWPLC_Display` está en
modo source-first: `library.properties` no declara todavía
`precompiled=full`.

El archive Alpha11 se conserva sólo como evidencia histórica. No se debe
publicar reutilizando su SHA como si representara las fuentes actuales.

Antes de publicar Alpha12:

1. congelar las fuentes;
2. regenerar `libJWPLC_Display.a`;
3. regenerar/validar también `libJWPLC_TFT.a`;
4. registrar SHA-256/tamaños nuevos;
5. restaurar la política precompilada sólo si la paridad source/archive pasa;
6. repetir compile/upload/runtime final.

Estado actual:

```text
DISPLAY_SOURCE_FIRST=YES
DISPLAY_ALPHA11_ARCHIVE_FINAL_FOR_ALPHA12=NO
DISPLAY_ALPHA12_ARCHIVE_REGEN=PENDING
```

---

## Compatibilidad

Se conservan APIs históricas cuando no existe motivo para romper sketches ya probados.

Eso incluye, entre otras:

```text
JWPLCDisplay::
setText()
setBool()
display()
getters de configuración
callbacks USER legacy
```

Para código nuevo se recomienda seguir la API curada mostrada por el autocompletado y por JWPLC HMI Designer.

---

## Estado Alpha12

```text
JWPLC ESP32 v2.1.0-alpha.12
JWPLC_Display 1.0.1
HMI_DESIGNER_V1=RETAINED_FROM_ALPHA11
BACKEND=JWPLC_TFT
AUTOLOAD_DISPLAY=YES
FULL_RUNTIME_DISPLAY=PASS_PHYSICAL
PRECOMPILED_FINAL_ALPHA12=PENDING_REGEN
RAW_BACKEND_EXPLICIT_ADAFRUIT_TYPE_COMPATIBILITY=REVIEW
```

Alpha13 será el ciclo dedicado a continuar la evolución funcional de TFT/Display.
Alpha12 sólo consolida la arquitectura que ya fue necesaria y validada durante
el hardening de comunicaciones/full runtime.

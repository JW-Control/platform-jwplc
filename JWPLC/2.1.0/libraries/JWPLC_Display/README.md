# JWPLC_Display

Librería del package **JWPLC ESP32** para la TFT ST7789 integrada del **JWPLC Basic**.

En `v2.1.0-alpha.8`, `JWPLC_Display` consolida tres capas que pueden usarse juntas o por separado:

1. pantalla automática `IDLE` del sistema;
2. pantalla `USER` para interfaces propias;
3. motor HMI declarativo de campos para construir interfaces sin redibujar toda la TFT manualmente.

La API pública recomendada usa siempre el objeto global:

```cpp
JWPLC_Display
```

La API histórica `JWPLCDisplay::` se conserva como compatibilidad interna/legacy, pero no es la forma recomendada para código nuevo.

---

## Inicialización automática

En JWPLC Basic la TFT forma parte del autoload normal del package.

El sketch no debe:

- crear otra instancia `Adafruit_ST7789` para la TFT integrada;
- volver a inicializar los pines del display;
- llamar un `begin()` paralelo;
- apropiarse del bus SPI sin usar la coordinación del runtime.

Un sketch puede consultar:

```cpp
if (JWPLC_Display.isReady())
{
    // TFT lista
}
```

---

## Cambio importante de Alpha8: IDLE seguro por defecto

Alpha8 cambia el comportamiento por defecto para evitar transiciones inesperadas a `USER`.

```text
Default Alpha8:
IDLE_WAKE_DISABLED
```

Por lo tanto, una pulsación física no despierta automáticamente la pantalla `USER` salvo que el sketch lo solicite explícitamente.

Entrada manual recomendada:

```cpp
if (JWPLC_Buttons.pressed(BTN_OK))
{
    JWPLC_Display.enterUserUI();
}
```

Si una aplicación sí desea wake automático:

```cpp
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_ANY_BUTTON);
```

o sólo con un botón:

```cpp
JWPLC_Display.setIdleWakeButton(BTN_OK);
JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
```

Modos disponibles:

```text
IDLE_WAKE_ANY_BUTTON
IDLE_WAKE_BUTTON_ONLY
IDLE_WAKE_DISABLED
```

---

## Botonera y Display son consumidores independientes

En Alpha8 la navegación del Display no consume los latches de aplicación `pressed()` / `released()`.

El Display observa el estado físico y calcula sus propios flancos de navegación. El sketch puede seguir recibiendo la misma pulsación mediante:

```cpp
JWPLC_Buttons.pressed(BTN_ESC);
JWPLC_Buttons.released(BTN_ESC);
JWPLC_Buttons.isDown(BTN_ESC);
```

Esto permite, por ejemplo, que `ESC` haga retornar el Display a `IDLE` y que el sketch también reciba el evento correspondiente.

El gate físico Alpha8 validó este comportamiento con los seis botones del JWPLC Basic.

---

## Pantalla IDLE

La pantalla base del sistema muestra:

- `PWR`;
- `RUN`;
- `ERR`;
- `BUS`;
- `ETH`;
- `I0.0..I0.7`;
- `Q0.0..Q0.7`;
- RTC cuando está disponible.

El runtime actualiza esta pantalla usando snapshots internos; el sketch no necesita leer otra vez RTC/I/O sólo para mantener el IDLE.

---

## Pantalla USER

La entrada y salida explícitas son:

```cpp
JWPLC_Display.enterUserUI();
JWPLC_Display.goIdle();
```

El modo de retorno puede configurarse con:

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_ESC_ONLY);
JWPLC_Display.setIdleReturnButton(BTN_ESC);
JWPLC_Display.setIdleTimeoutMs(15000);
```

Modos disponibles:

```text
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
IDLE_RETURN_BUTTON_ONLY
```

---

# HMI declarativa Alpha8

## Objetivo

La HMI declarativa permite definir campos una vez y luego actualizar valores mediante una API de alto nivel.

El motor evita que el usuario tenga que hacer continuamente:

```cpp
tft.fillScreen(...);
tft.setCursor(...);
tft.print(...);
```

para cada cambio de dato.

Alpha8 incorpora:

- hasta 32 campos;
- varias páginas USER;
- valor numérico;
- texto;
- booleano;
- barra;
- formato numérico fijo;
- alineación;
- colores;
- redibujado dirty-only;
- caché de valores;
- refresh bajo demanda o periódico;
- integración con `JWPLC_IO` y `JWPLC_Time`.

No utiliza `new` ni `String` dinámico para el almacenamiento interno de los valores.

---

## Tipos de campo

```text
JWPLC_UI_FIELD_VALUE
JWPLC_UI_FIELD_TEXT
JWPLC_UI_FIELD_BOOL
JWPLC_UI_FIELD_BAR
```

Layouts:

```text
JWPLC_UI_LAYOUT_INLINE
JWPLC_UI_LAYOUT_STACKED
```

Alineación:

```text
JWPLC_UI_ALIGN_LEFT
JWPLC_UI_ALIGN_CENTER
JWPLC_UI_ALIGN_RIGHT
```

Refresh USER:

```text
USER_REFRESH_ON_DEMAND
USER_REFRESH_PERIODIC
```

---

## Definir campos

Ejemplo de un valor numérico:

```cpp
static const JWPLC_UIField FIELDS[] = {
    {
        {1, 0, JWPLC_UI_FIELD_VALUE},
        {10, 95, JWPLC_UI_AUTO, JWPLC_UI_AUTO},
        {"Temp", "C"},
        {1, 2, true, JWPLC_UI_LAYOUT_INLINE, JWPLC_UI_ALIGN_RIGHT},
        {ST77XX_WHITE, ST77XX_WHITE, ST77XX_BLACK, ST77XX_WHITE},
        {3, 1, true, false}
    }
};

void setup()
{
    JWPLC_Display.setFields(FIELDS, sizeof(FIELDS) / sizeof(FIELDS[0]));
}
```

Los grupos conceptuales de un campo son:

```text
meta      -> id, page, type
rect      -> x, y, width, height
text      -> label, unit, capacity
style     -> labelTextSize, valueTextSize, frame, layout, valueAlign
colors    -> label, value, background, frame
format    -> integerDigits, decimalDigits, signedValue, leadingZeros
boolText  -> falseText, trueText
barRange  -> min, max
```

Los IDs deben ser únicos dentro de la HMI, incluso si los campos pertenecen a páginas distintas.

Las cadenas usadas en definiciones deben conservar una vida útil válida durante el uso de la HMI. Para etiquetas fijas se recomiendan literales o almacenamiento estático.

---

## Helpers de definición

También existen helpers para construir campos:

```cpp
JWPLC_UIValueField(...)
JWPLC_UITextField(...)
JWPLC_UIBoolField(...)
JWPLC_UIBarField(...)
```

Estos helpers mantienen la misma semántica del `JWPLC_UIField` directo y son útiles cuando se prefiere una declaración más compacta.

---

## Actualizar valores

API recomendada:

```cpp
JWPLC_Display.setValue(FIELD_COUNTER, counter);
JWPLC_Display.setValue(FIELD_TEMP, temperature);
JWPLC_Display.setText(FIELD_STATUS, "READY");
JWPLC_Display.setBool(FIELD_RUN, true);
JWPLC_Display.setBar(FIELD_LOAD, 75.0f);
```

`setValue()` dispone de overloads para valores numéricos, `bool` y `const char *`.

Cuando el valor formateado no cambia, el campo no se marca dirty innecesariamente.

---

## Formato numérico

El formato permite definir:

```text
integerDigits
decimalDigits
signedValue
leadingZeros
```

Reglas relevantes:

- si `signedValue=false`, un valor negativo no es válido para el campo;
- valores no representables muestran hashes de overflow;
- valores no finitos también se tratan como overflow;
- `leadingZeros=true` rellena la magnitud según el ancho configurado;
- el signo se conserva fuera de ese relleno.

---

## Páginas USER

```cpp
JWPLC_Display.setUserPage(0);
JWPLC_Display.setUserPage(1);

uint8_t page = JWPLC_Display.userPage();
```

Cuando un campo de una página no visible cambia, el valor queda cacheado. Al entrar posteriormente a esa página se dibuja el dato actualizado.

---

## Refresh y dirty regions

La HMI separa contenido estático de contenido dinámico.

El redibujado normal de un campo actualiza sólo su región de valor. Un `fillScreen()` completo se reserva para transiciones o invalidaciones donde realmente corresponde.

API:

```cpp
JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
JWPLC_Display.requestUserRefresh();

JWPLC_Display.invalidateField(FIELD_TEMP);
JWPLC_Display.invalidateAllFields();
```

En una pasada de actualización, los campos dirty visibles se dibujan dentro de una sola ventana de adquisición del bus TFT.

Esto reduce flicker y tiempo de retención del SPI compartido.

---

## Vistas cacheadas del runtime

Alpha8 expone fachadas ligeras que no realizan transacciones físicas adicionales:

```cpp
JWPLC_IO.inputs();
JWPLC_IO.outputs();
JWPLC_IO.input(0);
JWPLC_IO.output(0);
JWPLC_IO.ready();
JWPLC_IO.lastScanMs();
```

En JWPLC Basic, `outputs()` representa el banco lógico `Q0.0..Q0.7`.

RTC cacheado:

```cpp
JWPLC_Time.present();
JWPLC_Time.valid();
JWPLC_Time.lostPower();
JWPLC_Time.hour();
JWPLC_Time.minute();
JWPLC_Time.second();
JWPLC_Time.day();
JWPLC_Time.month();
JWPLC_Time.year();
JWPLC_Time.dayOfWeek();
JWPLC_Time.lastUpdateMs();
```

Estas APIs son especialmente útiles dentro de una HMI porque evitan duplicar lecturas I2C/SPI.

---

## Acceso directo a la TFT

Para interfaces que necesiten dibujo Adafruit manual:

```cpp
auto &tft = JWPLC_Display.tft();
```

`display()` existe como alias compatible.

No se recomienda guardar la referencia globalmente antes de que el Display esté listo.

---

## Callbacks USER legacy/manuales

Los callbacks manuales siguen disponibles:

```cpp
extern "C" void jwplcUserDisplayEnterCallback()
{
}

extern "C" void jwplcUserDisplayRefreshCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
}

extern "C" void jwplcUserDisplayExitCallback()
{
}
```

La HMI Alpha8 añade una capa superior para la mayoría de interfaces habituales; los callbacks continúan siendo válidos cuando se necesita control gráfico total.

---

## Indicadores laterales

### RUN

```cpp
JWPLC_Display.setRunLed(true);
bool run = JWPLC_Display.runLed();
```

### ERR

`ERR` pertenece a la aplicación:

```cpp
JWPLC_Display.setErrCode("A01");
const char *code = JWPLC_Display.errCode();
```

Formato:

- 1 a 4 caracteres `A-Z` o `0-9`;
- minúsculas se normalizan a mayúsculas;
- `nullptr`, vacío o sólo ceros significan sin error;
- una entrada inválida conserva el estado previo.

Compatibilidad legacy:

```cpp
JWPLC_Display.setErrLed(true);
JWPLC_Display.setErrLed(false);
```

### BUS

```cpp
JWPLC_Display.setBusLedAuto(true);
```

Códigos actuales:

```text
--- DIS INI SER SID MAP TMO CRC EXC RSP OVF FUN
```

### ETH

```cpp
JWPLC_Display.setEthLedAuto(true);
```

Códigos actuales:

```text
--- DIS INI PHY LNK DHC HW IP SPI
```

---

## Coexistencia SPI

La TFT comparte SPI con:

- W5500;
- FRAM;
- microSD.

El runtime utiliza el mutex SPI global del ecosistema JWPLC.

Patrón recomendado:

1. usar snapshots/cache cuando sea posible;
2. evitar operaciones SPI largas dentro del dibujo;
3. actualizar únicamente regiones dirty;
4. dejar que `JWPLC_Display` gestione la adquisición del bus para su HMI.

---

## Precompilación y lazy-link Alpha8

`JWPLC_Display` continúa usando:

```text
precompiled=full
```

Alpha8 separa el motor HMI del Display base a nivel de link mediante hooks internos.

Resultado esperado:

```text
Sketch que no usa HMI -> no extrae el motor JWPLC_UI del archive
Sketch que usa HMI    -> extrae JWPLC_UI / JWPLC_UI_API cuando corresponde
```

El gate de link de Alpha8 confirmó que un `01_empty` no enlaza los objetos HMI y redujo el tamaño APP en 3456 bytes respecto al estado Alpha8 previo al lazy-link.

La API pública de HMI se mantiene igual para el sketch.

---

## Validación física Alpha8

Se validó en JWPLC Basic:

- IDLE continuo durante más de 180 s sin autowake inesperado;
- `unexpectedUser=0`;
- RTC avanzando durante el soak;
- los seis botones;
- `OK` para entrada USER explícita;
- `LEFT/RIGHT` para navegación entre páginas;
- `UP/DOWN` para actualización de barra;
- `ESC` observado tanto por el Display como por el sketch;
- botones recibidos por la aplicación estando en IDLE sin despertar USER;
- contador, texto, booleano, valor y barra;
- refresh sin flicker observado;
- salida USER -> IDLE sin congelamiento;
- regresión de pulsación sostenida sin bloqueo.

El incidente histórico observado durante un taller con versiones/entornos anteriores se considera resuelto operacionalmente para avanzar; su causa exacta no se reproduce de forma concluyente y no se atribuye a Alpha8 como una única causa demostrada.

---

## Ejemplo de gate Alpha8

El package incluye:

```text
examples/Display_Alpha8_HMI_Gate/Display_Alpha8_HMI_Gate.ino
```

Este ejemplo sirve como referencia de:

- definición de campos;
- páginas;
- `JWPLC_IO`;
- `JWPLC_Time`;
- botonera independiente del Display;
- dirty refresh;
- transición IDLE/USER.

---

## Estado

```text
JWPLC ESP32 2.1.0-alpha.8
JWPLC_Display metadata: 1.0.1
```

Alpha8 amplía la HMI y endurece la interacción Display/botonera sin retirar periféricos del autoload normal y sin cambiar los nombres de la API pública ya probada.

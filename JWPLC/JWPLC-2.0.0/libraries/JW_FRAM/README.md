# JW_FRAM

Librería SPI FRAM enfocada inicialmente en productos basados en **ESP32**, especialmente los desarrollos de **JW Control**.

`JW_FRAM` ofrece dos niveles de uso:

- **API de bajo nivel** para acceso directo a memorias FRAM SPI.
- **API de alto nivel** con una lógica de uso estilo EEPROM, pensada para guardar variables, textos, estructuras y configuraciones completas de forma cómoda.

## Características principales

- Soporte para memorias **FRAM SPI**.
- Uso de **Adafruit BusIO** como dependencia.
- Frecuencia SPI por defecto de **1 MHz**.
- Posibilidad de configurar la frecuencia SPI en el constructor por hardware.
- Métodos de bajo nivel: `read`, `write`, `read8`, `write8`, `writeEnable`, `getDeviceID`, `getStatusRegister`, `setStatusRegister`.
- Métodos de alto nivel: `get`, `put`, `update`.
- Soporte para:
  - tipos numéricos (`int`, `uint32_t`, `float`, `double`, etc.)
  - `bool`
  - `String`
  - C strings (`char[]` terminados en `\0`)
  - `struct` trivially copyable
- Bloques de configuración validados mediante:
  - `magic`
  - `version`
  - `length`
  - `checksum`
- Validación de rangos de memoria con `isAddressValid()`.
- Depuración opcional usando cualquier objeto `Stream`.
- Posibilidad de forzar el tamaño de FRAM al iniciar, útil para modelos no listados en la tabla interna.

## Dependencia

Esta librería depende de **Adafruit BusIO**.

En `library.properties` ya se declara:

```ini
depends=Adafruit BusIO
```

## Instalación

1. Instala **Adafruit BusIO** desde el gestor de librerías de Arduino.
2. Copia esta librería en tu carpeta de librerías o instálala como archivo ZIP.
3. Incluye la librería en tu sketch:

```cpp
#include <JW_FRAM.h>
```

## Uso básico

```cpp
#include <JW_FRAM.h>

JW_FRAM fram(5);  // Pin CS

void setup() {
  Serial.begin(115200);

  fram.enableDebug(Serial);

  if (!fram.begin(8 * 1024)) {
    Serial.println("Fallo al inicializar la FRAM");
    while (1) {}
  }

  uint32_t contador = 1234;
  fram.put(0, contador);

  uint32_t valorLeido = 0;
  fram.get(0, valorLeido);

  Serial.print("Contador: ");
  Serial.println(valorLeido);
}

void loop() {}
```

## API de bajo nivel

La API de bajo nivel está pensada para tener control directo sobre la memoria FRAM.

Métodos disponibles:

- `writeEnable(bool enable)`
- `write8(uint32_t addr, uint8_t value)`
- `read8(uint32_t addr)`
- `write(uint32_t addr, const uint8_t* values, size_t count)`
- `read(uint32_t addr, uint8_t* values, size_t count)`
- `getDeviceID(uint8_t* manufacturerID, uint16_t* productID)`
- `getStatusRegister()`
- `setStatusRegister(uint8_t value)`
- `setAddressSize(uint8_t nAddressSize)`

### Importante

En esta capa, la escritura es manual. Eso significa que debes hacer:

```cpp
fram.writeEnable(true);
fram.write8(0, 0x55);
fram.writeEnable(false);
```

## API de alto nivel

La API de alto nivel maneja automáticamente `writeEnable(true/false)` cuando corresponde.

### Métodos principales

```cpp
fram.get(addr, variable);
fram.put(addr, variable);
fram.update(addr, variable);
```

### `get()`
Lee un valor desde la FRAM y lo copia a una variable.

### `put()`
Escribe un valor completo en la FRAM.

### `update()`
Primero lee el valor actual y solo escribe si detecta cambios.

## Tipos compatibles en `get`, `put`, `update`, `writeBlock`, `readBlock`

Estos métodos están pensados para tipos **trivially copyable**, como por ejemplo:

- `bool`
- enteros
- `float`
- `double`
- arreglos fijos
- estructuras simples

### Ejemplo correcto

```cpp
struct PlcConfig {
  uint8_t mode;
  bool enabled;
  float kp;
  float ki;
  float kd;
};
```

### Ejemplo no recomendado

```cpp
struct BadConfig {
  String ssid;
  String pass;
};
```

Si necesitas textos dentro de estructuras persistentes, es mejor usar arreglos fijos como `char nombre[32];`.

## Manejo de Strings

### 1. `String` con longitud prefijada

Formato almacenado:

```text
[len][data...]
```

Reglas:

- máximo **127 caracteres**
- modo estricto
- si el texto supera 127 caracteres, la función devuelve `false`
- no se recorta el texto automáticamente

Métodos:

```cpp
bool writeString(uint32_t addr, const String& value);
bool readString(uint32_t addr, String& value, uint8_t maxLen = 127);
```

### 2. C string terminada en `\0`

Formato almacenado:

```text
[data...]['\0']
```

Reglas:

- máximo **127 caracteres útiles**
- si el texto excede ese límite, la escritura falla
- la lectura se detiene al encontrar `\0`

Métodos:

```cpp
bool writeCString(uint32_t addr, const char* str, uint8_t maxLen = 127);
bool readCString(uint32_t addr, char* buffer, size_t bufferSize, uint8_t maxLen = 127);
```

## Bloques de configuración con validación

Los métodos `writeBlock()` y `readBlock()` permiten guardar una estructura acompañada de un encabezado de validación.

El encabezado contiene:

- `magic`
- `version`
- `reserved`
- `length`
- `checksum`

Esto sirve para detectar:

- memoria no inicializada
- estructuras incompatibles entre versiones
- corrupción de datos
- tamaños de payload incorrectos

### Ejemplo

```cpp
struct SystemConfig {
  uint8_t configVersion;
  bool firstBootDone;
  float kp;
  float ki;
  float kd;
  uint32_t totalStarts;
  char machineName[24];
};

SystemConfig cfg = {1, true, 2.0f, 0.5f, 0.1f, 42, "JWPLC"};

fram.writeBlock(128, cfg, 1);

SystemConfig restored;
if (fram.readBlock(128, restored, 1)) {
  // configuración válida recuperada
}
```

## Debug por Stream

Puedes habilitar depuración usando cualquier objeto compatible con `Stream`.

```cpp
fram.enableDebug(Serial);
fram.disableDebug();
```

Eso evita amarrar la librería exclusivamente a `Serial`.

## Dispositivos no soportados en la tabla interna

Si la librería no reconoce el `Device ID`, puedes forzar manualmente el tamaño al iniciar:

```cpp
fram.begin(8 * 1024);
```

Eso permite trabajar con memorias FRAM compatibles aunque no estén todavía incluidas en la tabla interna.

## Estructura recomendada de la librería

La librería ya fue preparada con archivos compatibles con el ecosistema Arduino:

- `src/`
- `examples/`
- `library.properties`
- `README.md`
- `CHANGELOG.md`
- `keywords.txt`
- `LICENCE`

## Ideas futuras

Para versiones posteriores podrían añadirse:

- CRC16 en lugar de checksum simple
- funciones `fill()` o `clear()`
- más modelos en la tabla de dispositivos
- más ejemplos de uso orientados a productos JWPLC

## Nota de licencia

La base original de Adafruit usa licencia BSD. Si esta librería deriva de ese trabajo, se debe conservar la atribución correspondiente y mantener el texto de licencia apropiado. El repositorio de Adafruit FRAM SPI publica su licencia BSD con cláusula de atribución y limitación de responsabilidad. citeturn948721view0

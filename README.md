# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, optimizado para el ecosistema **JWPLC Basic**.

El objetivo del package es ofrecer una experiencia industrial más directa que el core ESP32 genérico: menos variantes innecesarias, APIs de alto nivel y periféricos integrados al runtime del JWPLC.

---

## Enfoque del package

A partir de la etapa `alpha27`, el package vuelve a su enfoque compacto:

- **ESP32 Base Board** para proyectos ESP32 genéricos.
- **JWPLC Basic** para el hardware completo con periféricos integrados.
- **JWPLC Basic Core** para validación/core esencial sin periféricos opcionales.

No se mantiene soporte visible para ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2 u otras familias dentro del package JWPLC mientras no sean requeridas por un producto real. Esto reduce peso, simplifica mantenimiento y evita confusión en Arduino IDE.

---

## Instalación

En Arduino IDE, ingresa este enlace en **Archivo > Preferencias > Gestor de URLs adicionales de tarjetas**:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Luego abre **Herramientas > Placa > Gestor de tarjetas**, busca `JW Control ESP32 Boards` e instala la versión disponible.

---

## Placas objetivo

| Placa | Uso recomendado | Periféricos esperados |
|---|---|---|
| ESP32 Base Board | Desarrollo ESP32 genérico dentro del package JWPLC | Sin periféricos JWPLC automáticos. |
| JWPLC Basic | Hardware completo JWPLC Basic | Display, botonera, RTC, FRAM, microSD y Ethernet. |
| JWPLC Basic Core | Validación del core y pruebas sin periféricos opcionales | Display/botonera según build; SD/FRAM/Ethernet pueden reportar disabled. |

---

## Compatibilidad de periféricos

| Periférico / API | ESP32 Base Board | JWPLC Basic | JWPLC Basic Core |
|---|---:|---:|---:|
| `JWPLC_Display` | No automático | Sí | Sí, según configuración de core |
| `JWPLC_Buttons` | No automático | Sí | Sí, según configuración de core |
| `JWPLC_RTC` | No automático | Sí | Según configuración |
| `JWPLC_FRAM` | No automático | Sí | Disabled / size 0 |
| `JWPLC_SD` | No automático | Sí | Disabled |
| `JWPLC_Ethernet` | No automático | Sí | Disabled |
| TCA / IO industrial | No automático | Sí | Según variante/configuración |

> Nota: en **JWPLC Basic Core**, mensajes como `Ethernet disabled`, `SD disabled` o `FRAM size: 0` son esperados si esa variante fue compilada sin esos periféricos.

---

## APIs globales del ecosistema JWPLC

En JWPLC Basic, el package expone objetos globales para trabajar con sintaxis directa:

```cpp
JWPLC_Display
JWPLC_Ethernet
JWPLC_SD
JWPLC_FRAM
JWPLC_RTC
JWPLC_Buttons
```

La idea es que el usuario final no tenga que repetir inicializaciones internas ni recordar pines, buses SPI o detalles de hardware.

Ejemplo simple:

```cpp
void setup()
{
    Serial.begin(115200);
    delay(1200);
}

void loop()
{
    Serial.print("ETH: ");
    Serial.print(JWPLC_Ethernet.statusString());
    Serial.print(" | IP: ");
    Serial.println(JWPLC_Ethernet.localIP());

    delay(1000);
}
```

---

## Display integrado

`JWPLC_Display` se inicializa automáticamente en placas compatibles y muestra una pantalla `IDLE` con estado general del equipo.

La API recomendada usa estilo objeto con punto:

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
JWPLC_Display.setIdleTimeoutMs(8000);
JWPLC_Display.setRunLed(true);
```

Modos de retorno desde pantalla `USER` a `IDLE`:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
```

Para usar funciones de configuración, estado o LEDs del display no hace falta incluir librerías manualmente.

Para dibujar directamente sobre la TFT con métodos de Adafruit ST7789, sí se debe incluir:

```cpp
#include <JWPLC_Display.h>
```

Esto es necesario cuando se usa:

```cpp
auto &tft = JWPLC_Display.tft();
```

---

## Ethernet integrado

Desde `2.0.0-alpha.26`, Ethernet W5500 queda integrado al runtime del JWPLC Basic.

En uso normal no es necesario llamar:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

El runtime se encarga de:

- Inicializar Ethernet automáticamente.
- Evitar bloqueos largos cuando no hay RJ45 conectado.
- Reintentar cuando se conecta RJ45 después del arranque.
- Mantener DHCP.
- Proteger el bus SPI compartido.
- Actualizar automáticamente el indicador `ETH` en la pantalla IDLE.

### LED ETH en IDLE

| Condición | LED ETH |
|---|---|
| Ethernet disabled / Basic Core | Apagado |
| RJ45 desconectado / Link OFF | Apagado |
| Ethernet OK | Verde |
| Falla real de Ethernet | Rojo |

---

## Reglas de coexistencia SPI

El JWPLC Basic comparte SPI entre:

- TFT ST7789.
- Ethernet W5500.
- FRAM.
- microSD.

Regla recomendada para sketches avanzados:

1. Consultar periféricos SPI desde `loop()`.
2. Guardar resultados en variables simples.
3. En callbacks gráficos del display, dibujar solo variables cacheadas.

Evita consultar `JWPLC_Ethernet`, `JWPLC_SD` o `JWPLC_FRAM` directamente dentro de callbacks de dibujo.

---

## Ejemplos recomendados para validar instalación

### Prueba mínima

1. Compilar un sketch vacío.
2. Compilar un sketch con `Serial.begin(115200)`.
3. Validar que Arduino IDE use la placa seleccionada correctamente.

### Display

- `Display_DotAPI_Minimal`
- `Display_Idle_Return_Modes`
- `Display_UserUI_Callbacks`
- `Display_Efficient_Redraw`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Auto_StaticIP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

### microSD / FRAM / RTC

Usar los ejemplos internos de cada librería JW cuando estén disponibles dentro del package o desde sus repositorios de distribución.

---

## Checklist rápido antes de una beta

- Instalar el package desde `package_jwplc_index.json`.
- Seleccionar **ESP32 Base Board** y compilar sketch vacío.
- Seleccionar **JWPLC Basic** y compilar sketch vacío.
- Seleccionar **JWPLC Basic Core** y compilar sketch vacío.
- Probar Display IDLE.
- Probar retorno `USER -> IDLE` por timeout.
- Probar retorno `USER -> IDLE` por ESC.
- Probar Ethernet con RJ45 conectado.
- Probar Ethernet sin RJ45.
- Probar conexión de RJ45 después del arranque.
- Probar microSD insertada/retirada.
- Probar FRAM con contador de arranque.
- Probar coexistencia Ethernet + SD + FRAM + Display.

---

## Estructura principal

```txt
JWPLC/
  package_jwplc_index.json
  JWPLC-2.0.0/
    boards.txt
    platform.txt
    cores/
    variants/
    libraries/
```

---

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.

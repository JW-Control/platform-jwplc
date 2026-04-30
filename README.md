# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, optimizado para el ecosistema **JWPLC Basic**.

Este package reduce el peso del core ESP32 original y añade integración directa con periféricos industriales del JWPLC Basic, permitiendo trabajar con una experiencia más limpia desde Arduino IDE.

---

## Características principales

- Soporte enfocado en ESP32 base.
- Package reducido frente al core ESP32 oficial.
- Compatible con Arduino IDE.
- Preparado para desarrollo industrial con JWPLC Basic.
- Incluye variantes JWPLC:
  - ESP32 Base Board.
  - JWPLC Basic.
  - JWPLC Basic Core.
- Integra periféricos globales del ecosistema JWPLC.
- Expone APIs de alto nivel para evitar configuración repetitiva en sketches.

---

## Instalación

En Arduino IDE, ingresa este enlace en **Gestor de URLs adicionales de tarjetas**:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/package_jwplc_index.json
```

Luego instala el package desde el Gestor de Tarjetas.

---

## Versiones disponibles

La rama `main` contiene las versiones publicadas del package.

Las versiones `alpha` son versiones de prueba para validación progresiva de periféricos y APIs internas antes de la beta/versión estable.

---

## Estado actual: 2.0.0-alpha.26

`2.0.0-alpha.26` integra Ethernet automático para JWPLC Basic y consolida la coexistencia SPI entre periféricos.

### Periféricos integrados

| Periférico | Estado | Notas |
|---|---:|---|
| RTC | Operativo | Manejo automático del reloj del sistema. |
| FRAM | Operativo | Acceso global mediante `JWPLC_FRAM`. |
| microSD | Operativo | Acceso global protegido mediante `JWPLC_SD`. |
| TFT Display | Operativo | Pantalla IDLE/USER con API `JWPLC_Display`. |
| Botonera | Operativa | Lectura global integrada al Display. |
| Ethernet W5500 | Operativo en alpha26 | Runtime automático mediante `JWPLC_Ethernet`. |

---

## Ethernet en alpha26

Ethernet queda integrado al runtime del JWPLC.

En uso normal no es necesario llamar:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.maintain();
```

El sistema JWPLC se encarga de:

- Inicializar Ethernet automáticamente.
- Evitar bloqueo largo si no hay RJ45 conectado.
- Reintentar automáticamente cuando se conecta RJ45 después del arranque.
- Mantener DHCP.
- Reportar estado mediante `JWPLC_Ethernet`.
- Actualizar automáticamente el LED `ETH` en la pantalla IDLE.

### LED ETH en IDLE

| Condición | LED ETH |
|---|---|
| Ethernet disabled / Basic Core | Apagado |
| RJ45 desconectado / Link OFF | Apagado |
| Ethernet OK | Verde |
| Falla real con Ethernet | Rojo |

---

## Display en alpha26

La API recomendada para configuración de pantalla es:

```cpp
JWPLC_Display.setIdleReturnMode(IDLE_RETURN_TIMEOUT);
JWPLC_Display.setIdleTimeoutMs(8000);
JWPLC_Display.setRunLed(true);
```

Modos de retorno desde USER a IDLE:

```cpp
IDLE_RETURN_TIMEOUT
IDLE_RETURN_ESC_ONLY
IDLE_RETURN_DISABLED
```

Para uso normal de LEDs, modos y estado del Display, no se requiere incluir librerías manualmente.

Para dibujar directamente con la TFT usando métodos de Adafruit ST7789, incluir:

```cpp
#include <JWPLC_Display.h>
```

---

## APIs globales disponibles

En JWPLC Basic, las APIs globales principales son:

```cpp
JWPLC_Display
JWPLC_Ethernet
JWPLC_SD
JWPLC_FRAM
JWPLC_RTC
JWPLC_Buttons
```

El objetivo es que los usuarios puedan trabajar con sintaxis directa y clara, sin repetir inicializaciones internas de hardware.

---

## Reglas de coexistencia SPI

El JWPLC Basic comparte SPI entre:

- TFT.
- W5500 Ethernet.
- FRAM.
- microSD.

Regla importante para sketches avanzados:

- Consultar periféricos SPI desde `loop()`.
- Guardar resultados en variables.
- En callbacks gráficos del Display, solo dibujar datos cacheados.

Evitar llamadas directas a Ethernet/SD/FRAM dentro de callbacks gráficos.

---

## Ejemplos destacados

### Display

- `Display_Idle_Return_Modes`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Auto_StaticIP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

---

## Repositorio

```txt
https://github.com/JW-Control/platform-jwplc
```

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.

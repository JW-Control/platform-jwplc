# JWPLC Platform for Arduino IDE

<!-- JWPLC_RELEASE_VERSION: 2.1.0-alpha.15 -->

Package oficial de **JW Control** para programar el **JWPLC Basic** desde
Arduino IDE y Arduino CLI.

Al seleccionar la placa, el JWPLC arranca con sus periféricos listos: no hace
falta configurar pines, inicializar la pantalla ni levantar la red desde el
sketch. Tu programa se ocupa solo de la lógica de la máquina.

---

## Descargas · v2.1.0-alpha.15: OpenPLC + HMI

Programa el JWPLC Basic en **Ladder** desde OpenPLC Editor y diséñale la pantalla con el **HMI Designer**, que
viene incluido: lo abres desde el proyecto y, en el mismo editor, enlazas cada objeto de la pantalla a una variable
del programa, sin escribir código.

| Descarga | Archivo | Para qué |
|---|---|---|
| **OpenPLC Editor – JWPLC Edition 4.2.8-jwplc.7** | [⬇ Instalador Windows x64](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.7/OpenPLC.Editor.-.JWPLC.Edition_4.2.8-jwplc.7.exe) | Editor OpenPLC con el HMI Designer adentro |
| **Paquete de placas JWPLC Basic (VPP) 2.1.0-alpha.15** | [⬇ VPP firmado](https://github.com/JW-Control/openplc-editor/releases/download/jwplc-editor-v4.2.8-jwplc.7/jwplc-basic-openplc-2.1.0-alpha.15.jwcontrol-signed.vpp) | Se importa en OpenPLC Editor: placa JWPLC BASIC y su Pantalla HMI |
| **JWPLC HMI Designer 2.1.0-alpha.15** | [⬇ Instalador](https://github.com/JW-Control/platform-jwplc/releases/download/hmi-designer-v2.1.0-alpha.15/JWPLC-HMI-Designer-Setup-2.1.0-alpha.15-x64.exe) · [⬇ Portable](https://github.com/JW-Control/platform-jwplc/releases/download/hmi-designer-v2.1.0-alpha.15/JWPLC-HMI-Designer-Portable-2.1.0-alpha.15-x64.exe) | Solo para usarlo con sketches de Arduino IDE (en OpenPLC ya viene incluido) |

Requisitos: Windows 10/11 de 64 bits y el package **JWPLC 2.1.0-alpha.15** (o 2.1.0-alpha.14) instalado (ver
[Instalación](#instalación)). Los instaladores no tienen firma de código: si Windows SmartScreen avisa, pulsa
**Más información → Ejecutar de todas formas**. Pasos completos y sumas SHA-256 en las notas de cada descarga:
[OpenPLC Editor](https://github.com/JW-Control/openplc-editor/releases/tag/jwplc-editor-v4.2.8-jwplc.7) ·
[HMI Designer](https://github.com/JW-Control/platform-jwplc/releases/tag/hmi-designer-v2.1.0-alpha.15).

> Probado en un JWPLC Basic: Ladder con variables enlazadas a la pantalla, compilar, subir y ver la TFT.

---

## Versiones

| Canal | Versión | Para quién |
|---|---|---|
| Estable | `v2.0.0` | Proyectos en producción |
| Desarrollo (PreRelease) | `v2.1.0-alpha.15` | Probar las novedades de la línea 2.1 |

La versión de desarrollo se publica como PreRelease en GitHub y se instala
desde un índice propio, separado del estable.

---

## Instalación

Recomendado: **Arduino IDE 2.3.10 o superior**. Su compilador interno recompila
un proyecto ya compilado en la mitad de tiempo que las versiones anteriores
(9 s frente a 18 s en JWPLC Basic).

1. En Arduino IDE abre **Archivo → Preferencias** y pega en
   **Gestor de URLs adicionales de tarjetas** uno de estos índices:

   Desarrollo:

   ```text
   https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
   ```

   Estable:

   ```text
   https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
   ```

2. Abre **Herramientas → Placa → Gestor de tarjetas**, busca
   **JW Control ESP32 Boards** e instala la versión que quieras.
3. Elige la placa en **Herramientas → Placa → JW Control ESP32 Boards**.

Con Arduino CLI:

```bash
arduino-cli config add board_manager.additional_urls \
  https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
arduino-cli core update-index
arduino-cli core install jwplc:esp32@2.1.0-alpha.15
```

No instales a mano librerías con los mismos nombres que trae el package: el
package ya incluye las versiones probadas con el JWPLC.

---

## Placas

| Placa | FQBN | Qué incluye |
|---|---|---|
| **JWPLC Basic** | `jwplc:esp32:jwplcbasic` | El equipo completo, con Ethernet, FRAM y microSD |
| **JWPLC Basic Core** | `jwplc:esp32:jwplcbasiccore` | La misma base sin Ethernet, FRAM ni microSD |
| ESP32 Board | `jwplc:esp32:esp32` | ESP32 genérico, sin periféricos JWPLC |

---

## Qué trae el JWPLC Basic

- **8 entradas digitales industriales** (`I0_0` … `I0_7`).
- **8 salidas** (`Q0_0` … `Q0_7`).
- **Pantalla TFT a color** con modo de reposo (IDLE), páginas HMI e
  indicadores de estado.
- **Botonera frontal** de seis teclas.
- **Buzzer** para pitidos y melodías (`JWPLC_Buzzer`).
- **Reloj de tiempo real (RTC)** para fecha y hora.
- **FRAM** para guardar datos que deben sobrevivir a un corte de energía.
- **microSD** para registros y archivos.
- **Ethernet** con DHCP o IP fija, TCP y UDP.
- **RS-485** con **Modbus RTU** (maestro o esclavo).
- **Modbus TCP** (servidor o cliente).

Todo se usa con objetos globales ya creados, como `JWPLC_Ethernet`,
`JWPLC_Buttons`, `JWPLC_RTC` o `JWPLC_ModbusRTU`, sin `#include`. La única
excepción es Modbus TCP, que es opcional y se agrega con
`#include <JWPLC_ModbusTCP.h>`.

---

## Novedades de v2.1.0-alpha.15

Alpha 15 une el JWPLC Basic con **OpenPLC Editor** y el **HMI Designer**: programas en Ladder y le diseñas la
pantalla sin escribir código. Las descargas están [al inicio](#descargas--v210-alpha15-openplc--hmi).

- **HMI Designer dentro de OpenPLC.** En el árbol del proyecto, bajo Device, la entrada **HMI Designer** abre el
  Designer completo en su ventana para dibujar la pantalla. De vuelta en OpenPLC, la pestaña «HMI Designer» muestra
  la vista previa, las páginas y el árbol de objetos para **enlazar cada objeto a una variable del programa**.
- **Variables del Ladder sin dirección.** Las variables que declaras en el `main` (BOOL, enteros hasta 32 bits y
  REAL) se enlazan aunque no tengan `%IX`/`%QX`; la pantalla las lee y las escribe.
- **Todo se guarda con el proyecto.** La pantalla vive en la carpeta `hmi/` del proyecto; Ctrl+S, «Guardar
  proyecto» y el guardado antes de compilar la incluyen.
- **Variables en vivo.** Lo que declares o cambies en el programa aparece en «Variable PLC» sin reabrir nada.
- **Instaladores listos para descargar:** OpenPLC Editor – JWPLC Edition (con el Designer adentro), el paquete de
  placas firmado (VPP 2.1.0-alpha.15) y el HMI Designer para Arduino IDE.
- **Curso JWPLC Basic** en *Archivo → Ejemplos → JWPLC_GlobalPeripherals → Curso_JWPLC_Basic*: unidades 1
  («Primeros pasos») y 2 («Tiempo y lógica»), 11 lecciones de hola mundo a máquina de estados.
- **Plantilla estándar de proyecto** en *Ejemplos → JWPLC_GlobalPeripherals → 00.Plantilla_Proyecto_JWPLC*:
  pestañas fijas (configuración, tags, entradas, lógica, alarmas, salidas, HMI, comunicaciones, persistencia,
  diagnóstico) y la ayuda dentro del proyecto para trabajar sin internet.

Las librerías del package no cambian respecto de 2.1.0-alpha.14: sólo se agregan el curso y la plantilla como
ejemplos.

## Novedades de v2.1.0-alpha.14

Alpha 14 trae la nueva generación del **JWPLC HMI Designer** y lo que la
pantalla necesitaba para usarse como HMI de máquina.

- **Menús en pantalla.** Un MENU con submenús permite ver y ajustar consignas,
  tiempos y marchas desde la botonera, sin escribir código de navegación.
- **Valores con formato industrial.** Separador de miles, duraciones
  (`05:03`, `1h05m`), fecha y hora del reloj de la placa, pilotos ON/OFF y
  colores por estado.
- **Iconos con estado.** Bombas, válvulas, motores o alarmas que cambian
  de imagen según una variable, elegidos de una biblioteca del Designer.
- **Tildes, ñ y °** en los textos de la pantalla.
- **Pestañas de páginas** más claras: se distingue la página elegida de la
  página en la que se está trabajando, con un color por página.
- **Buzzer** integrado con pitidos y melodías que no detienen el programa.
- **Plantillas de HMI** en *Archivo → Ejemplos → JWPLC_Display* (caldera, botonera
  interactiva y planta con iconos), listas para subir y modificar en el Designer.
- **HMI Designer renovado:** inspector gráfico, herramientas de dibujo,
  trabajo directo con el sketch del Arduino IDE y vista en vivo que se
  instala en el equipo con un clic.
- **Dibujo directo** con elipses, triángulos, fuentes grandes e imágenes
  (`JWPLC_TFT` 0.3.0).

Además, las librerías y los ejemplos usan **pines con nombre** (`JWPLC_FRAM_CS`,
`JWPLC_SD_CS`, `JWPLC_BTN_*`…) en lugar de números sueltos, y `JWPLC_RS485` /
`JWPLC_ModbusRTU` se apoyan ahora en las librerías `JW_RS485` y `JW_ModbusRTU`,
con la misma API.

**Probado en banco:** menús, formatos, reloj, tildes, buzzer, pestañas y
tecla de despertar en un JWPLC Basic; Modbus RTU entre dos equipos; los 78
ejemplos del package compilan en JWPLC Basic y Basic Core.

---

## Primeros pasos

### Entradas y salidas

```cpp
void setup()
{
    pinMode(Q0_0, OUTPUT);
}

void loop()
{
    // La salida Q0_0 copia el estado de la entrada I0_0.
    digitalWrite(Q0_0, digitalRead(I0_0));
}
```

### Botonera

```cpp
bool encendida = false;

void setup()
{
    pinMode(Q0_0, OUTPUT);
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
    {
        encendida = !encendida;
        digitalWrite(Q0_0, encendida ? HIGH : LOW);
    }
}
```

### Ethernet con DHCP

```cpp
void setup()
{
    Serial.begin(115200);
    JWPLC_Ethernet.useDHCP();
}

void loop()
{
    if (JWPLC_Ethernet.isReady())
    {
        Serial.println(JWPLC_Ethernet.localIP());
        delay(5000);
    }
}
```

### Servidor Modbus TCP

```cpp
#include <JWPLC_ModbusTCP.h>

uint16_t registros[16] = {};

void setup()
{
    JWPLC_ModbusTCP.setHoldingRegisters(registros, 16);
    JWPLC_ModbusTCP.beginServer(1, 502);
}

void loop()
{
    registros[0] = digitalRead(I0_0);
}
```

### Esclavo Modbus RTU por RS-485

```cpp
uint16_t registros[8] = {};

void setup()
{
    JWPLC_ModbusRTU.setHoldingRegisters(registros, 8);
    JWPLC_ModbusRTU.begin(2, 115200, SERIAL_8N1);
}

void loop()
{
    JWPLC_ModbusRTU.task();
}
```

Cada librería trae más ejemplos en **Archivo → Ejemplos**.

---

## Documentación

Cada librería tiene una guía que empieza por lo básico y deja lo avanzado
para el final.

| Tema | Guía |
|---|---|
| Entradas, salidas, botonera, RTC, FRAM y microSD | [JWPLC_GlobalPeripherals](JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/README.md) |
| Pantalla y HMI | [JWPLC_Display](JWPLC/2.1.0/libraries/JWPLC_Display/README.md) |
| Dibujo directo en la TFT | [JWPLC_TFT](JWPLC/2.1.0/libraries/JWPLC_TFT/README.md) |
| Ethernet, TCP y UDP | [JWPLC_Ethernet](JWPLC/2.1.0/libraries/JWPLC_Ethernet/README.md) |
| Modbus TCP | [JWPLC_ModbusTCP](JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/README.md) |
| RS-485 | [JWPLC_RS485](JWPLC/2.1.0/libraries/JWPLC_RS485/README.md) |
| Modbus RTU | [JWPLC_ModbusRTU](JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/README.md) |

---

## JWPLC HMI Designer

Aplicación para Windows que permite diseñar las pantallas del JWPLC de forma
visual: textos, valores, indicadores, barras, menús, figuras e imágenes, en una
o varias páginas. Escribe en la carpeta del sketch el archivo
`JWPLC_HMI_Generated.h` y una guía para usarlo, se abre desde el Arduino IDE y
ofrece vista previa en vivo conectada al equipo. Desde 2.1.0-alpha.15 también
viene dentro de OpenPLC Editor – JWPLC Edition, enlazado a las variables del
programa.

Instalación y uso: [tools/jwplc-hmi-designer](tools/jwplc-hmi-designer/README.md).

---

## Historial de la línea 2.1

| Versión | Tema principal |
|---|---|
| [`v2.1.0-alpha.15`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.15) | OpenPLC Editor + HMI Designer: Ladder con pantalla enlazada a sus variables |
| [`v2.1.0-alpha.14`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.14) | HMI Designer renovado, menús en pantalla, formatos industriales y buzzer |
| [`v2.1.0-alpha.13`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.13) | Robustez de Ethernet, Modbus TCP y bus compartido; compilación más rápida |
| [`v2.1.0-alpha.12`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.12) | Modbus TCP, motor asíncrono de Modbus RTU y nuevo backend de pantalla |
| [`v2.1.0-alpha.11`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.11) | JWPLC HMI Designer |
| [`v2.1.0-alpha.10`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.10) | Corrección de librerías duplicadas y velocidad de compilación |
| [`v2.1.0-alpha.8`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.8) | HMI en Arduino, pantalla y botonera |
| [`v2.1.0-alpha.6`](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.6) | Ethernet sin bloqueos y diagnóstico integrado |

Las notas completas de cada versión están en su página de Release y en
`docs/v2.1.0-alpha.N/PRE_RELEASE.md`.

---

## Para desarrolladores del package

El estado técnico, la evidencia de pruebas y las decisiones de cada alpha
viven en `docs/`. La 2.1.0-alpha.15 (OpenPLC + HMI) se desarrolló como línea de
trabajo Alpha 14: empieza por
[ALPHA14_OPENPLC_STATUS.md](docs/v2.1.0-alpha.14/ALPHA14_OPENPLC_STATUS.md). La
2.1.0-alpha.14 se desarrolló como línea Alpha 15:
[ALPHA15_STATUS.md](docs/v2.1.0-alpha.15/ALPHA15_STATUS.md).

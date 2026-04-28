# JWPLC Platform for Arduino IDE

Package personalizado de **JW Control** para programar placas basadas en **ESP32** desde Arduino IDE, orientado a aplicaciones industriales y al ecosistema **JWPLC**.

Este repositorio contiene el core, variantes, librerías internas, herramientas y archivo de índice necesarios para instalar y usar placas JWPLC desde el Gestor de Placas de Arduino IDE.

---

## Estado actual

> Versión en desarrollo: `2.0.0-alpha.24`

La rama `main` contiene la base funcional de la serie `2.0.0`, con soporte validado para:

- `JWPLC Basic`
- `JWPLC Basic Core`
- `ESP32 Base Board`

La serie `2.0.0` todavía se considera **alpha / prerelease**. Ya es funcional para pruebas reales de hardware, pero aún se encuentra en evolución antes de declararse como versión estable.

---

## Instalación en Arduino IDE

En Arduino IDE, abre:

```text
Archivo > Preferencias > Gestor de URLs adicionales de tarjetas
```

Agrega la siguiente URL:

```text
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/package_jwplc_index.json
```

Luego ve a:

```text
Herramientas > Placa > Gestor de placas
```

Busca:

```text
JW Control ESP32 Boards
```

e instala la versión disponible.

---

## Placas soportadas

### JWPLC Basic

Perfil principal para el hardware JWPLC Basic.

Incluye soporte para:

- ESP32 base.
- Display TFT ST7789 con autoarranque.
- RTC.
- FRAM SPI.
- microSD.
- Botonera integrada.
- Base de integración para periféricos industriales.

### JWPLC Basic Core

Perfil reducido para pruebas, depuración o variantes de hardware sin todos los periféricos físicos.

Características:

- Mantiene compatibilidad con el ecosistema JWPLC.
- Deshabilita periféricos opcionales mediante flags de compilación.
- Permite compilar y ejecutar sketches sin requerir RTC, FRAM, microSD u otros periféricos físicos.

### ESP32 Base Board

Perfil genérico para ESP32 base.

Útil para:

- Pruebas del core.
- Compatibilidad general con ESP32.
- Desarrollo fuera del hardware JWPLC cuando no se requiere el runtime industrial completo.

---

## Filosofía del package

El objetivo del package JWPLC no es replicar todo el ecosistema oficial de Espressif, sino entregar una experiencia optimizada para productos industriales de JW Control.

Principios principales:

- Reducir peso del package.
- Evitar variantes no usadas.
- Facilitar instalación para clientes.
- Ocultar complejidad de periféricos externos.
- Exponer objetos globales listos para usar.
- Mantener compatibilidad con Arduino IDE.
- Preparar una base industrial reutilizable para futuras familias JWPLC.

---

## Periféricos globales

La serie `2.0.0` introduce periféricos globales administrados por el runtime del package.

Esto permite usar periféricos directamente desde el sketch sin instanciarlos manualmente.

### RTC

```cpp
JWPLC_RTC
```

### FRAM

```cpp
JWPLC_FRAM
```

Ejemplo:

```cpp
int32_t contador = 0;

JWPLC_FRAM.get(0, contador);
contador++;
JWPLC_FRAM.update(0, contador);
```

### microSD

```cpp
JWPLC_SD
```

Ejemplo:

```cpp
if (JWPLC_SD.isReady())
{
    auto file = JWPLC_SD.open("/log.txt", FILE_APPEND);

    if (file)
    {
        file.println("Hola desde JWPLC_SD");
        file.close();
    }
}
```

API disponible:

```cpp
JWPLC_SD.isEnabled();
JWPLC_SD.isCardPresent();
JWPLC_SD.isReady();
JWPLC_SD.lastErrorString();
JWPLC_SD.open("/archivo.txt", FILE_APPEND);
```

### Botonera

```cpp
JWPLC_Buttons
```

La botonera se integra con el display para permitir transición entre pantalla IDLE y pantalla USER.

---

## Display TFT

La librería interna del display fue renombrada de:

```text
JWPLC_Display_ST7789
```

a:

```text
JWPLC_Display
```

El display arranca automáticamente en placas JWPLC compatibles.

Actualmente la API pública mantiene el namespace:

```cpp
JWPLCDisplay::isReady();
JWPLCDisplay::enterUserUI();
JWPLCDisplay::setIdleReturnMode(...);
```

Una próxima alpha migrará esta API hacia un estilo de objeto global, más consistente con el resto del ecosistema:

```cpp
JWPLC_Display.isReady();
JWPLC_Display.enterUserUI();
JWPLC_Display.tft();
```

---

## Flags de hardware

El package usa flags de compilación para habilitar o deshabilitar periféricos según la placa seleccionada.

Ejemplos:

```cpp
JWPLC_HAS_RTC
JWPLC_HAS_FRAM
JWPLC_HAS_SD
JWPLC_HAS_ETHERNET
```

Esto permite que el mismo sketch pueda compilar en `JWPLC Basic` y `JWPLC Basic Core`, adaptándose automáticamente al hardware disponible.

---

## Versiones recientes

| Versión | Estado | Descripción |
|---|---|---|
| `2.0.0-alpha.24` | Prerelease | Integración funcional de microSD mediante `JWPLC_SD`, validada con FRAM, TFT y USER/IDLE. |
| `2.0.0-alpha.23` | Prerelease | Autoarranque del display, renombre a `JWPLC_Display` y mejoras de periféricos globales. |
| `2.0.0-alpha.22` | Prerelease | Consolidación de periféricos globales y validaciones de FRAM/RTC/display. |
| `1.0.5` | Estable anterior | Package liviano basado en ESP32, previo a la arquitectura 2.0.0. |

---

## Validaciones realizadas en `2.0.0-alpha.24`

### JWPLC Basic

Validado en hardware real:

- Compilación correcta.
- Carga correcta desde Arduino IDE 2.3.4.
- Display autoarranca.
- RTC operativo.
- FRAM SPI operativo.
- microSD detectada.
- Escritura y lectura en microSD.
- Convivencia microSD + FRAM + TFT.
- Entrada a pantalla USER mediante botonera.
- Retorno automático a pantalla IDLE.

### JWPLC Basic Core

Validado:

- Compilación correcta.
- Carga correcta.
- Display autoarranca.
- RTC/FRAM/SD reportan estado deshabilitado según perfil.
- El sistema no se bloquea aunque los periféricos opcionales estén desactivados.

---

## Observaciones conocidas

Durante pruebas dinámicas prolongadas de escritura en microSD junto con actividad de display se observaron algunos casos ocasionales de:

```text
Open failed
```

La operación se recupera automáticamente en el siguiente ciclo y no bloquea el sistema.

Pendiente para próximas versiones:

- Evaluar retry interno en `JW_SD.open()`.
- Mejorar coordinación entre display y periféricos SPI.
- Refactorizar la API del display hacia objeto global con estilo de punto.

---

## Recomendación sobre variantes ESP32

La línea actual del package está orientada principalmente a **ESP32 base**.

Para conservar la ventaja de tamaño y simplicidad, se recomienda mantener solamente:

- `ESP32 Base Board`
- `JWPLC Basic`
- `JWPLC Basic Core`

Las variantes como ESP32-S2, ESP32-S3, ESP32-C3, ESP32-C6, ESP32-H2, etc., deberían agregarse solo cuando exista un producto JWPLC que realmente las use y hayan sido validadas.

Esto reduce:

- Tamaño del package.
- Tiempo de mantenimiento.
- Complejidad del menú de placas.
- Riesgo de errores por targets no probados.
- Dependencias innecesarias de toolchains y librerías precompiladas.

---

## Estructura general del repositorio

```text
platform-jwplc/
├── JWPLC/
│   ├── JWPLC-2.0.0/
│   │   ├── boards.txt
│   │   ├── platform.txt
│   │   ├── cores/
│   │   ├── variants/
│   │   ├── libraries/
│   │   └── tools/
│   ├── package_jwplc_index.json
│   └── jwplc-esp32-*.zip
├── docs/
├── installers/
├── packages/
├── README.md
└── LICENSE
```

---

## Desarrollo

Flujo sugerido:

1. Desarrollar cambios en una rama `develop/*` o feature branch.
2. Validar en carpeta local enlazada al package de Arduino IDE.
3. Hacer commits pequeños por avance.
4. Agrupar cambios funcionales en una alpha.
5. Crear prerelease con tag correspondiente.
6. Actualizar `package_jwplc_index.json`.
7. Validar instalación desde Arduino IDE.
8. Hacer merge a `main` cuando la alpha esté validada.

---

## Créditos

Desarrollado por **JW Control**.

Basado en el core oficial de Espressif para Arduino ESP32, adaptado para placas industriales JWPLC.

---

## Licencia

Este repositorio mantiene la licencia indicada en el archivo `LICENSE`.

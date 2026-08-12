# Candidato de migracion Ethernet: backend ESP32 3.3.8

## Estado

Pendiente para evaluar en una rama separada. No forma parte del experimento de build-speed P3/P4.

## Contexto actual

`JWPLC_Ethernet` usa actualmente la libreria Arduino clasica `Ethernet.h` para W5500. La API interna se apoya, entre otros, en:

- `Ethernet.init()`
- `Ethernet.begin()`
- `Ethernet.hardwareStatus()`
- `Ethernet.linkStatus()`
- `Ethernet.maintain()`
- `Ethernet.setRetransmissionTimeout()`
- `Ethernet.setRetransmissionCount()`

La integracion Alpha26 se valido con el W5500 del JWPLC Basic y con arbitraje del bus SPI compartido mediante el mutex JWPLC.

## Aclaracion importante sobre versiones

La libreria `Ethernet` incluida por Espressif en Arduino-ESP32 3.3.8 no es una revision 3.3.8 de la libreria Arduino `Ethernet 2.0.2`.

Son backends distintos:

- Arduino `Ethernet 2.0.2`: API `Ethernet.h`, orientada a W5100/W5200/W5500.
- Espressif Arduino-ESP32 `Ethernet 3.3.8`: API principal `ETH.h`, integrada con `Network`/`esp_netif` y con soporte de W5500 SPI.

Por tanto, cambiar de una a otra requiere adaptar la implementacion interna de `JWPLC_Ethernet`; no es un simple cambio de version en `library.properties`.

## Hallazgo de reproducibilidad Alpha4

La auditoria de build del 2026-08-09 confirmo que el package actual no aporta una copia bundled de Arduino `Ethernet 2.0.2` que proporcione `Ethernet.h`.

En la maquina de prueba, Arduino Builder selecciono:

`C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\Ethernet`

Version: `2.0.2`.

La carpeta `JWPLC/2.1.0/libraries/Ethernet`, version `3.3.8`, corresponde al backend Espressif y no proporciona `Ethernet.h`.

Consecuencia: hoy `JWPLC_Ethernet` puede depender de una libreria instalada en el sketchbook del usuario. Esto no es aceptable como estado final del package porque compromete reproducibilidad y puede cambiar el comportamiento entre PCs.

## Correccion conservadora antes de evaluar ETH.h

Antes de una migracion de backend se debe cerrar la reproducibilidad del backend ya probado:

1. Vendorizar dentro del package una copia exacta de la release oficial Arduino Ethernet `2.0.2`.
2. Usar una carpeta y nombre de libreria exclusivos de JWPLC, por ejemplo:
   - carpeta: `JWPLC_Ethernet_W5x00_Backend`
   - nombre: `JWPLC Ethernet W5x00 Backend`
3. Mantener los headers/API upstream, incluyendo `Ethernet.h`, para no modificar `JWPLC_Ethernet` mas de lo necesario.
4. Agregar un header marcador exclusivo del backend JWPLC y resolverlo antes de `Ethernet.h`, de forma que Arduino Builder importe primero la copia bundled y no una copia del sketchbook.
5. Conservar la licencia y avisos upstream de Arduino Ethernet.
6. Repetir las pruebas Ethernet ya validadas: DHCP, IP estatica, link, HTTP, stress TFT, Modbus TCP y coexistencia SPI.
7. Repetir benchmark de compilacion con la dependencia ya determinista.

Esta correccion NO implica migrar a `ETH.h`; simplemente hace autocontenido el backend actualmente probado.

## Por que vale la pena evaluar ETH.h despues

Posibles ventajas que deben medirse, no asumirse:

- integracion nativa con el stack `Network` del core ESP32;
- uso uniforme de `NetworkClient`/`NetworkServer` junto con WiFi/Ethernet;
- eventos de red y gestion de interfaces del core;
- menor dependencia de una libreria externa separada;
- mejor alineamiento futuro con el core ESP32 3.x.

## Riesgos especificos del JWPLC Basic

El W5500 comparte bus SPI con TFT, microSD y FRAM. El backend actual se diseno alrededor de llamadas sincronas protegidas por `jwplcSPI_acquire()` / `jwplcSPI_release()`.

Antes de migrar a `ETH.h` se debe comprobar:

1. Que el driver W5500 de Espressif pueda coexistir de forma segura en el mismo bus SPI con TFT, SD y FRAM.
2. Que ninguna tarea/evento interno del driver acceda al SPI fuera del arbitraje esperado por JWPLC.
3. Funcionamiento con la conexion fisica real del JWPLC Basic, incluyendo uso sin IRQ si aplica.
4. DHCP y configuracion estatica.
5. Desconexion/reconexion de RJ45 durante runtime.
6. HTTP/TCP continuo y diagnostico por capas existente.
7. Modbus TCP existente.
8. Impacto en flash, RAM y tiempos de compilacion.
9. Basic Core con Ethernet deshabilitado.
10. Compatibilidad de la API publica `JWPLC_Ethernet`; una migracion de backend no debe romper sketches ya probados.

## Evidencia historica del repositorio

La integracion Alpha26 documenta explicitamente soporte W5500, proteccion del bus SPI compartido y validacion en Basic/Core. No se encontro una decision historica explicita que descarte `ETH.h` de Espressif.

Por tanto, no se debe afirmar que la libreria Arduino clasica se eligio por una limitacion conocida de `ETH.h`; la razon exacta no quedo registrada.

## Propuesta de rama futura para migracion de backend

Cuando termine el trabajo actual de build speed y la dependencia clasica este vendorizada/validada:

`v2.1.0-alpha.4/feature/ethernet-esp32-eth-eval`

El objetivo de esa rama debe ser crear un backend experimental manteniendo intacta la API publica `JWPLC_Ethernet`, ejecutar la misma bateria de pruebas ya usada con Ethernet 2.0.2 y comparar resultados antes de decidir una migracion.

## Decision actual

- No migrar a `ETH.h` 3.3.8 dentro de la rama de build speed.
- Si corregir ahora la reproducibilidad de dependencias que afecta a los benchmarks y al package.
- Para Adafruit, forzar las copias bundled del package.
- Para Ethernet, vendorizar primero Arduino Ethernet 2.0.2 como backend JWPLC autocontenido.
- Evaluar `ETH.h` 3.3.8 despues, en una rama separada y con pruebas fisicas.

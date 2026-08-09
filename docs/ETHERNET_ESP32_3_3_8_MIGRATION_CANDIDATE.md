# Candidato de migracion Ethernet: backend ESP32 3.3.8

## Estado

Pendiente para evaluar en una rama separada. No forma parte del experimento de build-speed P3.

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

## Por que vale la pena evaluarlo

Posibles ventajas que deben medirse, no asumirse:

- integracion nativa con el stack `Network` del core ESP32;
- uso uniforme de `NetworkClient`/`NetworkServer` junto con WiFi/Ethernet;
- eventos de red y gestion de interfaces del core;
- menor dependencia de una libreria externa del sketchbook;
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

## Propuesta de rama futura

Sugerencia cuando termine el trabajo actual de build speed:

`v2.1.0-alpha.4/feature/ethernet-esp32-eth-eval`

El objetivo de esa rama debe ser crear un backend experimental manteniendo intacta la API publica `JWPLC_Ethernet`, ejecutar la misma bateria de pruebas ya usada con Ethernet 2.0.2 y comparar resultados antes de decidir una migracion.

## Decision actual

- No cambiar el backend Ethernet dentro de P3.
- Registrar la evaluacion como pendiente separada.
- Mantener por ahora el backend probado con `Ethernet.h` mientras se cierra la optimizacion de build.

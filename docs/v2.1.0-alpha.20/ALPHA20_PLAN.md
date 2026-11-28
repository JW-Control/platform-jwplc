# 2.1.0-alpha.20 · Línea Red: plan

Reservada el 28/11/2026 a pedido del propietario («procede», hilo del piloto Autotech). Va **aparte del
alpha.19** (C++ U8, IEC U5, Ejecutar en vivo, cancelar compilación): no comparte ramas ni gates con él. Si el
orden de publicación cambia, se renumera antes de publicar.

## Objetivo

Que el JWPLC Basic hable con Internet por **cable o WiFi con el mismo sketch**, y que el primer uso sea el
piloto de presión constante de Autotech conectado a JW Cloud (panel de presión, bombas y fallas, alertas
push en la app de JW Cloud).

## Árbol de librerías

```text
Capa física     JWPLC_Ethernet (W5500, existe)      JWPLC_WiFi (nueva)
Capa común      JWPLC_Red (nueva): cable, WiFi o automático + JWPLC_RedClient
Seguridad       JWPLC_TLS (nueva): HTTPS sobre JWPLC_RedClient
Protocolos      JWPLC_ModbusTCP y JWPLC_WebScreen (existen; pasarlas a JWPLC_Red)
                JWPLC_HTTP → JWPLC_Cloud (JW Cloud) · JWPLC_MQTT (opcional) · JWPLC_Hora (NTP → RTC)
                JWPLC_WebServer (páginas propias; estudio de mercado en hilo aparte)
```

## Hitos

| Hito | Resultado | Cierre |
|---|---|---|
| H1 | `JWPLC_WiFi` + `JWPLC_Red` 0.1.0 con ejemplos | compilan ESP32; cable probado en JWPLC Sim; WiFi y modo automático probados en placa |
| H2 | `JWPLC_TLS`: HTTPS sobre `JWPLC_RedClient` (cable y WiFi), raíces de Let's Encrypt | GET HTTPS a `ota.jw-control.com/health` por cable en el Sim y por los dos en placa; RAM libre medida con Modbus y TFT |
| H3 | `JWPLC_Cloud`: cola de eventos y envío en lotes a JW Cloud (patrón de `Telemetry_Cloud.h` de Laundry), token por equipo | arquitectura 01 de bombeo enviando a JW Cloud de pruebas desde el Sim; fallas inyectadas llegan como alerta |
| H4 | `JWPLC_ModbusTCP` y `JWPLC_WebScreen` sobre `JWPLC_Red` (también por WiFi) | ejemplos existentes sin cambios por cable; nuevos por WiFi en placa |

Fuera de este repo, en paralelo: ruta y vista de bombeo en JW Cloud (`JW-Control-Organization/JW-Cloud`) y
«WiFi virtual» en JWPLC Sim (segundo puerto de red junto al W5500, TLS hecho por la PC).

## Reglas de la línea

- Librerías nuevas en código fuente (sin `precompiled=full`) mientras dure la línea.
- API del sketch solo con «.»; el `loop()` nunca espera a la red.
- `JWPLC_Red` no arrastra WiFi: lo engancha solo si el sketch incluye `JWPLC_WiFi.h`, para que los
  sketches de cable sigan corriendo en JWPLC Sim.

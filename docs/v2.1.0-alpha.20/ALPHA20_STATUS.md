# 2.1.0-alpha.20 · Línea Red: estado vivo

## 1. Ramas y sincronización
| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea | `v2.1.0-alpha.20/Red/main` | desde `release/v2.1.x` = `main` @ `733d229` (alpha.18 publicada) |
| platform-jwplc | Feature en curso | `v2.1.0-alpha.20/Red/feature/wifi-y-red` | H1, PR hacia `…/main` |

`BASE_SYNC=OK` · `KNOWLEDGE_SYNC=PENDIENTE` (reservar alpha.20 en `RAMAS_Y_TAGS.md` de jwplc-dev-mcp)

No toca el editor ni el HMI Designer.

## 2. Librerías en modo fuente
`JWPLC_WiFi` y `JWPLC_Red` (nuevas, sin `.a`). Ninguna precompilada cambió.

## 3. Qué funciona en la máquina
Nada validado en placa todavía.

## 4. Qué existe aislado o detrás de un flag
`PENDIENTE_DE_PROMOCIÓN:` `JWPLC_WiFi`, `JWPLC_Red` (H1, falta gate de taller).

## 5. Evidencia vigente
`A20_1_WIFI_RED_20261128.md`: compila ESP32 (4 ejemplos medidos) y cable en JWPLC Sim contra un servidor HTTP.

## 6. Plan de hitos y NEXT
H1 WiFi + Red · H2 TLS · H3 JW Cloud · H4 Modbus TCP y WebScreen sobre Red (detalle en `ALPHA20_PLAN.md`).
`NEXT_GATE=A20_1_WIFI_PLACA` · `HARDWARE=AUTORIZADO(taller: placa + router WiFi)`

## 7. Para retomar
- H1 en `…/feature/wifi-y-red`; falta que el taller pruebe `01.WiFi_Conectar` y `01.Red_Cable_o_WiFi` en placa.
- Siguiente: H2 `JWPLC_TLS` (BearSSL sobre `Client`, raíces de `Ota_RootCa.h` del firmware Laundry).
- No mezclar con el alpha.19.

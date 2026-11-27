# v2.1.0-alpha.18 — JWPLC Sim: estado vivo

Actualizado: `2026-11-27` (feature de cierre: compilación rápida fusionada, ejemplos de ESP32 podados, README)

## 1. Ramas y sincronización

| Repo | Rol | Rama | SHA / estado |
|---|---|---|---|
| platform-jwplc | Base de la línea | `v2.1.0-alpha.18/JWPLC-Sim/main` | `0661e83` (#247, #248) |
| platform-jwplc | Cierre | `v2.1.0-alpha.18/JWPLC-Sim/feature/cierre` | PR hacia `…/main` |
| openplc-editor | Publicado | `release/v2.1.x` | `7272626` (#23, tag `jwplc-editor-v4.3.3-jwplc.3`, pre-release 18/11) |
| openplc-editor | Base de la línea | `v2.1.0-alpha.18/JWPLC-Sim/main` | `77680f0` |
| openplc-editor | Designer / VPP empaquetados | `PLATFORM_DESIGNER_SHA` / `PLATFORM_VPP_SHA` | platform `3f29398` / `0647a84` (sin cambios desde jwplc.2) |

La rama por defecto del editor en GitHub pasó a `release/v2.1.x` (propietario, 27/11). Ramas del editor
limpiadas el 27/11: en GitHub quedan `main` y `development` (siguen a Autonomy), `release/v2.1.x` y la `…/main` de esta
línea.

## 2. Qué entró en `…/main`

| Repo | PR | Rama | Merge | Contenido |
|---|---|---|---|---|
| openplc-editor | #19 | `…/feature/integracion-openplc` | `f344a79` | botón Simular, `--open-project`, `--jwsim-close`; HMI: TREND ch2/ch3 y pisos de Baliza, Guardar sin reescribir |
| openplc-editor | #20 | `…/feature/modbus-tcp-enlace-tardio` | `6ef47ef` | Modbus TCP (`MBETH_JWPLC`): listener 502 con enlace tardío y ADU reensamblado |
| openplc-editor | #23 | `…/main` → `release/v2.1.x` | `7272626` | 4.3.3-jwplc.3 publicado (VPP 2.1.0-alpha.17 sin cambios) |
| platform-jwplc | #247 | `…/feature/curso-al-sim` | `46228c9` | el curso, retos y arquitecturas pasan a `JWPLC_Aprende` (jwplc-sim). Último SHA con el curso: `5b0d0bd` |
| platform-jwplc | #248 | `…/feature/compilacion-rapida` | `0661e83` | búsqueda de librerías liviana + 5 librerías precompiladas ([A18_COMPILACION_RAPIDA_20261127.md](A18_COMPILACION_RAPIDA_20261127.md)) |
| platform-jwplc | (cierre) | `…/feature/cierre` | — | poda de ejemplos de arduino-esp32 (`tools/alpha18/podar_ejemplos_esp32.sh`), README del package y de las 5 librerías, `PRE_RELEASE.md`, este estado |

Librerías en modo fuente durante la línea: ninguna. Las precompiladas de alpha.17 no cambiaron. Las 5 nuevas
(`JWPLC_RS485`, `JW_RS485`, `JWPLC_ModbusRTU`, `JW_ModbusRTU`, `JW_RTC`) se generaron con
`tools/alpha16/regenerate_precompiled.sh` (ABI_COMPAT=PASS). El cambio en `JWPLC_TFT.h` está dentro de la guarda de
la búsqueda de librerías y no altera el `.a`.

## 3. Gates

```text
A18_H0_APERTURA=PASS
A18_H1_EDITOR_SIM=DEFERRED            # Simular, --open-project, second-instance y --jwsim-close PASS; «Localizar» y sin project.json → alpha.19
A18_H2_HMI_ENLACES=DEFERRED           # unit tests PASS; checklist en vivo con el Designer → alpha.19
A18_H3_MODBUS_TCP=DEFERRED            # prueba en placa con cable de red → alpha.19
A18_COMPILACION_RAPIDA=PASS_PHYSICAL  # PC del propietario + placa COM14 (RTC, Modbus RTU, RS485)
A18_BENCHMARK_PC_PROPIETARIO=PASS     # 05 limpia 55 → 17,5 s; 02.Runtime_En_Vivo 100 → 30 s (propietario)
A18_EJEMPLOS_ESP32=PASS               # solo ejemplos (propietario, 27/11); placas genéricas ESP32 → decisión en alpha.19
A18_H4_CIERRE=IN_PROGRESS
```

## 4. Riesgos y deuda

- H1–H3 del editor pasan a alpha.19 (propietario, 27/11: «cerremos el 18 con lo que teníamos»). El editor jwplc.3 ya
  está publicado; H3 se puede correr en la placa del propietario con cable de red.
- Benchmark de cierre en la PC del propietario: se hace después de publicar, con el package del gestor de placas, y
  se anota en un commit en `main` (propietario, 27/11). Medición previa con `jwplc_local`: 02.Runtime_En_Vivo 100 → 30 s.
- alpha.19 decide qué hacer con las placas genéricas `ESP32 Board` y `ESP32 Family`: quitarlas (recomendado, usar el
  core oficial de Espressif) o llevarlas a una plataforma propia con todos los ejemplos.
- `--jwsim-close` pregunta «¿Seguro que quieres salir?» aun sin cambios: falta decisión de producto.
- Propuesta para el editor (Kenyi): `compileArduino` (`editor-compiler-platform-port.ts`) debería reescribir solo los
  archivos generados que cambian. Hoy la librería `src` del proyecto se recompila siempre (~2–3 s).
- La poda de ejemplos se pierde si se reimporta arduino-esp32: repetir `tools/alpha18/podar_ejemplos_esp32.sh`.
- La placa genérica `jwplc:esp32:esp32` del package también pierde esos ejemplos. Para un ESP32 genérico se usa
  el core oficial de Espressif.
- Upstream 4.3.4 y 4.3.5 del editor sin integrar: alpha.19.

## 5. Para retomar

1. Fusionar el PR de cierre en `…/main` (CI verde + OK del propietario).
2. PR `…/main` → `release/v2.1.x`: publica la Release 2.1.0-alpha.18 y **sólo se fusiona con autorización
   explícita**.
3. Tras publicar: `release/v2.1.x` → `main` con merge commit y fast-forward de vuelta, PR de índices del package,
   `package.lock` de jwplc-sim al tag `v2.1.0-alpha.18`, y borrar las `…/main` de la línea en platform y editor.

`NEXT_GATE=A18_H4_CIERRE`

# Consolidación de ramas de 2.1.0-alpha.14 (24/10/2026)

Rama de trabajo: `v2.1.0-alpha.14/HmiDesigner/feature/consolidacion-ramas` (desde `v2.1.0-alpha.14/HmiDesigner/main`).
Se revisan las ramas en el orden que indica el propietario; lo pendiente se fusiona aquí y la rama revisada se borra.
Cada rama borrada queda anotada con su último SHA para poder recuperarla (`git push origin <sha>:refs/heads/<rama>`).

## 1. `v2.1.0-alpha.15-HmiDesigner`

```text
TIP=157d4e833927d26f5e6419f28083ab2f76ec226d  (2026-10-23, feat(rs485,modbusrtu) #138)
COMMITS_FUERA_DE_LA_ALPHA=0      (ancestro de v2.1.0-alpha.14/feature/hmi-designer)
FUSION=no hace falta (Already up to date)
DECISION=BORRAR
```

- La línea entró completa en la integración `06a636a` (merge sobre `release/v2.1.x`). Desde entonces la rama de la alpha
  suma 54 commits propios
  (ICON, pines con nombre, congelado de los `.a`, cierre) más los 177 de Alpha13 que trajo `release/v2.1.x`; la línea HMI
  no recibió nada nuevo.
- En esa fusión, 10 archivos que la línea HMI había cambiado quedaron distintos de su versión `[visto]`, todos por
  combinarlos con Alpha13, no por perder trabajo:
  - `JWPLC_TFT` (`.h`, `.cpp`, `tft_setup.h`, `library.properties`, `.a`): se suman el DISPON diferido y
    `_batchOwner` de Alpha13 a las fuentes, elipses/triángulos y `pushImage` de la línea HMI; el `.a` se regeneró en
    `d7afe35` y `161e406`.
  - README de `JWPLC_Display`, `JWPLC_TFT`, `JWPLC_GlobalPeripherals`, `JWPLC_RS485` y `JWPLC_ModbusRTU`: cabecera de
    versión 2.1.0-alpha.14 y secciones de Alpha13.
- Las demás diferencias entre el tip HMI y la alpha son cambios posteriores en la alpha.

Borrado remoto: lo hace el propietario en GitHub (la sesión en la nube no puede borrar ramas remotas: HTTP 403).

## 2. Renombrado al esquema `v2.1.0-alpha.14/HmiDesigner/…` (24/10)

Mismos commits, ramas nuevas; las viejas las borra el propietario en GitHub (la nube recibe HTTP 403 al borrar).

| Rama vieja | Rama nueva | SHA | PR viejo → nuevo |
|---|---|---|---|
| `v2.1.0-alpha.14/feature/hmi-designer` | `v2.1.0-alpha.14/HmiDesigner/main` | `9a27b13` | #141 → #157 (borrador, hacia `release/v2.1.x`) |
| `claude/project-thread-iuopjt`, `v2.1.0-alpha.14/feature/designer-installer` | `…/HmiDesigner/feature/designer-installer` | `7c25780` | #151, #156 → #158 |
| `v2.1.0-alpha.14/feature/readmes-librerias` | `…/HmiDesigner/feature/readmes-librerias` | `b11d142` | #152 → #159 |
| `v2.1.0-alpha.14/feature/prueba-fisica-combinada` | `…/HmiDesigner/feature/prueba-fisica-combinada` | `3477474` | #153 → #160 |
| `v2.1.0-alpha.14/feature/benchmark-compilacion` | `…/HmiDesigner/feature/benchmark-compilation` | `d260628` | #154 → #161 |
| `v2.1.0-alpha.14/feature/consolidacion-ramas` | `…/HmiDesigner/feature/consolidacion-ramas` | `6b46737` | — → #162 |
| `v2.1.0-alpha.14/feature/cierre-alpha14` | `…/HmiDesigner/feature/cierre-alpha14` | `05866f3` | #155 → #163 |

`v2.1.0-alpha.14-OpenPLC-HMI` (Kenyi) no se renombra.

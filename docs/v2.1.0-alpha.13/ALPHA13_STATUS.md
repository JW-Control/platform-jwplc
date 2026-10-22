# Alpha13 — estado vivo

> Documento único y corto (máximo 150 líneas). Se reescribe al cerrar cada hito; no se añaden cortes históricos.
> Historial completo hasta el 21/10/2026: [history/ALPHA13_STATUS_HISTORIAL_HASTA_20261021.md](history/ALPHA13_STATUS_HISTORIAL_HASTA_20261021.md). Solo se consulta cuando hace falta evidencia de un gate concreto.

**Actualizado:** 22/10/2026 · **Publicada:** [v2.1.0-alpha.13](https://github.com/JW-Control/platform-jwplc/releases/tag/v2.1.0-alpha.13) (PreRelease, índice dev)

## 1. Ramas y sincronización

| Rol | Rama | Estado |
|---|---|---|
| Base del alpha | `v2.1.0-alpha.13/feature/cleanup-robustness` | `6034bfb8` (PR #127 y #128); ya no recibe cambios |
| Publicación | `release/v2.1.x` | `11237c2c` (PR #129, squash) + este cierre |
| Índice Boards Manager | `main` | índice dev con alpha.13 (PR #130); árbol sincronizado con `release/v2.1.x` |

`BASE_SYNC=OK` · `KNOWLEDGE_SYNC=PENDING_PR_docs/presupuesto-tokens-ci` (`jwplc-dev-mcp`)

## 2. Qué contiene el package (`JWPLC/2.1.0`)

- **F1:** `JWPLC_ETH_CS=5` como SS del W5500 y guarda contra GPIO6–11.
- **G10:** `JWPLC_W5500_SafeAccess` (mutex SPI, owner y generación de socket, `BUS_BUSY` explícito), Client/Server/UDP cooperativos y Modbus TCP Client/Server sobre esa base.
- **F137:** `libJWPLC_Ethernet.a` y `libJWPLC_ModbusTCP.a` precompilados (`precompiled=full`), política Ethernet weak en Basic y strong en Basic Core.
- **C:** README de Ethernet y Modbus TCP ampliados para un primer PLC.
- Árbol del package: `6b498f4a0797e2a1bb0054b005b91f985bc071e2` (N3 funcional `cc17716f` más dos README).

## 3. Publicación

| Dato | Valor |
|---|---|
| Release | [#39740054387](https://github.com/JW-Control/platform-jwplc/actions/runs/39740054387), commit `11237c2c` |
| Asset | `jwplc-esp32-2.1.0-alpha.13.zip`, 24 527 296 bytes, 1992 archivos, raíz única `2.1.0/` |
| SHA-256 | `b3dc5ac710229bd272b0382fb56dd8988e242a66eebc872198cd82c5bdc57428` (asset, digest de GitHub e índice dev coinciden) |
| Contenido | Igual byte a byte al árbol `JWPLC/2.1.0` de `11237c2c` |
| Índice | `main/JWPLC/package_jwplc_index_dev.json` (PR #130); índice estable sin cambios |

## 4. Evidencia

| Nivel | Resultado | Referencia |
|---|---|---|
| N3 package | PASS 13/13 Windows+Ubuntu | [#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047) |
| H física | `PASS_FUNCIONAL_ACOTADO` (COM4 Master, COM14 Slave, FC03 4/4, UDP 3/3, RTU) | `A13_F137_H_DICTAMEN_FISICO_20261021.md` |
| Guard F137-C | PASS en la fuente publicada | [#39739018771](https://github.com/JW-Control/platform-jwplc/actions/runs/39739018771) |
| Package Smoke | PASS en el PR de publicación | [#39739102108](https://github.com/JW-Control/platform-jwplc/actions/runs/39739102108) |
| Postpublicación CLI | Instalación limpia desde el índice dev publicado (Arduino CLI 1.3.1, Linux) y 6/6 compilaciones: `01.ModbusTCP_Server`, `02.ModbusTCP_Client` y `01.Ethernet_DHCP_Basic` en `jwplcbasic` y `jwplcbasiccore`. `JWPLC_Ethernet` y `JWPLC_ModbusTCP` se enlazan desde sus `.a` | sesión del 22/10 |

**Límites que no se convierten en PASS:** Basic Core físico, R3 interno, R4 de 1805 s, renovación DHCP/DNS, E1/E2, marcador literal de BOOT por UART, tiempo de upload.

## 5. Pendiente

1. Operador: instalar desde Arduino IDE (GUI) con el índice dev y subir un ejemplo a una placa desde el package distribuido. Con eso, `ALPHA13=CLOSED_PUBLISHED`.
2. Revisar y fusionar en `jwplc-dev-mcp` la rama `docs/presupuesto-tokens-ci` (auditoría y regla de presupuesto).
3. Borrar ramas obsoletas (`claude/sleepy-hopper-i4i23y` en los tres repositorios y las `codex/alpha13-*` ya fusionadas o cerradas).

```text
ALPHA13=PUBLISHED_PENDING_IDE_GUI_CHECK
RELEASE_TAG=v2.1.0-alpha.13
RELEASE_ZIP_SHA256=b3dc5ac710229bd272b0382fb56dd8988e242a66eebc872198cd82c5bdc57428
PACKAGE_TREE=6b498f4a0797e2a1bb0054b005b91f985bc071e2
POSTPUBLISH_CLI=PASS_6_OF_6
POSTPUBLISH_IDE_GUI=NOT_RUN
POSTPUBLISH_UPLOAD=NOT_RUN
NEXT_FAILURE_ID=F138
NEXT_GATE=OPERATOR_IDE_GUI_INSTALL_AND_UPLOAD
```

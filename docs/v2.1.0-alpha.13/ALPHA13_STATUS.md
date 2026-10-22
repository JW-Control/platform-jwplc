# Alpha13 — estado vivo

> Documento único y corto (máximo 150 líneas). Se reescribe al cerrar cada hito; no se añaden cortes históricos.
> Historial completo hasta el 21/10/2026: [history/ALPHA13_STATUS_HISTORIAL_HASTA_20261021.md](history/ALPHA13_STATUS_HISTORIAL_HASTA_20261021.md). Solo se consulta cuando hace falta evidencia de un gate concreto.

**Actualizado:** 22/10/2026 · **Rama base:** `v2.1.0-alpha.13/feature/cleanup-robustness`

## 1. Ramas y sincronización

| Rol | Rama | Estado |
|---|---|---|
| Base de integración | `v2.1.0-alpha.13/feature/cleanup-robustness` | `8704a1a0` (PR #127) |
| Publicación | `release/v2.1.x` | alpha.12; 1118 commits por detrás de la base |
| Índice Boards Manager | `main` | alpha.12 |

`BASE_SYNC=OK_PR126` · `KNOWLEDGE_SYNC=OK_PR29` (`jwplc-dev-mcp/main` `55015c5e`)

## 2. Qué contiene el package (`JWPLC/2.1.0`)

- **F1:** `JWPLC_ETH_CS=5` como SS del W5500 y guarda contra GPIO6–11.
- **G10:** `JWPLC_W5500_SafeAccess` (mutex SPI, owner y generación de socket, `BUS_BUSY` explícito), Client/Server/UDP cooperativos y Modbus TCP Client/Server sobre esa base.
- **F137:** `libJWPLC_Ethernet.a` y `libJWPLC_ModbusTCP.a` precompilados (`precompiled=full`), política Ethernet weak en Basic y strong en Basic Core.
- **C:** README de Ethernet y Modbus TCP ampliados para un primer PLC. Sin cambios funcionales.
- Árbol del package: `6b498f4a0797e2a1bb0054b005b91f985bc071e2`. El árbol funcional N3 es `cc17716f173509b1e0f3c0df5eb99549014c1d61`; la diferencia entre ambos son solo esos dos README.

## 3. Evidencia vigente

| Nivel | Resultado | Referencia |
|---|---|---|
| N3 package | PASS 13/13 Windows+Ubuntu | [#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047) |
| H física | `PASS_FUNCIONAL_ACOTADO` (COM4 Master, COM14 Slave, FC03 4/4, UDP 3/3, RTU) | `A13_F137_H_DICTAMEN_FISICO_20261021.md` |
| C release-like | PASS en el HEAD del PR #126 | [#39663616618](https://github.com/JW-Control/platform-jwplc/actions/runs/39663616618) |
| Benchmark PC | Basic frío −22,89 %, Basic Core frío −12,75 % | `A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md` |
| Guard F137-C en BASE | PASS en `8704a1a0` (FAIL previo en `efe20fe0` y `908a9c2f`) | [#39729850539](https://github.com/JW-Control/platform-jwplc/actions/runs/39729850539) |

**Por qué falla el guard en BASE:** `tools/alpha13/f137_c_release_guard.py` solo acepta el árbol N3 (`cc17716f`). El PR #126 cambió dos README, así que el árbol pasó a ser `6b498f4a` y el guard se detiene con `F137_C_REVIEW_STOP_PACKAGE_TREE_NOT_N3`. No es una regresión del producto. Con la aprobación del propietario (22/10), el guard acepta `6b498f4a` como segundo árbol aprobado y sigue verificando los SHA-256 de los `.a`, `precompiled=full` y la política weak/strong. Un cambio en una fuente del package sigue deteniéndolo.

**Límites que no se convierten en PASS:** Basic Core físico, R3 interno, R4 de 1805 s, renovación DHCP/DNS, E1/E2, marcador literal de BOOT por UART, tiempo de upload.

## 4. Pendiente para cerrar Alpha13

1. ~~Fusionar la corrección del guard en BASE~~: PR #127, guard F137-C en verde ([#39729850539](https://github.com/JW-Control/platform-jwplc/actions/runs/39729850539)).
2. Retirar los 53 workflows `alpha13-*` de gates cerrados (rama `alpha13/retirar-workflows`) y fusionar BASE en `release/v2.1.x` con squash (ver sección 5). Publicación autorizada el 22/10.
3. Publicar: workflow `Release JWPLC Arduino Package`, `version=2.1.0-alpha.13`.
4. Postpublicación: asset y SHA-256 del ZIP, PR automático de índice hacia `main`, instalación limpia desde el índice dev con Arduino CLI e IDE y compilación de los dos FQBN.
5. Completar `PRE_RELEASE.md` con los metadatos reales y marcar `ALPHA13=CLOSED_PUBLISHED`.

## 5. Topología de publicación (leer antes de autorizar)

- `auto-release-jwplc-on-readme.yml` despacha la Release **automáticamente** cuando llega a `release/v2.1.x` un README con un `JWPLC_RELEASE_VERSION` nuevo. El README de la base ya marca `2.1.0-alpha.13`. **Fusionar la base en `release/v2.1.x` equivale a publicar.**
- Recomendación: PR de la base hacia `release/v2.1.x` con **squash** (un commit en lugar de 1118), revisado con el diff de `JWPLC/2.1.0` y el README.
- Cuidado con los 55 workflows `alpha13-*`: 11 tienen `pull_request` sin filtro de rama (`g10-p0-source-manifest`, `g10-promote-functional`, `g10-r-preflight`, `g10-r-source-build`, `h0-e1e2`, `h0-f1-com4-runner`, `h0-f1-package`, `preh-b2/b3/b4`, `preh-tcp124-offline/source-build`). Un PR hacia `release/v2.1.x` los dispararía y varios fallarían por harness obsoleto (F134, `CLIENT_CTOR_CHANGED`). Decisión del propietario (22/10): retirarlos de `.github/workflows/` antes de ese PR; siguen en el historial de Git. Se conservan `alpha13-f137-c-base-source.yml` y `alpha13-c-release-like.yml`. En el PR hacia `release/v2.1.x` corre además `ci-jwplc-package-smoke.yml`.
- No reutilizar un tag ni un asset existentes. El workflow rechaza un tag ya presente.

```text
ALPHA13=OPEN
PACKAGE_TREE=6b498f4a0797e2a1bb0054b005b91f985bc071e2
PACKAGE_FUNCTIONAL_CHANGES_SINCE_N3=NONE
BASE_CI=PASS_F137_C_8704a1a0
PENDIENTE_DE_PROMOCION_GLOBAL_ALPHA13=EMPTY_AUDITED_SCOPE
RELEASE_DISPATCH=AUTHORIZED_20261022
POSTPUBLISH=NOT_RUN
NEXT_FAILURE_ID=F138
NEXT_GATE=RETIRE_WORKFLOWS_THEN_RELEASE_PR
```

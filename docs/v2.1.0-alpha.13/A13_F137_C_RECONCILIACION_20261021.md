## Resultado final de integración aprobado (21/10/2026) — prevalece sobre el plan pendiente inferior

- **PR producto #125 = MERGED** por squash en la base `v2.1.0-alpha.13/feature/cleanup-robustness`, commit `23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`; los tres documentos de conflicto fueron conciliados, conservando las entradas de la base y la revisión C de la candidata. No hubo cambio de fuentes ni de archives después de N3.
- El package integrado `JWPLC/2.1.0` tiene **tree SHA `cc17716f173509b1e0f3c0df5eb99549014c1d61`**, el mismo de N3. Se conserva el par `precompiled=full`, símbolo Basic weak, override Basic Core strong y los `.a` canónicos.
- [CI reconciliación #39631264428](https://github.com/JW-Control/platform-jwplc/actions/runs/39631264428): **SUCCESS** Ubuntu y Windows sobre HEAD de PR reconciliado `3993f051`; [CI base source #39632213031](https://github.com/JW-Control/platform-jwplc/actions/runs/39632213031): **SUCCESS global**, dos jobs PASS sobre HEAD documental `7cf87eec9ab8eb4d7e7cb894c522e305b7a33ed1`, `F137_C_RELEASE_SOURCE=PASS_PACKAGE_TREE_IDENTICAL_TO_N3`, `F137_C_ARCHIVES=PASS_2_SHA256`, `F137_C_PROFILE_POLICY=PASS_WEAK_BASIC_STRONG_BASICCORE`.
- **Conocimiento PR privado #27 = MERGED en `knowledge/main`** mediante `28ac12e033106a6debeec45eded6d84f93c1a3ea`. `BASE_SYNC=OK` y `KNOWLEDGE_SYNC=OK`.
- **PreRelease NO ejecutada y no autorizada.** Comprobación de GitHub: tag y Release `v2.1.0-alpha.13` ausentes (404). La versión y raíz del ZIP siguen pendientes de workflow de publicación y validación postpublicación. No proclamar `CLOSED_PUBLISHED`.
- **Separación de incidencia heredada:** el workflow antiguo `alpha13-g10-integrated.yml` volvió a fallar sobre el commit de integración `23fae3a3`, con `RuntimeError: CLIENT_CTOR_CHANGED`, pero **el mismo error ya figuraba sobre la base anterior** en CI [#39487371928](https://github.com/JW-Control/platform-jwplc/actions/runs/39487371928), antes de adoptar F137. Es un **bloqueo de CI legacy por diagnosticar**, no evidencia de regresión en los archivos `.a` ni PASS del flujo viejo. Separar su diagnóstico durante la auditoría global; no crear un nuevo ID F sin causa verificada ni repetir hardware.
- Los apartados de abajo documentan el **plan anterior** al merge y permanecen únicamente como evidencia histórica.

```text
F137_C_REVIEW=PASS
F137_C_BASE_SOURCE_CI=PASS_39632213031
PR125=MERGED_23fae3a3
PR27=MERGED_28ac12e0
F137_PACKAGE_PROMOTION=COMPLETE_TREE_N3_IDENTICAL
BASE_SYNC=OK
KNOWLEDGE_SYNC=OK
LEGACY_G10_INTEGRATED_CI=FAIL_PREEXISTING_CLIENT_CTOR_CHANGED_AUDIT_PENDING
ALPHA13=OPEN
RELEASE_DISPATCH=NOT_AUTHORIZED
NEXT_GATE=ALPHA13_C_GLOBAL_AUDIT_AND_RELEASE_SOURCE_APPROVAL
```

---

# Alpha13 F137-C — Revisión de integración y preparación de PreRelease (21/10/2026)

> **Actualización de reconciliación (posterior a la primera comparación):** la rama base recibió exclusivamente una protección documental adicional en `A13_RELEASE_SOURCE_DECISION_20261020.md` y su estado, para bloquear la publicación prematura. Al concluir la sincronización documental de base quedaron **8 commits de base por incorporar** (conteo de este corte; comprobar nuevamente antes de resolver), con **tres documentos** de diferencia del lado base (`ALPHA13_STATUS.md`, `A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md` y `A13_RELEASE_SOURCE_DECISION_20261020.md`). El **package no cambió**. Los 6 commits/2 documentos que se mencionan más abajo corresponden al primer corte, no al estado posterior. No integrar ciegamente ninguno de estos documentos.

**Estado:** `C_REVIEW=PASS_NONPHYSICAL`; **NO** es aprobación para merge ni Release. Candidato: [PR #125 Draft](https://github.com/JW-Control/platform-jwplc/pull/125), destino `v2.1.0-alpha.13/feature/cleanup-robustness`. No tocar COM4 ni COM14; `H_F137=PASS_FUNCIONAL_ACOTADO` en [dictamen H](A13_F137_H_DICTAMEN_FISICO_20261021.md).

## 1. Fuente N3 inalterada

Árbol exacto del package candidato `JWPLC/2.1.0` = `cc17716f173509b1e0f3c0df5eb99549014c1d61`, igual al del HEAD de N3 validada `5e1010927a8c03c7ad46d38e4d5459b06bae543f`. Entre aquella N3 y la revisión del PR no cambiaron archivos dentro de `JWPLC/2.1.0`. El commit del PR sí incorpora docs, CI y runners posteriores, que NO invalidan el package compilado.

**CI C [#39628750096](https://github.com/JW-Control/platform-jwplc/actions/runs/39628750096)**, fuente `9c2d8a5af8bcb631ef19b7b263676da6ebdb59d9`: **SUCCESS global, Windows+Ubuntu**. Contrato ejecutable `tools/alpha13/f137_c_release_guard.py`: `F137_C_REVIEW=PASS_EXACT_14_PRODUCT_PATHS`, `F137_C_RELEASE_SOURCE=PASS_PACKAGE_TREE_IDENTICAL_TO_N3`, `F137_C_ARCHIVES=PASS_2_SHA256`, `F137_C_PROFILE_POLICY=PASS_WEAK_BASIC_STRONG_BASICCORE`. Los artifacts contienen manifest JSON, no binarios listos para flashear.

### Alcance de los 14 archivos del package a promover

**Seis piezas funcionales indivisibles**:

1. `JWPLC/2.1.0/cores/jwcontrol/jwplc_peripherals.cpp`: override fuerte `jwplcEthernetProfileAvailable=false` en Basic Core.
2. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp`: política débil habilitada por defecto para Basic.
3. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/library.properties`: `precompiled=full`.
4. `JWPLC/2.1.0/libraries/JWPLC_Ethernet/src/esp32/libJWPLC_Ethernet.a`: SHA-256 `f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4`.
5. `JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/library.properties`: `precompiled=full`.
6. `JWPLC/2.1.0/libraries/JWPLC_ModbusTCP/src/esp32/libJWPLC_ModbusTCP.a`: SHA-256 `d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d`.

**Ocho README** asociados (Ethernet, GlobalPeripherals, ModbusRTU, ModbusTCP, RS485, TFT, FRAM y SD). No se añaden ni eliminan periféricos del autoload; no hay cambio de `boards.txt`, `platform.txt`, pines, FlashFreq, bootloader distribuido ni otros archives.

La adopción de **solo los dos `.a` o de los cuatro archivos archive/properties no es suficiente**: el código C++ del perfil Ethernet debe acompañarlos. El guard no sustituye revisión humana del código ni prueba física adicional si cambian estos seis archivos.

## 2. Desfase y conflicto del PR

Al revisar en GitHub:
- Rama candidata: **107 commits delante / 6 detrás** de la base; `PR125=Draft`, GitHub `mergeable=false; mergeable_state=dirty`.
- La diferencia de base hacia candidata contiene **solo dos archivos con aportaciones exclusivas de base**: `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md` y `docs/v2.1.0-alpha.13/A13_TOOLING_FAILURES_AND_PREVENTION_20261006.md`. Ningún cambio de package exclusivo de base.
- Ello acota el esfuerzo de reconciliación a los documentos y **no demuestra por sí solo el algoritmo de resolución del conflicto**: comprobar Git merge-tree/merge review antes de mover referencias. No reemplazar el estado vigente de base por una copia antigua del PR, ni perder filas F del registro de fallos.

**Ruta propuesta, pendiente de permiso de merge:** sincronizar base hacia PR #125 en una operación revisada, resolver los dos documentos preservando historial y entradas vigentes, mantener exactamente el árbol package `cc17716f...`, correr el guard `--verify-release-source` desde el resultado, revisar diffs/estado real y PR mergeability. Solo después, con permiso explícito del propietario, fusionar PR #125 a la rama base. Confirmar en la **base ya integrada** el mismo árbol y hashes; si cambia package hay que revaluar N2/N3 por impacto y nunca reutilizar PASS a ciegas.

No hacer `git reset`, `clean`, rebase, force push ni merge no autorizado en la PC del operador o en GitHub.

## 3. Release correcto después de integración, NO ahora

- Workflow verificado de la base: `.github/workflows/release-jwplc-package.yml`, despacho manual `version=2.1.0-alpha.13`, **seleccionando la rama base Alpha13 deliberadamente**. El source `JWPLC/2.1.0` se toma del SHA de la rama elegida. La ruta `release/v2.1.x` sigue desfasada; no mezclar sus ~1095 commits ciegamente.
- El workflow exige `boards.txt`, `platform.txt`, `docs/v2.1.0-alpha.13/PRE_RELEASE.md`, crea ZIP raíz `2.1.0/` y PR hacia `main` del índice **dev**, deja canal estable v2.0.0 intacto, y apunta la PreRelease a `GITHUB_SHA`. La selección del SHA correcto sigue pendiente de integración.
- Antes de ejecutar, comprobar ausencia real de tag/Release duplicado y que README + PreRelease describan **los archives F137 ya integrados**, no solo el G10 fuente anterior. Registrar `RELEASE_SOURCE_SHA` y `PACKAGE_TREE` exactos.
- Después, validar tag/asset/tamaño/SHA ZIP, PR índice dev, instalación limpia con Arduino IDE/CLI desde **package publicado**, sketch mínimo/periféricos, y cierre con limitaciones y excepciones explícitas. **Sin estos pasos, `ALPHA13=CLOSED_PUBLISHED` sería falso.**

## 4. Conocimiento y gate siguiente

- [PR privado #27](https://github.com/JW-Control/jwplc-dev-mcp/pull/27): cuatro commits delante de `knowledge/main`, pendiente de merge/revisión. No propagar documentos privados a Actions públicos. `KNOWLEDGE_SYNC=PENDING_PR27`.
- [Checklist](ALPHA13_CIERRE_CHECKLIST.md) y [PreRelease](PRE_RELEASE.md) del PR se actualizaron en español, describiendo F137 como candidato (no publicado).
- No reabrir H de COM4/COM14: FC03 dual, UDP 3/3, RTU y autoload periféricos ya están acreditados. No falsear Basic Core físico, marcador literal UART, E1/E2, DNS/DHCP renewal o R4 prolongado.

```text
F137_C_REVIEW=PASS_NONPHYSICAL
F137_C_CI=PASS_BOTH_WINDOWS_UBUNTU_39628750096
F137_C_PACKAGE_TREE=cc17716f173509b1e0f3c0df5eb99549014c1d61
F137_C_PRODUCT_PATHS=EXACT_14
F137_C_PROFILE_POLICY=PASS_WEAK_BASIC_STRONG_BASICCORE
F137_H=PASS_FUNCIONAL_ACOTADO
PR125_MERGEABLE=FALSE_DOC_CONFLICTS_NEED_REVIEW
PR125_BASE_SYNC=PENDING_OPERATOR_APPROVED_RECONCILIATION
PENDIENTE_DE_PROMOCION=F137_6_FUNCTIONAL_PLUS_8_README
KNOWLEDGE_SYNC=PENDING_PR27_REVIEW_MERGE
PRE_RELEASE=NOT_PUBLISHED
ALPHA13=OPEN
NEXT_FAILURE_ID=F138
NEXT_GATE=F137_C_OPERATOR_APPROVE_PR125_RECONCILIATION_AND_MERGE_THEN_RELEASE_SOURCE_VERIFICATION
```

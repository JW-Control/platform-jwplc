# Alpha13 — C-AUDIT y C-RELEASE-LIKE antes de publicación

**Fecha:** 21/10/2026. **Ámbito:** revisión de documentación, repositorio y distribución, **sin cambios de firmware, merges, upload o Release**. Rama propuesta `codex/alpha13-c-global-release-like-20261021` → base `v2.1.0-alpha.13/feature/cleanup-robustness`. Este informe acompaña al PR y no adelanta un PASS de CI no ejecutada.

## Origen del código y protección de invariantes

- HEAD de base previo a esta revisión: `d86ccdd3fab2352d0a6579e676b07a54662ccfc3`. Última integración funcional/código: PR #125 squash `23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`. PR #124 G10 promovido: `3467a375ccaf7c7ee27220ed0bc348056e5e8219`.
- Tree Git inmutable de `JWPLC/2.1.0` esperado: `cc17716f173509b1e0f3c0df5eb99549014c1d61`; mismo tree N3 #39538155047 y verificación de fuente base #39632213031 (Windows y Ubuntu, dos jobs).
- Ethernet: `src/esp32/libJWPLC_Ethernet.a` SHA-256 `f190bbbbd69861941807a290be653de422e11a898d1a9583d289cc573d38a0f4`. Modbus TCP: `src/esp32/libJWPLC_ModbusTCP.a` SHA-256 `d5456307c5e8a4d2c4fcd3252464b96859faaa4df3ece3ca23e3c720f3b25a1d`. Ambos `precompiled=full`. Basic policy `weak`, Basic Core `strong`. No modificar core, archives, GPIO4, pines, variants ni `platform.txt`.
- La rama candidata del PR #125 fue fusionada por squash. Compararla directamente con la base genera divergencia histórica; **no** es prueba de que su firmware tenga mejoras pendientes. Confirmar por árbol y hashes del producto. No fusionar `release/v2.1.x` a ciegas.

## Inventario de evidencia ya cerrada (se reutiliza, no se repite)

| Gate | Evidencia canónica del registro vivo | Alcance y límites |
|---|---|---|
| G1 — DNS truncation | `G1_STATUS=CLOSED_PASS`, fuente producto `8bc48b7`, paquetes válidos 3/3 y prefijos truncados 136/136 | No acredita DNS real bajo todas las condiciones de red |
| G2 — TCA / EN_IO | `G2_STATUS=CLOSED_PASS`, producto `6a585693` | Conserva configuración de seguridad de I/O |
| TFT closure | `TFT_CLOSURE=CLOSED_PASS` | No habilita features nuevas |
| G3 — RMW/shadow TCA | `G3=CLOSED_PASS`, producto `8b8b9a7`, físico 150/150 | Ensayo dirigido, no afirmación de concurrencia universal |
| G4 — TFT SPI batch | `G4=CLOSED_PASS`, producto `46a6957`, físico 50/50 y regresiones 4/4 | Validación acotada a los casos registrados |
| G5 — TCP | `G5=CLOSED_PASS`, producto `c45b77f`, físico TX 5000 / RX 128 / Server 64, regresiones 3/3 | Antes de la promoción posterior de G10 |
| G6 — UDP | `G6=CLOSED_PASS`, TX 2048, RX parcial 256, recuperación/reopen 16 | La etiqueta histórica `G6_A13_008=REVIEW_MAPPING_PENDING` es reconciliación de auditoría, no PASS adicional |
| G7/G7C — FRAM | `G7C_FRAM_CANONICAL=CLOSED_PASS`, producto `f2a93a8`, nativo 11/11 y CI #38185454578 | `G7C_RUNTIME_PHYSICAL_RETEST=NOT_EXECUTED_NOT_REQUIRED_BY_CURRENT_DECISION`; no inventarlo |
| G8 — RS485/RTU | `G8_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`, producto `fb71d65`, COM4/COM14 FC03 12/12 | UART/RTU real, sin ensayos de todo el campo industrial |
| G9 — SD/RTC/botonera | `G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`, SD read-only + RTC + UP press/release COM4, producto `63e0291` | No equivale a escritura SD/hot-removal |
| H0-F1 — SS de W5500 | `JWPLC_ETH_CS=5` en `w5100.cpp` y `#error` para GPIO6-11; producto [PR #116](https://github.com/JW-Control/platform-jwplc/pull/116) | Precondición de SPI corregida; E1/E2 pendientes |
| G10 — SafeAccess/TCP/UDP/Modbus | Promovido en [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124), N0/N1/N2/N3 y PH0/R1/R2; R1 605 s: 589 FC03 por PLC + 589 UDP; R2 3/3 recuperación y 2 PHY reales | R3 interno, R4 1805 s, DNS y renovación DHCP no ejecutados |
| F137 — precompilados por FQBN | [N3 #39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047) 13/13 por SO; [F137-C #39631264428](https://github.com/JW-Control/platform-jwplc/actions/runs/39631264428) 2 jobs; H COM4/COM14 `PASS_FUNCIONAL_ACOTADO` | Basic Core físico y BOOT marker literal no capturado |

**Último código vs documentación:** después del squash `23fae3a3`, la base añadió `7cf87ee` y `d86ccdd` sin modificar el árbol `JWPLC/2.1.0`. Los PASS de source anteriores pertenecen a sus SHA específicos y al tree verificado, no a cualquier commit nuevo.

## Incidencia legacy G10 — causa y clasificación

El workflow `.github/workflows/alpha13-g10-integrated.yml` todavía ejecuta el **compositor histórico de overlays** `a13_g10_integrated.py`. Durante el contrato B03, `a13_g10_p1g_c2_s2b1_client_server_lease.py` exige `"_jwplcSocketToken" not in body` dentro del constructor `EthernetClient(uint8_t s)` y lanza `RuntimeError: CLIENT_CTOR_CHANGED`. El producto real `JWPLC_W5x00_Ethernet.h` **ya contiene** el token y `currentSocketTokenLocked(s)`, correctamente promovidos, por lo que el compositor intenta parchear una fuente que **ya implementó** la lógica del parche. El fallo es precondición de harness obsoleto, no una regresión demostrada del firmware.

El error aparece antes de F137 en [#39487371928](https://github.com/JW-Control/platform-jwplc/actions/runs/39487371928) en Windows y Ubuntu. Ambos `overlay-compose` FAILURE; `arduino-review` y `N3` SKIPPED: **no** etiquetarlos PASS. Clasificación: `LEGACY_HARNESS_STALE_POST_PROMOTION`. Se conserva evidencia de FAILURE y se propone marcar **solo** el workflow antiguo como supersedido, sin modificar `JWPLC/` ni forzar otro flash. No crear F138 por un reintento del mismo caso; `NEXT_FAILURE_ID=F138` sin asignar.

## PENDIENTE_DE_PROMOCIÓN, riesgos y discrepancias

- G10: `EMPTY`. F137: `EMPTY`. G1–G9: los cierres canónicos registran promoción productiva; no apareció ningún cambio funcional **validado** limitado a un overlay pendiente de integrar. **C-AUDIT dictamen de inventario: `PENDIENTE_DE_PROMOCION_GLOBAL=EMPTY`**, condicionado a que el PR siga con diff `JWPLC/2.1.0` vacío. Los pendientes históricos de mapeo A13-008, instrumentación, R3/R4 y tests físicos **no son funcionalidades aprobadas pendientes de promoción** ni se convierten en PASS.
- El README raíz y la cabecera del benchmark F137 contenían párrafos cronológicamente obsoletos: corregidos en esta rama documental; las secciones históricas continúan explícitas. `PRE_RELEASE.md` y el checklist mantienen estado previo a publicación.
- `knowledge/main` en `28ac12e0` tiene lección F137 y PR #27 merged, pero `OPERATING_KNOWLEDGE.md` conserva párrafos Draft anteriores al merge; sincronizar por **nuevo PR privado**, no tocar el PR #28 de Alpha15.
- Exclusiones: `OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`, `TFT_NEW_FEATURES=OUT_OF_SCOPE`. No mover trabajo a Alpha15 sin planificación separada.
- Pendientes **aceptados y no PASS**: R3 interno y churning concurrente, R4 1805 s, renovación y rebind DHCP/DNS, Basic Core físico, E1 `PENDING_GDB_UNAVAILABLE`, E2 `PENDING_SDKCONFIG_NOT_LOCATED`, literal UART BOOT F137 no capturado, long-term SPI starvation y ausencia de ensayo RJ45 nuevo para F137.

## C-RELEASE-LIKE — contrato y criterio de parada

**Validación release-like ejecutada con éxito en [CI #39641744857](https://github.com/JW-Control/platform-jwplc/actions/runs/39641744857), commit `4aa3c82a` (todos sus pasos SUCCESS; sin skipped):** ejecutar `JWPLC/tools/jwplc_release.py prepare` con los mismos argumentos de Release real, índices semilla de `main`, `--recreate-zip --archive-root-mode folder --version 2.1.0-alpha.13`. Prohibidas APIs `gh release create`, `git push`, upload COM, merge y escritura en índices remotos.

El gate debe demostrar en artifact `MANIFEST.json`: (1) HEAD exacto y tree esperado; (2) ZIP `jwplc-esp32-2.1.0-alpha.13.zip`, única raíz `2.1.0/`; (3) todos los bytes de entradas de ZIP idénticos al source de `JWPLC/2.1.0`, SHA-256 y tamaño; (4) dos `.a` con SHA canónicos y `library.properties` con `precompiled=full`; (5) índice dev simulado apuntando al ZIP generado, índice estable **sin cambios**; (6) instalación aislada/extracción del ZIP en sketchbook Arduino CLI, selección de **exactamente** el package extraído en ambos FQBN `jwplc:esp32:jwplcbasic`/`jwplcbasiccore`; (7) compilación de consumidores representativos, `-L/-l`, ELF real y ausencia de recompilación de los `.cpp` de las dos librerías.

**Evidencia comprobada (run #39641744857):** `C_SOURCE_TREE=PASS_IDENTICAL_TO_F137_N3`; `C_NO_FIRMWARE_CHANGE=PASS`; `C_RELEASE_GENERATOR=PASS_NO_GITHUB_PUBLISH`; `C_ZIP_STRUCTURE=PASS_UNIQUE_ROOT_ALL_SOURCE_BYTES`; `C_ARCHIVES=PASS_TWO_SHA256_LIBRARY_PROPERTIES`; `C_INDEX_SIMULATED=PASS_DEV_ONLY_STABLE_OMITTED`; `C_CLI_STAGE=PASS_ZIP_EXTRACTED_IN_ISOLATED_DATA`; `C_CLI_COMPILE_jwplcbasic=PASS_ELF_LINK_ARCHIVES`; `C_CLI_COMPILE_jwplcbasiccore=PASS_ELF_LINK_ARCHIVES`. ZIP: `jwplc-esp32-2.1.0-alpha.13.zip`, **1 992 archivos**, **24 522 767 bytes**, SHA-256 **`02dfbc70053b652a1c2df202070573bb15abcb4ed4ab000e8699ed5f5643c783`**. Estos bytes son del ZIP de **este run candidato**; el despacho final regenerará el ZIP y deberá usar su propio SHA-256 real. Artifacts del run: `MANIFEST.json`, logs y ZIP. No se creó Release ni índice remoto.

**Segundo workflow legacy disparado por el cambio de STATUS:** [CI G10-R #39641744816](https://github.com/JW-Control/platform-jwplc/actions/runs/39641744816) terminó FAILURE en Windows/Ubuntu en el **scope preflight** al interpretar el PR global como overlay experimental G10-R (F134), con etapas dependientes SKIPPED; no es una regresión de producto. Se acotó su `if:` a ramas experimentales G10-R/H0-F1, **sin relajar la whitelist**; para un PR Alpha13-C su job debe ser `SKIPPED_NOT_APPLICABLE`, nunca PASS de la función G10-R. Validar esa clasificación en el siguiente run.

**Matiz de instalación:** el staging local del ZIP en sketchbook **no** equivale a una instalación desde el índice dev publicado. Eso queda `POSTPUBLISH=NOT_RUN` hasta un asset y entrada de índice reales. Tampoco acredita Arduino IDE gráfico ni upload. Una CI con pasos `skipped` críticos se considera REVIEW/FAIL, no PASS.

## Salida y permisos

```text
ALPHA13=OPEN
C_AUDIT=PASS_SCOPED_INVENTORY_PENDING_OPERATOR_PR_MERGE
C_RELEASE_LIKE=PASS_39641744857_SHA_4aa3c82a
PENDIENTE_DE_PROMOCION_G10=EMPTY
PENDIENTE_DE_PROMOCION_F137=EMPTY
PENDIENTE_DE_PROMOCION_GLOBAL=EMPTY_AUDITED_SCOPE
BASE_SYNC=PENDING_PR_MERGE
KNOWLEDGE_SYNC=OK_F137_MAIN__NEW_DOC_PR_PENDING
NEXT_FAILURE_ID=F138
NEXT_GATE=ALPHA13_C_GLOBAL_AUDIT_AND_RELEASE_SOURCE_APPROVAL
RELEASE_DISPATCH=NOT_AUTHORIZED
POSTPUBLISH=NOT_RUN
```

Tras CI en verde, presentar SHA/hashes y solicitar **separadamente** autorización del operador para merge y workflow `Release JWPLC Arduino Package` desde la base con `version=2.1.0-alpha.13`. **No** declarar `CLOSED_PUBLISHED` antes de verificar GitHub Release, PR índice dev a `main`, SHA-256 del ZIP publicado e instalación/compilación desde el índice.

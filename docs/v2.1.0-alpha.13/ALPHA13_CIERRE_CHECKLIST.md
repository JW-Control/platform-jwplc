# Alpha13 — Checklist de cierre técnico y publicación

**Versión:** `v2.1.0-alpha.13`. **Rama base del alpha:** `v2.1.0-alpha.13/feature/cleanup-robustness`. **Fuente de verdad:** `ALPHA13_STATUS.md` en esta rama. **NO actualizar histórico de Alpha30 ni asumir que el package de Alpha13 ya se publicó.**

## Gate F137 (archives Ethernet/Modbus TCP) — cierre técnico H y preparación C, 21/10/2026

- [x] N2 y mediciones con 3 pares por SO/FQBN; fuente, objeto, archive y ELF, política weak/strong según Basic/Core; N3 package real 13/13 Windows+Ubuntu PASS ([#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047)).
- [x] Los dos `.a` y `precompiled=full`, junto con política weak/strong Basic/Core y ocho README, **integrados en base Alpha13** mediante [PR #125 MERGED](https://github.com/JW-Control/platform-jwplc/pull/125); autoload y periféricos preservados.
- [x] CI de Master y Slave con archivos reales: [#39622285204](https://github.com/JW-Control/platform-jwplc/actions/runs/39622285204) PASS Windows y Ubuntu.
- [x] Operador autorizó y ejecutó upload dual en COM14 Slave y COM4 Master de desarrollo, ambos `COMMAND_SUCCESS`; backup exento solo para dichas placas.
- [x] UART: dos enlaces Ethernet activos, IP DHCP privadas distintas, RTU Master-Slave con contadores correctos, Master SD/FRAM/RTC/IO/TFT observados.
- [x] FC03 read-only por Ethernet a ambos PLC: 4 registros correctos cada uno y IP distintas.
- [x] Eco UDP COM4: 3/3 datagramas de prueba correctos.
- [x] Dictamen H `PASS_FUNCIONAL_ACOTADO` con límites: `A13_F137_H_DICTAMEN_FISICO_20261021.md`. No falsear boot marker UART no capturado, Basic Core físico, E1/E2, R3 interno, R4 largo, renovación DNS/DHCP o RJ45 de este F137.
- [x] **F137-C read-only:** [CI #39628750096](https://github.com/JW-Control/platform-jwplc/actions/runs/39628750096) y replay corregido [#39629312465](https://github.com/JW-Control/platform-jwplc/actions/runs/39629312465) **SUCCESS global Windows+Ubuntu**, paquete N3 tree `cc17716f173509b1e0f3c0df5eb99549014c1d61`, 14 paths exactos, dos SHA-256 de archives, política Basic/Basic Core weak/strong PASS. El guard admite squash/referencia SHA de árbol sin exigir disponer del commit histórico N3.
- [x] **Reconciliar e integrar PR #125 en base Alpha13**, autorizado por propietario, resultado MERGED squash `23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`; revisión de tres documentos divergentes, historia preservada y árbol package `cc17716f173509b1e0f3c0df5eb99549014c1d61` idéntico a N3.
- [x] Confirmar en **base** las seis rutas funcionales F137 (2 CPP, 2 `.a`, 2 `library.properties`) y 8 README, `precompiled=full`, SHA-256 canónicos, tree exacto N3; `PENDIENTE_DE_PROMOCION_F137=EMPTY`, `BASE_SYNC=OK`.
- [ ] Comprobar que `PRE_RELEASE.md`, README, source SHA y ZIP de publicación correspondan **a la rama base actualizada**, nunca publicar base antigua ni binarios de runner CI.
- [x] Cierre técnico C-F137: guard source SHA Windows/Ubuntu PASS y límites físicos explícitos; [PR privado #27](https://github.com/JW-Control/jwplc-dev-mcp/pull/27) MERGED a `knowledge/main`, `KNOWLEDGE_SYNC=OK`.
- [ ] Auditoría global Alpha13 y comprobaciones release-like completas del package ya integrado (incluido source SHA real al despacho), checklist de distribución e instalación publicada.
- [ ] No marcar `ALPHA13=CLOSED_PUBLISHED` hasta fusionar con autorización y verificar GitHub Release, SHA ZIP, índice dev y Arduino CLI/IDE de **package publicado**.

**Estado:** `F137_H=PASS_FUNCIONAL_ACOTADO`, `F137_C=PASS_MERGED_BASE`, `BASE_SYNC=OK`, `KNOWLEDGE_SYNC=OK`, `RELEASE=BLOCKED_PENDING_SEPARATE_AUTH`.

---

## Integración técnica de G10

- [x] Merge funcional [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124) a base, `3467a375ccaf7c7ee27220ed0bc348056e5e8219`.
- [x] Inventario de 10 cambios + 2 SafeAccess nuevos; `PENDIENTE_DE_PROMOCION_G10=EMPTY`.
- [x] F1 SS de W5500 desde `JWPLC_ETH_CS=5` y guarda GPIO6–11 conservados.
- [x] Autoload íntegro, APIs previas preservadas; no cambiar archives, FlashFreq, bootloader, GPIO4, OpenPLC ni HMI.
- [x] N0, N1 nativo/paridad, N2 ambos FQBN y N3 11/11 sobre código productivo idéntico.
- [x] PH0 doble placa package real, upload, RTU real, dos IP DHCP y FC03 TCP 2/2.
- [x] R1 605 s, 589 rondas TCP ambos + UDP eco, UART posventana RTU y periféricos.
- [x] R2 recuperación de red 3/3 + UART con dos caídas/recuperaciones PHY reales.
- [x] R3 churn TCP externo **reutilizado de R1**; alcance interno pendiente documentado.
- [x] R4 de 1805 s **no ejecutado y expresamente aceptado como excepción**, no marcar PASS.
- [x] E1/E2 y DNS/DHCP renewal documentados como cobertura pendiente, sin falsear PASS.
- [x] F135 causa del verificador `Sketch.ino.elf/.bin` registrada; sin nuevo fallo productivo.
- [x] `BASE_SYNC=OK` tras integración a base.
- [x] Sincronización de conocimiento F135/G10-H al privado `knowledge/main` por [PR #25](https://github.com/JW-Control/jwplc-dev-mcp/pull/25).

## Gate de publicación **obligatorio antes de CLOSED_PUBLISHED**

- [x] Notas de PreRelease en español versionadas (este directorio).
- [ ] Auditoría del **cierre global Alpha13**, incluidos hallazgos de cleanup no relacionados con G10, y aceptación final de excepciones (fuera de alcance nuevo).
- [ ] Confirmar en la rama de publicación el **source exacto**: no fusionar ciegamente `release/v2.1.x` con 1095 commits adelantados de Alpha13; comparar árbol de `JWPLC/2.1.0` y documentar SHA.
- [ ] Confirmar que el flujo de publicación usará el package exacto de Alpha13 y `PRE_RELEASE.md`, y que la rama seleccionada contiene el README destinado al índice de `main`.
- [ ] Verificación final release-like de Arduino CLI/IDE, binarios fuente→archivo→ELF, selección de las librerías oficiales y sketch mínimo; no inventar PASS del instalador distribuido.
- [ ] Ejecutar workflow **Release JWPLC Arduino Package**, `version=2.1.0-alpha.13`, apuntando a la rama de contenido auditado. Nunca republicar mismo tag/asset sin revisión.
- [ ] Confirmar ZIP raíz única `2.1.0/`, SHA-256, tamaño y assets/tag.
- [ ] Revisar PR automático del índice dev hacia `main` y README; no modificar índice estable de `v2.0.0`.
- [ ] Instalar **package publicado** desde dev index limpio en Arduino IDE y Arduino CLI; compilar, subir y observar hardware o registrar excepción explícita aceptada.
- [ ] Completar metadatos reales de `PRE_RELEASE.md`, actualizar README público y `ALPHA13_STATUS.md` con `ALPHA13=CLOSED_PUBLISHED` solo tras todos los verificables.
- [ ] Registrar `PENDIENTE_DE_PROMOCION`, `BASE_SYNC`, `KNOWLEDGE_SYNC`, `NEXT_FAILURE_ID`, siguiente alpha, y decisión de no reabrir gates ya cerrados.

```text
G10_IN_ALPHA13_BASE=YES
G10_PHYSICAL=ACCEPTED_WITH_R3_R4_DISCLOSED_RISKS
RELEASE_SOURCE_SHA=PENDING
RELEASE_BRANCH_READINESS=PENDING
ALPHA13_PRERELEASE=NOT_PUBLISHED
ALPHA13_TECHNICAL_FINAL_AUDIT=PENDING
ALPHA13_GLOBAL_STATUS=OPEN
```

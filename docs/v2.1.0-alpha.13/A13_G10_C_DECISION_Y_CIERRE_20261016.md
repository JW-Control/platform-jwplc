# Alpha13 G10-C — decisión técnica y cierre documental

**Fecha:** 16 de octubre de 2026 (Perú).  
**Naturaleza:** cierre de **decisión**, no nueva campaña técnica ni integración al producto.  
**Rama documental aislada:** `codex/alpha13-g10-c-decision-closure`, derivada del cierre G10-H `268aee73cbcd6b1aa7ec81ddb52d44390a61663e`.  
**PRs de origen:** [#105 — G10-I](https://github.com/JW-Control/platform-jwplc/pull/105) y [#106 — G10-H](https://github.com/JW-Control/platform-jwplc/pull/106), ambos Draft.

## 1. Decisión

**`G10-C=CLOSED_DECISION_DEFER_ADOPTION`.**

La candidatura integrada Ethernet/W5500/Modbus TCP supera sus pruebas N1–N3 de integración y el **smoke físico G10-H definido y ejecutado**. Sin embargo, **no existe todavía cobertura física suficiente de recuperación/fallo y ciclo de vida de sockets para adoptar el overlay en el package productivo**. En consecuencia:

- **`A13-006=ADOPTION_BLOCKED`**, no `FIXED` en producto. El comportamiento candidato probado y el estado de adopción son dos afirmaciones distintas.
- **`G10-FINAL=NOT_CLOSED_FOR_PRODUCT`**, porque la decisión de diferir no constituye un cierre de regresión/adopción final.
- **`G10-I=NONPHYSICAL_PASS_CANDIDATE`** y **`G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED`**: mantener sus PASS, sin reejecutarlos por inercia.
- **`PRODUCT_MODIFIED=NO`**, **`AUTOLOAD_NORMAL=UNCHANGED`**, **`FROZEN_ARCHIVES=5_UNCHANGED`**, **`GPIO4_RESET=DISABLED`**.
- **`MERGE=NOT_AUTHORIZED`**, **`RELEASE=NOT_AUTHORIZED`**, **`PR_105_AND_106=DRAFT`**.
- **`OPENPLC=OUT_OF_SCOPE`**, **`HMI_DESIGNER=OUT_OF_SCOPE`**, **`TFT_NEW_FEATURES=OUT_OF_SCOPE`**.

Esta decisión está alineada con [el diseño aprobado de hitos G10-I/H/C](A13_G10_INTEGRATED_MILESTONES_20261013.md): **G10-C es documentación, conciliación de evidencia y decisión de adopción**, no otro gate físico. No abrir una sucesión de microgates para repetir pruebas anteriores.

## 2. Evidencia positiva reutilizada sin reconstrucción de firmware

| Nivel | Fuente / identidad | Conclusión comprobada | Límite |
| --- | --- | --- | --- |
| N1–N3 G10-I | HEAD `a954cabd9c6e37876b59995a41ff0a449308ccd1`; [CI #38843599043](https://github.com/JW-Control/platform-jwplc/actions/runs/38843599043) | 5/5 jobs SUCCESS: nativo Linux/Windows, Arduino selectivo, **N3 11/11 baseline y 11/11 candidato** | Overlay experimental; no producto distribuido, no IDE local demostrado |
| Preparación G10-H | HEAD ejecutable `6672ddbeddee7ae0db298596b802a79ab021114c`; [CI #38850064913](https://github.com/JW-Control/platform-jwplc/actions/runs/38850064913) | 3/3 jobs SUCCESS: stage íntegro Windows, compilación COM4 y COM14 sobre FQBN temporal en Windows y Ubuntu | Sin hardware ni upload durante CI |
| H físico autorizado | HEAD local `c776ab92388e8765e933bf50cab8215e7c18325b`; `g10_h_20261015_234401_713114fc` | `PASS_DUAL_ETHERNET_SPI_RTU`; `exit_code=0`, ambos uploads confirmados | Smoke ~40 s, no ensayo de recuperación o endurance |
| Fuente G10-H | [Informe del banco](A13_G10_H_COM4_COM14_BENCH_20261015.md) | `SUMMARY.log` y `MANIFEST.json` copiados por el operador al chat, revisados | Archivos de log serial no recibidos ni versionados |

**Cifras físicas finales** extraídas del manifiesto presentado: TCP `47 OK / 0 FAIL`; UDP `47 OK / 0 FAIL`; COM4 Master RTU `49 OK / 0 FAIL`; COM14 Slave `52 REQUESTS / 0 CRC`; microSD `56 OK / 0 FAIL`; FRAM `56` sondeos de lectura; RTC `1` válido; botonera `6` eventos; `40` heartbeats. IP COM4 `192.168.0.31`. Los contadores UDP `45` del último heartbeat son un snapshot anterior a los `47` ecos del cliente PC; no son contradictorios.

**Procedencia física:** `jwplc_hbench:esp32:jwplcbasic`, overlay de 14 fuentes, core staged SHA-256 `a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d`, objeto SafeAccess SHA-256 `817d04ea874eae60e85447f65fa3c0c4c483bd4e6465959c91a792300ed52833`; ELF COM4 `cc2636518f4bac2823ecfb924e8c07a91a0e8c2a88431cc0542d924291ad61db`, ELF COM14 `82b2b4d85e3113ed555699d5a15f53610c75568198a630044a3a3921c9d70890`.

## 3. Matriz de adopción: acreditado y pendiente

| Aspecto | Estado para decisión de G10-C | Acción necesaria si se retoma adopción |
| --- | --- | --- |
| Compilación/enlace fuente→.d→.o→ELF, APIs, autoload representativo | PASS en CI candidata y banco | Reutilizar evidencia si código/flags/core/archives no cambian |
| Concurrencia corta TCP/UDP + RS-485/RTU + SD/FRAM/RTC/botonera | PASS físico delimitado | No repetir smoke sin nueva hipótesis |
| DNS/DHCP en red real ante pérdida/recuperación | **PENDIENTE** | Ensayo diseñado y autorizado con desconexión/reconexión controlada |
| Socket epochs: TCP client/server, varios sockets, timeouts y `close` diferido bajo `SPI_BUSY` físico | **PENDIENTE** | Pruebas de vida y contención con observables verificables |
| Fairness con TFT, TCA/I/O y resto del autoload bajo estrés/reentrada | **PARCIAL** | Instrumentación física; el smoke no prueba starvation ni rendimiento |
| Power-cycle/watchdog/reset y recuperación sin GPIO4 | **PENDIENTE** | Plan seguro separado, sin activar el reset del W5500 por defecto |
| Compatibilidad Arduino IDE local y package instalable | **NO DEMOSTRADO EN ESTE GATE** | Prueba IDE y matriz de instalación/distribución antes de release |
| B01: APIs escalares `connected/status/socketStatus/operator bool` ante `SPI_BUSY` | **LIMITACIÓN CONOCIDA** | Consumidores que requieren certeza deben usar probes triestado; no afirmar semántica nueva de API legacy |
| Código dentro del package productivo / variante instalada | **NO ADOPTADO** | Plan de integración controlada, diff, gate N3/físico según impacto y permiso de merge |
| Programa de aplicación de las dos placas de banco | **NO RESTAURADO** | Operador debe restaurarlo antes de conectar actuadores o máquinas |

**No extrapolar** los contadores `0 FAIL` a cero fallos bajo carga industrial, EMC, pérdida de cable o funcionamiento prolongado. El uso de 5 archives congelados **no** autoriza publicar un bootloader definitivo, fijar FlashFreq final ni declarar OTA soportada.

## 4. Checklist de cierre documental G10-C

- [x] Reconciliados HEAD de G10-I/G10-H y CI vinculadas a cada código.
- [x] Verificados `SUMMARY.log` y `MANIFEST.json` textuales del smoke real, con contadores de PC y Slave.
- [x] Identificados los dos uploads autorizados y la falta de rollback automático.
- [x] Reconciliada selección de plataforma de banco frente a instalación `jwplc_local 2.1.0-dev`, sin modificar Arduino15.
- [x] Aclarado que los 5 archives no cambiaron y que el package productivo no incorporó el overlay.
- [x] Registradas las limitaciones de B01, red en fallos, sockets/epoch y entorno Arduino IDE.
- [x] Decisión conservadora: **diferir adopción**, preservando resultados PASS.
- [x] Alcance excluido: OpenPLC, HMI Designer y nuevas funciones TFT.
- [ ] Trazas brutas `SERIAL_COM4.log` y `SERIAL_COM14.log` archivadas/verificadas de forma independiente (no imprescindible para validar el manifiesto, sí para reproducibilidad ampliada).
- [ ] Suite separada de recuperación, DNS/DHCP y endurance, **solo si se decide reabrir adopción**.
- [ ] Validación IDE/producto final y aprobación explícita de merge/release.

## 5. Próxima decisión — fuera de G10-C

Si se decide buscar adopción en un hito posterior, preparar una **campaña consolidada de robustez física** (provisionalmente `G10-R`) y someter su plan a aprobación; no convertirla silenciosamente en una condición retrospectiva del smoke ya cerrado.

Propósito propuesto: red DHCP y DNS bajo fault injection controlada; retirada/reconexión RJ45; varios sockets TCP y clientes concurrentes; cierre diferido, lifecycle y epoch; fairness SPI; recuperación sin GPIO4; y una ventana de estabilidad prolongada. **Ninguna de esas pruebas está autorizada a realizar reset, power-cycle, upload, cambiar cableado o accionar cargas por este documento.** Exigir criterios cuantificables, procedimiento de abortado y consentimiento humano independiente antes de operaciones físicas.

Mientras tanto, el resultado de este documento permite terminar el **hito de decisión G10-C** con estado `CLOSED_DECISION_DEFER_ADOPTION`, sin afirmar que el producto o Alpha13 estén cerrados.

```text
G10-I=NONPHYSICAL_PASS_CANDIDATE
G10-H=PASS_PHYSICAL_SMOKE_MANIFEST_REVIEWED
G10-C=CLOSED_DECISION_DEFER_ADOPTION
A13-006=ADOPTION_BLOCKED
G10-FINAL=NOT_CLOSED_FOR_PRODUCT
NEXT_GATE=G10-R_PLAN_IF_PRODUCT_ADOPTION_IS_REQUESTED
PRODUCT_MODIFIED=NO
PR_105=OPEN_DRAFT
PR_106=OPEN_DRAFT
MERGE=NOT_AUTHORIZED
RELEASE=NOT_AUTHORIZED
```

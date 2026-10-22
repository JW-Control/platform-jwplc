# Alpha13 — H0-F1: campaña extendida 10 EN + 10 power-cycle (COM4)

**Autorización del propietario:** 19/10/2026 — «Aprobado F1. Pasamos a la campaña extendida y luego la integración G10».  
**Estado:** **AUTORIZADO / NO EJECUTADO** (hasta recibir evidencia del operador).  
**Placa única:** JWPLC Basic de desarrollo **COM4 Master**. **COM14 NO_TOUCHED**.  
**Firmware ya instalado:** rama base Alpha13 HEAD \`2b5802720df552e01a0b4f9d4fe1a7bd2de16daf\`; sketch \`tools/alpha13/h0_f1_com4/A13_H0_F1_SMOKE.ino\`. No \`git pull\`, compilación, upload ni alteración del package para ejecutar esta campaña.

## 1. Antecedentes físicos: no repetir

- CI F1 source-first N0–N3, y F132 CI+compilación Windows local PASS.
- Upload previo solo COM4 PASS, con \`H0F1_EARLY_LINK_RETURN=2\`, \`STAGE=2\`, PHY real.
- BOOT \`6BCAE2DA\`: >15 min estables confirmados, contador hasta 1600 (~26m40s de uptime estimado), LinkOFF→LinkON, y una desconexión/reconexión RJ45 \`1→2→1\` en el mismo BOOT sin watchdog observable.
- \`rst:0x1 POWERON_RESET\` anterior: **reinicio manual confirmado**.
- Todo lo anterior es F131/H0 principal PASS; la campaña de 20 ciclos todavía no se ha ejecutado.

## 2. Preflight físico antes de cada fase

Banco propio aislado: sin cargas/tensiones en relés Q; sin maquinaria ni red industrial; alimentación y USB seguros; únicamente COM4; pantalla y periféricos integrados intactos. **RJ45 desconectado durante los 20 ciclos** para comprobar la causa inicial H0 (consulta temprana W5500 sin link) sin sumar variables. No tocar GPIO4 ni ejecutar \`ESP.restart()\`/reset por firmware. Cerrar programas que pudieran abrir COM4 en paralelo y no confundir un reset de DTR/RTS del monitor serie con una maniobra EN/power intencional. Registrar manualmente todo reset adicional.

**No escribir firmware, no ejecutar esptool, no instalar core, no resetear COM14.** Una apertura del monitor o del puerto COM tras volver a energizar puede ocultar las primeras líneas UART o incluso generar un reset adicional; registrar \`BOOT\`, el motivo del reset observado y cualquier captura incompleta.

## 3. Fase EN — diez reinicios manuales consecutivos

1. Mantener el USB/alimentación en servicio y el monitor COM4 a **115200** abierto. Confirmar \`LINK=2\`, \`STAGE=2\`, contador progresivo antes de empezar.
2. Por cada ciclo **EN-01 a EN-10**, pulsar una vez **EN/reset hardware del ESP32**, soltar, y esperar cinco mensajes \`H0F1_READY\` sucesivos del **nuevo BOOT** antes del siguiente. No realizar maniobras solapadas.
3. Guardar la línea \`H0F1_BOOT=<nonce>\`, \`H0F1_PREVIOUS_STAGE\` si se capturó, \`H0F1_EARLY_LINK_BEGIN\` y \`H0F1_EARLY_LINK_RETURN\` si fueron visibles; más la secuencia de cinco contadores \`COUNT=n..n+4 STAGE=2 LINK=2\`. El número de BOOT debe ser nuevo respecto al ciclo anterior; no exigir un valor fijo para \`PREVIOUS_STAGE\`.
4. Si falta el comienzo porque el monitor reconectó, registrar **CAPTURE_GAP**. Un \`STAGE=2\` nuevo implica que la llamada temprana retornó en ese arranque, según el sketch source-backed, pero no se inventa la línea \`EARLY_LINK_RETURN\` que no se grabó.

**Aceptación EN:** 10 maniobras observadas con arranques nuevos, \`STAGE=2\`, \`LINK=2\`, cinco heartbeats consecutivos sin WDT/panic ni reset espontáneo. Para cierre estricto, distinguir la evidencia directa \`EARLY_LINK_RETURN=2\` de los ciclos acreditados indirectamente por \`STAGE=2\`.

## 4. Fase POWER — diez cortes completos de alimentación

1. Mantener **RJ45 desconectado**, sin alimentación alternativa ni señales externas que puedan alimentar parcialmente la placa. Si USB es la única fuente, cortar la alimentación retirando el USB de la placa de banco y mantener **al menos 5 segundos** antes de reconectar. Si existe otra fuente, comprobar que se desenergizó por completo; no confundir una pulsación EN con un power-cycle.
2. Para cada ciclo **PW-01 a PW-10**, reconectar solamente COM4/alimentación, comprobar que el puerto regresa al rol esperado y abrir/reabrir el monitor **115200** cuando sea necesario; esperar cinco \`H0F1_READY\` continuos. No cambiar a otro COM porque Windows reasignó el número: en ese caso **REVIEW_STOP** hasta reconocer físicamente la placa.
3. El monitor serie puede no capturar el comienzo si el USB se desconectó. **No sustituir las líneas faltantes**. Registrar \`BOOT\` nuevo, \`STAGE=2 LINK=2\`, progreso \`COUNT\`, y si se capturó \`H0F1_EARLY_LINK_RETURN=2\`. Registrar cuando la apertura del monitor provoque un reset adicional y NO contarlo como power-cycle distinto.
4. Tras cada ciclo documentar OK/REVIEW_STOP/FAIL antes de iniciar el siguiente.

**Aceptación POWER:** diez desenergizaciones completas verificadas por el operador, diez recuperaciones de COM4, diez BOOT nuevos y estables (\`STAGE=2 LINK=2\`, cinco contadores), sin TG1WDT, panic, boot loop, salida Q inesperada ni reinicio extra inexplicado.

## 5. Interrupción obligatoria

Detener en el primer \`TG1WDT_SYS_RESET\`, \`rst:0xf BROWNOUT\` u otro reset inesperado, \`H0F1_REVIEW_STOP_PREVIOUS_EARLY_LINK_NOT_RETURNED\`, \`EARLY_LINK_BEGIN\` sin \`RETURN\` y sin \`STAGE=2\`, \`LINK=0\` persistente, COM inesperado, puerto ocupado, ausencia de cinco heartbeats, anomalía eléctrica/térmica o carga activa en Q. **No repetir automáticamente el ciclo fallido ni reflashear.** Conservar los fragmentos de UART y una nota del contexto físico.

Un \`rst:0x1 POWERON_RESET\` justo después de un corte voluntario **no** es falla por sí mismo. No aceptar WDT como reset manual.

## 6. Formato de registro y cierre

Capturar localmente en archivo fuera del repositorio si es posible; compartir al asistente tabla compacta de **20 filas**, sin MAC/PnP ni archivos binarios. Usar \`tools/alpha13/h0_f1_com4/CIERRE_COM4_PLANTILLA.md\` y este contrato.

| Fase | Ciclos | Resultado por ciclo |
| --- | --- | --- |
| EN | 01–10 | BOOT, retorno temprano directo/indirecto, STAGE, LINK, cinco COUNT, resets imprevistos, PASS/REVIEW_STOP |
| PW | 01–10 | Confirmación corte total, retorno COM4, BOOT, STAGE, LINK, cinco COUNT, resets imprevistos, PASS/REVIEW_STOP |

**H0-F1 extendido = PASS** solo si las **20** filas evidencian recuperaciones acordes al contrato, sin fallos. Si las capturas carecen de etapas indispensables, declarar \`REVIEW\` y precisar la brecha sin inventar datos. Tras PASS se podrá abordar la **integración G10 productiva**, pero no se considera aprobado ningún upload físico G10-R R1–R4, modificación GPIO4 ni publicación.

\`\`\`text
H0_F1_EXTENDED_APPROVED=YES
H0_F1_EN_10=NOT_RUN
H0_F1_POWERCYCLE_10=NOT_RUN
H0_F1_HARDWARE_PORT=COM4_ONLY
COM14=NOT_TOUCHED
H0_F1_PRODUCT_HEAD=2b5802720df552e01a0b4f9d4fe1a7bd2de16daf
F131=H0_PRIMARY_PASS_EXTENDED_PENDING
F132=RESOLVED_CI_AND_LOCAL_BUILD
NEXT_GATE=H0_F1_EXTENDED_10_EN_THEN_10_POWERCYCLE_OPERATOR
AFTER_EXTENDED_PASS=G10_PRODUCT_PROMOTION_PLAN_AND_NONPHYSICAL_GATES
\`\`\`


## 7. Resultado recibido — 19/10/2026 (no confundir cobertura física con cobertura de captura)

El operador reporta **10 EN y 10 power-cycles completados**, RJ45 desconectado, sin flashear. En el archivo UART recibido se observan **12 BOOT** con `EARLY_LINK_RETURN=2` (9 con al menos 5 READY); la salida de consola adicional aporta **10 BOOT** con `EARLY_LINK_RETURN=2` (9 con 5 o más READY). No hay `TG1WDT_SYS_RESET` visible. En consola `915169EA` llega solo hasta `COUNT=3`; en el archivo existen tres ventanas que acaban en 3/4. No queda inequívocamente asignado cada BOOT a su maniobra EN/PW respectiva, por lo que **20/20 PASS estricto sigue REVIEW** por evidencia de captura (no es regresión de firmware). Se necesita reconciliar las filas faltantes o decidir explícitamente una excepción de aceptación al criterio de cinco READY. Una comprobación con RJ45 conectado (2 EN y 2 power) es **opcional, no obligatoria**, pendiente de autorización separada. No repetir 20 maniobras por inercia.

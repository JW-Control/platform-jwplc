# Alpha13 — G3 / A13-004 — finalizador de Git y documentación

**Estado:** herramienta preparada; G3 superó el gate físico integrado, pero el producto permanece local, sin commit hasta autorización explícita del operador.

## Evidencia y alcance

El gate reportó `STATUS=PASS_PHYSICAL_AND_NORMAL_CORE`: source-first, core precompilado, 3/3 compilaciones de consumidores, subida por COM4 y 150 ensayos concurrentes completados sin errores. SHA-256 del nuevo `core.a`:
`a1a985f64c22838a1987d4280b8c1dc431a42d5789c6692cbfa36dfb6252246d`.

El finalizador comprueba el `SUMMARY.log`, `MANIFEST.json`, logs de compilación y serial de la ejecución real, comparando hash de archivo, token del sketch y lecturas de registro/estado. Comprueba que solo existen **cinco archivos productivos modificados** y que los cuatro fuente coinciden byte a byte con los candidatos de G3. Impone HEAD exacto `1f1548d8dffec802771e8f08445d6a419803f87d` y remoto sin movimientos nuevos antes de iniciar cambios Git.

## Una sola ejecución protegida

Desde la raíz de `platform-jwplc`, extraer el ZIP en el mismo repositorio y ejecutar `tools/alpha13/gates/run_a13_g3_finalize.bat`. El ZIP solo contiene este Python, el BAT y este plan. Se exige que no existan otros archivos sin seguimiento, staged ni cambios productivos ajenos. No borrar artefactos ni hacer `git pull`, `reset`, `checkout`, `clean` o rebase.

El finalizador primero verifica fuentes, archivo precompilado y evidencia. Solo con éxito solicita al operador escribir `AUTORIZO_COMMIT_G3`. A continuación crea **tres commits separados**: tooling, cinco archivos productivos y documentación/checklist/fallo F110; realiza un `git push` sin `--force` a la rama Alpha13. No cambia ni sube firmware, no repite gate físico y no publica una release o PR.

En cualquier discrepancia detiene el proceso y deja logs bajo `tools/alpha13/results/g3_integrated_*/FINALIZE_GIT_SUMMARY.log`. No efectúa rollback Git destructivo si hay cambios parcialmente commiteados. En ese caso inspeccionar antes de volver a ejecutar. Al concluir: `STATUS=CLOSED_PASS` y `NEXT_GATE=A13-G4_TFT_BATCH_TASK_OWNERSHIP`.

No ampliar alcance a OpenPLC, HMI Designer ni nuevas funcionalidades TFT.

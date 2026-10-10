# Alpha13 G4 — finalizador Git, producto y documentación

Estado: TOOLING_VERSIONED_AWAITING_LOCAL_OPERATOR_CLOSURE.

La ejecución G4 reportada por el usuario concluyó con STATUS=PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE y PRODUCT_DIRTY_FILES=3. Producto local contiene dos fuentes del candidato y libJWPLC_TFT.a con SHA256 4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc. Cuatro regresiones, serial 50 ciclos sin errores, reconocimiento visual NO/SI y auditoría PASS.

Este script finalizador no compila, sube firmware ni intenta reproducir pruebas cerradas. Audita que la evidencia PASS local sea única, las fases, los logs, el binario de la prueba, el token del serial, hashes de producto y los tres archivos modificados.

## Transferencia sin ZIP ni pull

Conservando el worktree productivo modificado, usar git fetch y extraer con git show el script versionado en la rama remota hacia un fichero temporal .py fuera del repo, usando Python subprocess.check_output para preservar bytes, y ejecutarlo desde la raíz del repositorio.

El script exige exactamente un commit remoto de tooling desde bc9e7c938506bd872cbb4d943d58ef9644be6e5d y diferencias remotas limitadas a este finalizador y el documento de plan. Prepara tres etapas en una ejecución con confirmación textual AUTORIZO_COMMIT_G4: commit productivo (tres paths), merge no destructivo del tooling remoto, actualización de cierre/checklist/estado/F111 y commit documental, push no-force a la misma rama Alpha13.

No usa git pull, git clean, git reset, git checkout ni publica Alpha13. Un fallo luego del primer commit puede dejar estado Git parcialmente avanzado: no reintentar ni revertir sin diagnóstico. Evidencia adicional de cierre en tools/alpha13/results/g4_integrated_*/G4_FINALIZE_GIT_SUMMARY.log.

Después del cierre: G5 TCP correctness A13-005/A13-006. OPENPLC, HMI_DESIGNER y TFT_NEW_FEATURES continúan fuera de alcance.

## Ajuste por HEAD local incorporado automáticamente (2026-10-10)

El operador aportó captura con 3 archivos TFT productivos modificados y abortó el cierre en el guard PowerShell `HEAD local inesperado`; el finalizador **no fue ejecutado** en ese intento. El histórico de GitHub Desktop ya muestra el commit remoto `b15c59c`, por lo que el HEAD local puede estar en el baseline `bc9e7c9` o en dicho commit (sin inferirlo definitivamente de la captura).

La versión actual acepta únicamente las siguientes configuraciones verificadas contra la rama remota tras `git fetch`: (1) HEAD en `bc9e7c9`, (2) HEAD en `b15c59c` o (3) HEAD en el nuevo commit remoto del finalizador. La rama remota debe tener exactamente dos commits de tooling posteriores a `bc9e7c9`; su diferencia contra el baseline se limita exactamente a estos dos archivos `.py` y `.md`. Si el tooling ya está en el HEAD local, exige igualdad de bytes y omite el merge cuando procede. En todo caso preserva los 3 archivos productivos, exige evidencia única G4 PASS y autorización expresa antes de los commits.

No hacer `git pull`, `reset`, `checkout` ni `clean`. Ejecutar el finalizador desde `git show` a fichero `%TEMP%`; el ejecutor verifica además que sus propios bytes sean exactamente los versionados en el ref remoto validado. Conservar como frontera irreversible el primer commit producto: ante fallo posterior, solicitar revisión en vez de relanzar.

## Ajuste R3: falsa discrepancia por lectura byte-a-byte en Windows (2026-10-10)

La ejecución del operador confirmó:
`LOCAL_HEAD_ACCEPTED=b15c59cd599a1d5e7dbabc594a38ccdc0c49f791`,
`GIT_PREFLIGHT=PASS`, `PRODUCT=PASS_2_SOURCES_ARCHIVE_SHA`,
`EVIDENCE=PASS_50_CYCLES_4_REGRESSIONS_VISUAL`, y se detuvo antes de
autorizar o crear commits con
`LOCAL_TOOLING_NOT_HEAD_IDENTICAL_tools/alpha13/gates/a13_g4_finalize.py`.

El finalizador comparaba bytes crudos de `git show HEAD:path` con los bytes
del archivo de trabajo, lo que puede fallar en Windows con `core.autocrlf`
sin cambios lógicos. **No se ha demostrado una corrupción real del archivo.**

La comprobación actual usa los contratos Git de `git diff --quiet HEAD -- path`,
`git diff --cached --quiet HEAD -- path`, tracking explícito, e igualdad
del blob de índice y `HEAD:path`. El preflight sigue exigiendo únicamente
3 archivos TFT modificados y cero cambios staged/untracked. La evidencia
física y el SHA de `libJWPLC_TFT.a` se verifican sin cambios.

El nuevo historial remoto permitido desde `bc9e7c9` consta de tres commits
lineales de **tooling solamente**: `b15c59c`, `21a0816` y la reparación.
El cierre nunca sobrescribe archivos TFT sin auditar y no realiza
`reset`, `checkout`, `clean`, ni recompilación de prueba.

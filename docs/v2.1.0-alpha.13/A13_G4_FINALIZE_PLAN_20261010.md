# Alpha13 G4 — finalizador Git, producto y documentación

Estado: TOOLING_VERSIONED_AWAITING_LOCAL_OPERATOR_CLOSURE.

La ejecución G4 reportada por el usuario concluyó con STATUS=PASS_PHYSICAL_AND_REBUILT_NORMAL_ARCHIVE y PRODUCT_DIRTY_FILES=3. Producto local contiene dos fuentes del candidato y libJWPLC_TFT.a con SHA256 4c606ba66e29c1337d42450a29fd09fe76c6988f72aa51d3882b81db59f82ecc. Cuatro regresiones, serial 50 ciclos sin errores, reconocimiento visual NO/SI y auditoría PASS.

Este script finalizador no compila, sube firmware ni intenta reproducir pruebas cerradas. Audita que la evidencia PASS local sea única, las fases, los logs, el binario de la prueba, el token del serial, hashes de producto y los tres archivos modificados.

## Transferencia sin ZIP ni pull

Conservando el worktree productivo modificado, usar git fetch y extraer con git show el script versionado en la rama remota hacia un fichero temporal .py fuera del repo, usando Python subprocess.check_output para preservar bytes, y ejecutarlo desde la raíz del repositorio.

El script exige exactamente un commit remoto de tooling desde bc9e7c938506bd872cbb4d943d58ef9644be6e5d y diferencias remotas limitadas a este finalizador y el documento de plan. Prepara tres etapas en una ejecución con confirmación textual AUTORIZO_COMMIT_G4: commit productivo (tres paths), merge no destructivo del tooling remoto, actualización de cierre/checklist/estado/F111 y commit documental, push no-force a la misma rama Alpha13.

No usa git pull, git clean, git reset, git checkout ni publica Alpha13. Un fallo luego del primer commit puede dejar estado Git parcialmente avanzado: no reintentar ni revertir sin diagnóstico. Evidencia adicional de cierre en tools/alpha13/results/g4_integrated_*/G4_FINALIZE_GIT_SUMMARY.log.

Después del cierre: G5 TCP correctness A13-005/A13-006. OPENPLC, HMI_DESIGNER y TFT_NEW_FEATURES continúan fuera de alcance.

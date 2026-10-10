# Alpha13 G3 — candidato integrado de atomicidad TCA/I²C

Estado: **tooling versionado, candidato NO adoptado en producto; gate real pendiente**.

## Evidencia base y objeto

El usuario confirmó A13-G3-P0 con `PASS_BASELINE_RACE_REPRODUCED`, sin cambios productivos ni pruebas físicas. Baseline exacto: `c33aff2bad3c3a94e988ae030ceb99ad241c9eef`.

El riesgo afecta: (1) `jwplcI2C_updateBit`: lectura y escritura con locks independientes; (2) shadow del driver TCA; (3) coherencia del shadow `JWPLC_IO` después de operaciones concurrentes.

## Cambios candidatos — NO en fuentes productivas

El commit de preparación anterior `a0f38e73542225aec433dde706ac1064d81c993e` contiene cuatro reemplazos completos, bajo `tools/alpha13/candidates/g3`:

- `jwplc_i2c_bridge.cpp` y `.h`: entrada/salida interna de mutex recursivo + RMW cubierto por una única adquisición.
- `peripheral-tca6424a.c`: serialización de init, writePin y writeBank respecto al shadow; un I²C fallido invalida el shadow.
- `jwplc_peripherals.cpp`: writeOutputs, digitalWrite y su shadow protegidos por el mismo mutex recursivo.

Contrato público Arduino sin cambios. No se modifica la política de autoload, I/O mapping, hardware de TFT, periféricos ajenos ni APIs OpenPLC/HMI Designer. Los escritores **directos al registro TCA por fuera del driver** no quedan cubiertos por el contrato del shadow; documentado como límite.

## Ejecutor

```powershell
.\tools\alpha13\gates\run_a13_g3_integrated.bat --serial-port COM4
```

Fases agrupadas: preflight de Git/sha/status, respaldos fuera del repositorio, adopción **local reversible** de cuatro fuentes, reconstrucción usando `Build-JWPLCPrecompiledCore.ps1`, verificación de enlace normal con `Verify-JWPLCPrecompiledCore.ps1`, compilación de sketch físico con token irrepetible, tres regresiones normales de consumidores Display/LogicRuntime_UI, confirmación presencial de desconexión de cargas, upload a COM detectado, stress test de dos tareas y lectura de registro físico 0x05, auditoría del diff.

**La prueba física energiza los relés Q0_0 y Q0_1 150 veces.** No ejecutar en equipo cableado a motores, válvulas, calefacción, ni otros actuadores. Requiere confirmación textual `DESCONECTADAS` para subir. El sketch permanece en espera sin ejecutar el test hasta recibir el token serie único. Desenergiza ambos relés al finalizar.

```powershell
.\tools\alpha13\gates\run_a13_g3_integrated.bat --skip-physical
```

La variante sin hardware compila y enlaza pero no cierra físicamente G3; deja el candidato local pendiente de evaluación. **No repetir el gate automáticamente** sobre el worktree modificado de una ejecución exitosa.

En FAIL/REVIEW el gate intenta restaurar las cuatro fuentes y `core.a` usando backups bajo `%TEMP%`; preserva las evidencias. Si ocurrió upload, el firmware físico puede permanecer instalado a pesar del rollback local; registrar esta diferencia antes de otro test. No auto-commit, no push, no reset/checkout/clean.

## Resultado y cierre esperado

- `STATUS=PASS_PHYSICAL_AND_NORMAL_CORE`: reconstrucción y enlace oficiales, prueba real concurrente con 150 ciclos sin pérdidas, cinco cambios productivos locales sin commit. Solicitar observación humana y revisión de logs para el commit y cierre documental.
- `STATUS=PASS_STATIC_PHYSICAL_PENDING`: solo validación de build/link; no promover firmware.
- `STATUS=REVIEW/FAIL`: detener e interpretar clasificación; no confundir harness con producto.
- Genera `SUMMARY.log`, `MANIFEST.json`, `BUILD_OFFICIAL_CORE.log`, `VERIFY_OFFICIAL_CORE.log`, `COMPILE_G3_PROBE.log` `COMPILE_REG_*.log` y, si aplica, `UPLOAD_G3_PROBE.log` y `serial_g3.log`.

La construcción del archive no debe exigir SHA bit-a-bit histórico: el core contiene objetos con `__DATE__/__TIME__`. Se exige prueba de fuentes, link normal, hashes y cobertura de runtime.

**Cierre del hito sujeto a evidencias reales y autorización de commit.** El no-op con shadow validado y la invalidación en fallo deben conservarse.

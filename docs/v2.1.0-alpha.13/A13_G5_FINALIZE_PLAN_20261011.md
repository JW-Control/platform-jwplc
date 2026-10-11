# Alpha13-G5 — Finalizador integrado después de PASS físico

**Estado:** TOOLING_VERSIONED_AWAITING_GIT_CLOSURE, 2026-10-11.

G5 reportó `STATUS=PASS_PHYSICAL_NORMAL_SOURCE`, overlay 3/3 PASS,
`PHYSICAL_G5_PC_EXIT=0`, tres regresiones normales PASS y dos fuentes
Ethernet adoptadas localmente. Un segundo intento de iniciar el gate se
detuvo correctamente al encontrar esos dos archivos modificados; no
es un defecto de la compilación ni justifica repetir el upload.

## Seguridad y límites del ejecutor

`a13_g5_finalize.py` no compila, no sube firmware y no ejecuta G5 otra vez.
Compara los archivos productivos con sus candidatos por SHA, verifica
los archives G3/G4, la evidencia local `MANIFEST.json`, el token
serial y el archivo PC, y exige todas las fases PASS. Exige Git worktree
exactamente con las dos fuentes modificadas y nada staged/untracked.
Comprueba la fuente remota exacta del finalizador, una única nueva
revisión de tooling desde `ab269503fd25b942faecc9256b7bf35dc5ccbc03`,
padres y diff allowlisted. No compara bytes de textos checkout CRLF con
blobs de Git (F112).

Ejecutar mediante Python 3.11 con ruta de ejecutable explícita en el
terminal PowerShell de VS Code. Primero `git fetch` (nunca `pull`
con el árbol intencionalmente sucio), extraer el finalizador al TEMP
con `git show` y comprobar integridad con `git hash-object`.

Solo después de `READY_FOR_OPERATOR_AUTHORIZATION=YES` solicita
`AUTORIZO_COMMIT_G5`. El script crea un commit exacto de 2 fuentes,
integra tooling si no está incorporado, crea 3 documentos
(cierre, checklist, estado), genera commit documental y push normal
sin fuerza a la misma rama. No abre PR, merge release ni publicación.

Si falla tras el primer commit, detener y revisar el estado; **no**
resetear ni repetir ciegamente. El informe original que asignaba
A13-005 y A13-006 individualmente aún falta para corroborar mapping;
queda como pendiente documental explícito, no una conjetura.
\n## Corrección F112 preoperativa (2026-10-11)

Tras preparar el finalizador se detectó un riesgo de falso error por
comparar el hash raw del backup previo con `git show HEAD:archivo`.
El checkout Windows puede usar CRLF y el blob Git LF sin modificación
semántica. Se sustituyó esa igualdad por validación de formato SHA-256
del manifiesto y comparación contra el respaldo real de `%TEMP%`
**si aún existe**, nunca contra bytes de `git show`.

El histórico remoto admitido es únicamente:
`ab269503fd25b942faecc9256b7bf35dc5ccbc03`
→ `79116d90b7fe2fb37b68e8a30c678b1cfaf288ab`
→ el commit de reparación; solo dos paths de tooling y plan.
El HEAD local puede ser cualquiera de esos tres ancestros verificados.
No muta firmware ni altera los candidatos de la prueba G5 PASS.

# JWPLC — Flujo de colaboración por hitos

Actualizado: 2026-10-10
Estado: VIGENTE para `platform-jwplc` y siguientes alphas.
Sustituye la práctica de solicitar una respuesta del usuario por **cada microgate**, pero NO elimina comprobaciones, criterios de parada ni evidencia.

## 1. Principio

**Un hito = preparación consolidada + un ejecutor integrador + validación humana cuando haga falta + cierre documental**.

La unidad de conversación es el **hito**. Los microgates continúan como *fases automáticas internas*, con nombre, resultados y puntos de parada. No se exige una conversación separada por cada comprobación segura.

## 2. Responsabilidades

| Responsable | Actividad |
|---|---|
| Asistente/Codex | Lectura de fuentes, diseño, implementación, autoverificación del harness, pruebas ejecutables sin hardware, documentación de resultado y reversión |
| Ejecutores versionados | Preflight, hashes, compilaciones, link, tests, control de cambios, logs, clasificación, gates de hardware disponibles |
| Usuario | Aprobaciones materiales, observación visual/física, aportar equipo/COM/red, autorizar commit, PR o publicación cuando proceda |

Nunca atribuir al asistente un gate físico no ejecutado. No declarar PASS por una revisión estática. No inventar logs, hashes, resultado visual o estado de puertos.

## 3. Hito integrador

**Preparar:** consultar `ALPHA13_STATUS.md` (o estado equivalente del alpha), alcance, guardias de precompilados y árbol Git. Diseñar todo el hito y ejecutar revisión estática, pruebas de scripts y dry-runs posibles **antes** de solicitar al usuario un comando. No depender de archivos efímeros cuando exista una alternativa de prueba durable.

**Validar:** ofrecer un único ejecutor versionado (`.ps1` + `.bat`, u orquestación equivalente probada) que realice etapas en orden y aborte antes de acciones peligrosas si falla una precondición. Guardar **log por fase**, `SUMMARY.log`, manifiesto de hashes y etiqueta de clasificación `PRODUCT/HARNESS/HARDWARE/ENVIRONMENT/PRECONDITION`. Resultados de pruebas lógicas se agrupan; físicas que exijan observación requieren intervención del usuario.

**Confirmar:** solicitar únicamente la validación humana que una máquina no pueda confirmar (por ejemplo un fondo blanco/transición de TFT, el estado real de un actuador). No aceptar PASS visual sin dicha confirmación.

**Cerrar:** documentar una sola vez por hito en `ALPHA13_STATUS.md`, actualizar registro de fallos si hubo hallazgo nuevo, checklist y compromisos de siguiente hito; preparar commit/PR/pre-release en español cuando corresponda. No exigir un documento nuevo para cada subfase.

## 4. Seguridad, rollback y límites de agrupación

- Mantener periféricos de autoload completo: Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU y TCA/I/O; no quitar periféricos por rendimiento.
- No cambiar APIs públicas probadas ni sustituir archivos precompilados sin demostrar identidad fuente→objeto→archive→enlace. Arduino CLI **no equivale** automáticamente a Arduino IDE: validar las rutas relevantes.
- Antes de escribir en producto: hashes, branch, alcance exacto, backup externo y rollback comprobable. No realizar auto-commit, PR o release de firmware sin autorización del hito.
- Una falla detiene la secuencia en ese punto; etapas no relacionadas o solamente read-only pueden seguir **si no contaminan el resultado**.
- El archivo `SUMMARY.log` local es útil, **no la única fuente de verdad**. El manifiesto durable/cierre versionado y la identidad real del binario/fuentes deben permitir retomar un hito tras migración.
- Distinguir revisión del harness vs fallo del firmware, y una hipótesis vs una causa demostrada. No crear un nuevo número de fallo para el mismo incidente con reintentos.
- Si el worktree está **sucio intencionalmente**, no prescribir `git pull`, `git reset`, `git checkout`, `git clean` ni rebase por inercia. Comprobar vías de sincronización no destructivas antes de cualquier operación.
- No desplegar a dispositivos, cambiar configuración irreversible ni publicar firmware simplemente por optimizar número de mensajes.
- `OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`, `TFT_NEW_FEATURES=OUT_OF_SCOPE` para Alpha13.

## 5. Tamaño correcto de gate

Agrupar cuando las fases tienen un objetivo, una base de fuentes y criterios de parada compartidos. Separar si hay una frontera real de seguridad, decisión humana o evaluación física. Objetivo orientativo: **1 ejecución principal + 1 confirmación humana + 1 cierre** por hito, no un límite absoluto.

Preferir gates parametrizados y composables por debajo del ejecutor a una proliferación de `R1/R2/R3/P1A/P1B` con intervención manual innecesaria. Realizar preflight semántico, sintáctico y validación de entradas antes de transferir comandos. En tests hardware, loguear la selección de COM y firmware, no asumir que un puerto es estable.

## 6. Documentación y continuidad en un chat nuevo

No generar handoff adicional por defecto. El nuevo chat debe tomar `docs/<alpha>/ALPHA13_STATUS.md` como **estado canónico**, este workflow como contrato de interacción y el checklist/failures de ese alpha como controles. Un handoff separado se justifica sólo si el repositorio/estado vivo no es accesible o falta una decisión material no registrada.

Estructura mínima de la ficha del hito en `ALPHA13_STATUS.md`:
```text
HITO=
BRANCH=
HEAD_REMOTO=
WORKTREE_LOCAL=
EVIDENCIA_PASS=
BLOQUEOS=
SIGUIENTE_ACCION=
ACCION_PROHIBIDA=
REQUIERE_CONFIRMACION_USUARIO=
```

## 7. Aplicación inmediata: TFT Alpha13

```text
BRANCH=v2.1.0-alpha.13/feature/cleanup-robustness
HITO=TFT-PRE6
PRE5=PASS_FISICO_CANDIDATO
P0=PASS_ARCHIVE_TEMPORAL
P1A_R3=PASS_SOURCES_CANONICOS
P1B=PASS_ARCHIVE_RECONSTRUIDO
P2A=PASS_ADOPCION_LOCAL_4_BUILDS
P2A_PRODUCT_SHA_TFT_A=ab73b244c44ebd75d29a4eeb3cd97f5d18c08470f535eb16d55c2fdbf2310ff8
P2A_WORKTREE_LOCAL=3_ARCHIVOS_TRACKED_MODIFICADOS_SIN_COMMIT
P2A_UPLOAD=NO
NEXT_HITO=TFT-CLOSURE
NEXT_PHYSICAL=TFT-PRE6-P2B
COM4=ULTIMO_PUERTO_CONOCIDO_NO_ASUMIR_PRESENTE
F109=CLOSED
NEXT_FAILURE_ID=F110
```

**No repetir P0/P1A/P1B/P2A.** Preparar un ejecutor consolidado de la fase restante, que compruebe los tres archivos adoptados, compile y suba por puerto verificado, capture serial y solicite revisión visual del arranque limpio. Solo tras confirmación humana, ejecutar regresiones/cierre y terminar la receta de reconstrucción canónica desde el **nuevo** baseline de producto.

La rama remota puede contener **documentación posterior a P2A** no integrada en la copia local; actualizar docs remotos no convierte el cambio productivo local en commit. No crear conflictos con sus tres archivos. No anunciar como concluida la corrección del package antes del gate físico sobre archive normal y el cierre de receta+regresiones.

## 8. Retorno en el chat

Comunicar resultados, causas y siguiente comando de forma compacta. No copiar 500 líneas del harness al chat cuando puede versionarse. Solo un bloque de comandos por intervención, salvo solicitud expresa. No publicar comandos de reset/limpieza cuando exista un worktree productivo intencionalmente sucio.

## 9. Contrato Windows para Python y finalizadores Git (F112)

**Aplicación obligatoria** a Alpha13-G5 y siguientes gates de
`platform-jwplc` que usen PowerShell, Python, archivos precompilados
o commits locales. Antes de proporcionar comandos al operador:

- Resolver **un único ejecutable real** `python.exe`, validar
  `Test-Path -PathType Leaf` y `& $pythonExe --version`. Nunca
  convertir `Get-Command python` completo en una cadena, ejecutar
  `.py` por asociación ni usar WindowsApps como intérprete supuesto.
- Validar `ast.parse` del `.py` concreto y su procedencia con
  `git rev-parse ref:path` y `git hash-object $runner`. Descargar
  tooling con `git fetch` y extraerlo fuera del repositorio si el
  producto quedó modificado localmente; no usar `pull/reset/checkout/clean`.
- Leer `LOCAL_HEAD` y `REMOTE_HEAD` reales, no fijar el local desde
  una ejecución anterior. Autorizar solamente **ancestros explícitos**
  comprobados por `git rev-list` y diff remoto exactamente allowlisted.
- Para verificar archivos de texto tracked, usar limpieza lógica de
  Git (`git diff --quiet HEAD -- path`, `git diff --cached --quiet`)
  y coherencia índice/HEAD (`git rev-parse :path` frente a
  `git rev-parse HEAD:path`). No exigir igualdad de bytes crudos
  entre `git show` y el checkout Windows; puede existir LF/CRLF.
  Para `.a` y otros binarios, exigir SHA-256 byte a byte.
- Ejecutar el self-test portable versionado
  `tools/alpha13/gates/a13_finalizer_portability_selftest.py` y una
  simulación/preflight del finalizador concreto con worktree sucio
  intencionalmente, HEAD base y HEAD con tooling incorporado. El test
  debe abortar **antes del primer commit** si faltan identidades,
  ejecutable, evidencia, limpieza o autorización.
- Después de un cierre validado no volver a reconstruir/subir firmware
  por una falla exclusiva del launcher; clasificarla como harness y
  corregir el finalizador sin alterar los bytes probados.

Ejemplo seguro de llamada con Python 3.11 instalado explícitamente:

```powershell
$pythonExe = Join-Path $env:LOCALAPPDATA "Programs\Python\Python311\python.exe"
if (-not (Test-Path -LiteralPath $pythonExe -PathType Leaf)) {
    throw "Python verificado no disponible; detener."
}
& $pythonExe --version
if ($LASTEXITCODE -ne 0) { throw "Python no ejecutable." }
& $pythonExe -B $runner
if ($LASTEXITCODE -ne 0) { throw "Finalizador REVIEW; no reintentar a ciegas." }
```

La ruta es **ejemplo de entorno**, no una instalación universal
ni autorización para usar cualquier versión de Python: cada gate
declara su mínimo y verifica `sys.executable`. El historial y las
razones de esta norma están en el incidente **F112** del FAILURES
canónico de Alpha13.

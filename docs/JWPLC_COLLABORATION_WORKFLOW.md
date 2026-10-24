# JWPLC — Flujo de colaboración por hitos

Actualizado: 2026-10-24
Estado: VIGENTE para `platform-jwplc` y siguientes alphas.
Sustituye la práctica de solicitar una respuesta del usuario por **cada microgate**, pero NO elimina comprobaciones, criterios de parada ni evidencia.

## 1. Principio

**Un hito = preparación consolidada + un ejecutor integrador + validación humana cuando haga falta + cierre documental**.

La unidad de conversación es el **hito**. Los microgates continúan como *fases automáticas internas*, con nombre, resultados y puntos de parada. No se exige una conversación separada por cada comprobación segura.

## 1.1 Ramas y PR por feature (desde 2.1.0-alpha.14)

Cada hito o cambio trabaja en `v2.1.0-alpha.N/<Línea>/feature/<tema>`, creada desde la rama principal de la alpha
`v2.1.0-alpha.N/<Línea>/main`; entra por PR hacia esa principal, se revisa (CI + OK del propietario), se fusiona y
se borra. Sólo la principal va, por PR en borrador, a `release/v2.1.x`. Detalle y nombres vigentes: `CLAUDE.md` §5.

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

## 10. Finalizadores con archivos Git no rastreados (F113)

El preflight de un finalizador **no debe exigir ciegamente un worktree
sin archivos untracked** si el hito ya dejó cambios de producto validados.
Se exige verificar el conjunto exacto de archivos tracked dirty,
índice vacío, colisiones en destinos de documentación y alcance explícito
de todos los comandos `git add -- rutas`. Los archivos untracked ajenos
no deben limpiarse, modificar `.gitignore` sin evidencia, ni entrar en
commits por accidente. Registrar sus rutas y dejarlos al operador.

Se rechaza cualquier colisión con un fichero que el finalizador pretenda
crear; tras un cierre se comprueba limpieza solo de tracked y ningún
archivo staged, manteniendo los untracked ajenos intactos. El defecto
real observado en G5 está documentado como F113.

## 11. Contratos de MANIFEST y archivos generados (F114)

Antes de entregar un finalizador, comprobar todas las claves, tipos
y valores del `MANIFEST.json` contra las asignaciones reales del
generador. Nunca inferir que el diccionario SHA está indexado por
rutas de candidatos: el gate G5 utiliza rutas del **producto**.
No permitir borrados de archivos locales de `build/`; se ignoran
mediante `.gitignore` raíz. No incorporar salidas de CMake al repositorio.

## 12. Hitos integrados: commit/push automático tras PASS (2026-10-11)

**Decisión explícita del operador:** dejar de solicitar la cadena interactiva
`AUTORIZO_COMMIT_GN` para commits de cada gate. Un resultado íntegramente
validado del gate constituye autorización operativa para versionar y hacer
push **únicamente** del alcance del hito, en su rama de trabajo.

El ejecutor de cierre:
1. Audita antes de mutar Git la evidencia real, hashes, fases PASS,
   contrato de MANIFEST comprobado contra el generador, identidad de
   fuentes/archive y alcance exacto de producto.
2. Resuelve Git HEAD local/remoto y ascendencia permitida, registra los
   archivos tracked modificados, staged y untracked, verifica colisiones,
   EOL LF/CRLF y que todo commit incluya sólo rutas allowlisted.
3. Cuando todo es PASS, ejecuta commits de producto y documentación y push
   **no force** automáticamente, sin petición `AUTORIZO`. Cuando un
   preflight o prueba es REVIEW/FAIL, no hace commit.
4. Nunca ejecuta `git reset/checkout/clean` para resolver fallos,
   nunca elimina archivos ajenos, no cambia release ni publica
   PR/PreRelease sin la fase de cierre de alpha.
5. Si se requieren pruebas físicas que sustituyen el sketch, el paso de
   upload mantiene una **confirmación de condiciones seguras de banco**
   (`BANCO`/similar); esto es distinto del permiso Git y no se elimina.
6. En caso de fallo después de un commit, no reintenta automáticamente
   ni reescribe historia; conserva log y estado para recuperación.
7. Antes de mandar un finalizador al operador, su autor debe haber
   verificado sintaxis, contrato entre runner/manifiesto, pruebas
   de portabilidad F112 y escenarios base/HEAD-tooling EOL en un Git
   temporal. No enviar un finalizador no ensayado como si fuera robusto.

Esta política aplica desde G6; G1–G5 permanecen cerrados con sus
evidencias históricas. La automatización no equivale a permiso para
eludir seguridad física o publicar Alpha13.

## 13. Procedencia de Arduino CLI (F115)

Todos los gates nuevos del package Arduino JWPLC deben utilizar un
`arduino-cli.exe` **explícitamente identificado** y compatible con la
instalación de Arduino IDE del operador, no una ruta oculta por PATH
ni un fallback a `Arduino PLC IDE Tools`.

Cada gate debe registrar ruta absoluta, versión, SHA-256 del ejecutable
y core instalado `jwplc_local:esp32` antes de compilar y antes de
subir firmware. Si no existe una ruta verificada, detenerse con
`REVIEW_ENVIRONMENT` y pedir diagnóstico read-only; no escoger otra
versión/instalación de Arduino silenciosamente. Distinguir fallos de
resolución del CLI de bloqueos físicos de COM/USB y guardar el log
original de esptool. No alterar scripts históricos de gates cerrados
sin una razón de mantenimiento explícita.

## 14. PowerShell: no confundir filtros de arreglos con comparaciones escalares (F116)

Los comandos nativos invocados con `&` pueden producir varias líneas,
y PowerShell las materializa como arreglo. `-match` o `-notmatch`
con un arreglo selecciona elementos; **no** devuelve el resultado
booleano de examinar la totalidad de la salida.

Cuando sea necesario comprobar una fila, usar
`$text = (& $cli core list) -join "`n"` y después
`$text -match 'patron'`, o mejor evitar esta duplicación y dejar que
el runner Python compruebe el core exacto sobre stdout unido.
Nunca interpretar que un `-notmatch` de arreglos prueba ausencia
de `jwplc_local:esp32`. Validar el caso con varios cores instalados
antes de compartir bloques PowerShell.

## 15. Evidencia de red física: errores distinguibles (F117)

Para comparaciones UDP/TCP de PC con un dispositivo real, un único
`PAYLOAD_MISMATCH` no constituye un diagnóstico suficiente.
Registrar longitud observada y esperada, IP/puerto de origen, hash
de payload, primer índice diferente y salida serial del emisor.
Separar resultados por causa antes de decidir cambios en firmware.
Si el sketch de prueba ya se subió y permite repetir la transacción
sin actuadores, preferir diagnóstico read-only y **sin nuevo upload**.
No presentar un fallo ambiguo de prueba como defecto confirmado del
producto ni ejecutar automáticamente un cierre Git.

## 16. GitHub Actions como primera línea de validación (2026-10-13)

**Decisión de colaboración:** en desarrollo de firmware/package JWPLC,
primero ejecutar en CI los gates que no necesitan interacción con
hardware; no enviar un BAT de Windows por cada método o corrección.
Esto **complementa** los contratos de hitos (§1–§6), los commits
condicionados por evidencia (§12) y la procedencia del CLI (§13);
no elimina el gate local o físico cuando corresponde.

### 16.1 Orden de pruebas y alcance de cada PASS

| Etapa | Ejecutor preferido | Significado del PASS |
|---|---|---|
| Static/preflight | GitHub Actions Linux/Windows | Árbol, hashes, scripts, manifiestos y condiciones reproducibles |
| C++ host | GitHub Actions Linux/Windows | Contratos de concurrencia y estados **bajo mocks**; no hardware |
| Arduino CLI | GitHub Actions con toolchain explícito | Sketch y fuentes elegidas **compilan/enlazan**; no implica Arduino IDE |
| Package del branch | CI con platform/boards/precompilados de la rama en instalación temporal separada | Compilación del package probado; no equivale a la distribución instalada ni a todo el autoload |
| Instalación Windows/IDE | Operador, cuando exista candidato integrado | Compatibilidad con la instalación local, sin repetir microtests anteriores |
| Gate físico | Operador y banco autorizado | Comportamiento real, periféricos, comunicación, fairness SPI y recuperación |

Exigir **baseline vs candidato** en el mismo toolchain cuando la prueba
sea comparativa. Registrar `HEAD`, FQBN, versión/ruta del CLI, identidad
del core, selección de bibliotecas, `.a` congelados, hashes y la
procedencia **fuente → objeto → ELF/mapa**. Evitar que un core bootstrap
sea seleccionado por error en lugar del core del branch. Si el
bootstrap se utiliza solo para instalar herramientas, seleccionarlo
separadamente del package a evaluar, p. ej. `jwplc_local:esp32`.

Una CI roja no demuestra automáticamente defecto de firmware:
clasificar `PRODUCT / HARNESS / ENVIRONMENT / PRECONDITION / HARDWARE`.
Diferenciar PASS parcial de cierre de hito y no contabilizar reintentos
fallidos como gates nuevos. Conservar URL de run, commit y artefactos.
No publicar binarios experimentales como releases.

### 16.2 Ejecución autónoma e intervención humana

El asistente/Codex puede iterar, corregir **overlays/tooling**
y verificar por GitHub Actions sin requerir un mensaje del operador
por cada corrida; preferir un workflow por **hito**, con etapas
internas que fallen cerrado. No iniciar nuevos workflows redundantes
mientras siga una ejecución idéntica en curso.

Pedir intervención cuando sea indispensable: autorización material
de alcance/adopción, validación de Arduino IDE local, selección de
puerto/equipo, flasheo, evidencia de periféricos físicos o cierre
de release. No convertir el PASS de CI en aprobación de upload.
La actividad entre conversaciones **no debe presumirse automática**
salvo que exista tarea/agente programado explícitamente y habilitado.

**G10-I Alpha13:** CI puede componer y compilar el overlay de modo
aislado; `A13-006=ADOPTION_BLOCKED`, `G10_FINAL=NOT_CLOSED` hasta
evidencia integradora y autorización. Mantener GPIO4 reset deshabilitado
(`255`), cinco archives `.a` inmutables y todos los periféricos
de autoload normal. No modificar producto por velocidad o por un
resultado parcial.

### 16.3 Separación entre documentación pública y memoria interna

`platform-jwplc` es un repositorio **público**. No hay carpetas,
ramas, nombres `private`, `.gitignore` ni GitHub Actions artifacts
que conviertan en confidencial un archivo ya incluido en el repositorio
o en su historial público.

- **Aquí, público:** APIs, README para usuarios, instrucciones
  de compilación, contratos reproducibles del CI, decisiones de
  producto publicables, changelog y gates desprovistos de secretos.
- **Solo en repositorio privado independiente, cuando se autorice
  y configure:** `JWPLC_ASSISTANT_FAILURES_AND_PREVENTION`,
  historial interno detallado de errores y correcciones de asistentes,
  decisiones de colaboración no publicables, handoffs internos,
  notas sensibles de desarrollo y transferencias de contexto.
  Los nombres son ejemplos; ningún contenido privado debe trasladarse
  automáticamente desde archivos del Proyecto de ChatGPT.
- **Referencia cruzada segura:** el repo público puede mostrar el
  nombre de una política interna o un identificador de decisión
  **no sensible**, pero no su texto, URL privada con credenciales,
  datos de clientes, secretos, logs seriales identificadores
  ni rutas locales con información reservada.
- **GitHub Actions:** no imprimir contenido interno en logs de una
  ejecución pública ni incluirlo en artifacts públicos. Para
  comprobaciones que necesitan documentación privada, usar
  workflows aislados en el repositorio privado y permisos mínimos.
- **Sin sincronización implícita:** archivos subidos a ChatGPT Project
  no se actualizan al cambiar GitHub ni GitHub se actualiza al
  editar esos adjuntos. Mantener una única versión canónica
  por documento, con fecha/hash, y sincronizar deliberadamente
  la copia del Proyecto cuando se requiera; evitar forks
  documentales silenciosos.
- **Publicación histórica:** mover posteriormente un documento
  a un repo privado no borra versiones antiguas ya expuestas
  en commits, forks, clones o artifacts públicos. Auditar
  la procedencia antes de subir contenidos internos.

**Estado actualizado (2026-10-13):** el operador aprobó
reutilizar el repositorio privado ya existente `JW-Control/jwplc-dev-mcp`
como fuente de conocimiento interno; el índice está en
`knowledge/README.md`, rama documental
`docs/private-knowledge-foundation`. No se creó otro
repositorio ni se alteró el servidor MCP. La migración del contenido
histórico `FAILURES` y otros adjuntos sigue **PENDIENTE DE REVISIÓN**,
y las autorizaciones para compartir material privado no cambian.
La versión pública de este documento conserva solamente las reglas
de colaboración y una referencia nominal; no copiar documentos
internos ni su contenido a `platform-jwplc`.

# Alpha13 G10-I — campaña nocturna Codex Cloud (sin hardware)

**Estado:** INSTRUCCIONES PREPARADAS; no se ha lanzado tarea Codex ni certificado CI nuevo. Fecha 2026-10-14.

## Rama y alcance
- Checkout de partida: `v2.1.0-alpha.13/feature/cleanup-robustness` de `JW-Control/platform-jwplc`.
- Rama aislada de entrega: `codex/alpha13-g10-b03-nightly` (o sufijo único con ese prefijo); jamás push directo a release/main o rama alpha de partida.
- Workflow `.github/workflows/alpha13-g10-integrated.yml` permite push en `codex/alpha13-g10-*` desde commit `e4be368d`. Ejecuta N1 y N2; N3 sigue manual.
- El entorno Codex Cloud es Linux para el agente aunque el usuario desarrolla en Windows. No necesita USB/COM ni acceso al MCP instalado en el PC.

## Etapas autónomas consecutivas, con stop por evidencia
1. **P0 preflight:** verificar HEAD/branch, `git status`, las instrucciones `AGENTS.md`, `ALPHA13_STATUS.md`, contrato B03, integrador, ruta de archives y limitaciones del entorno. No rehacer auditoría global ni reinstalar toolchain si está disponible.
2. **B03 candidato:** modificar exclusivamente el overlay/harness necesarios para corregir UDP/DNS/DHCP, propiedad/epoch del socket y `SPI_BUSY` sin bloquear el bus, incluyendo rutas finish/cancel/timeout/rebegin y reintento incluso sin operación activa. Mantener producto original intacto. Implementar tests source-backed positivos y negativos que detecten regresiones. No usar falsos PASS de mocks desconectados.
3. **Actualizar semántica del gate:** el integrador actual `a13_g10_integrated.py` y el YAML exigen literalmente `RISK_REPRODUCED` y `COMPOSED_REVIEW_BLOCKED`. Un fix real causará falla deliberada de esas condiciones. Sustituir el *criterio de aceptación* con evidencia positiva de corrección, conservando un test mutante que reproduce el fallo original. No limitarse a cambiar cadenas esperadas; validar que las pruebas fallan ante la mutación.
4. **N1/N2:** push rama `codex/alpha13-g10-*`; esperar a GitHub Actions, comprobar CI del HEAD exacto, todos los jobs y artifacts. En `PARTIAL_JOBS`, fallos no explicados o ausencia de run, NO declarar PASS. Corregir causas demostradas, máximo tres ciclos CI por blocker. Los 4 sketches N2 compilan APIs pero no validan comportamiento en red.
5. **Revisión de otros bloqueos G10-I:** únicamente después de B03 con pruebas positivas y CI; clasificar B01–B08 según pruebas reales del overlay. Resolver bloqueos adicionales no físicos SOLO si son separables y medibles sin tocar firmware productivo. No declarar un bloqueador resuelto por compilación o por cambiar el estado escrito.
6. **N3:** si N1/N2 y los bloqueos técnicos están resueltos, ejecutar `workflow_dispatch regression_level=full` sobre la rama de tarea (si permisos/entorno lo permiten), y comprobar 11 sketches baseline + 11 candidato y procedencia. Si no puede solicitar run, dejarlo como pendiente y STOP; no asumir ejecución automática.
7. **Entrega:** preparar PR hacia rama alpha, sin merge, con commits, archivos, método fuente→objeto→ELF, pruebas positivas/negativas, N1/N2/N3, URLs/SHAs, decisiones y gates no físicos agrupados. Registrar estado de la rama de tarea; no alterar el estado canónico de la rama alpha hasta revisión del PR. Preparar una matriz concisa de pruebas físicas para el usuario al volver.

## Límites no negociables
- **NO hardware**: no USB, COM, flash, serial físico, GPIO4 reset ni red física; no cargas/sensores.
- No regenerar ni retirar archives del producto. Si cambia biblioteca precompilada, source-first solo en overlay temporal y verificar procedencia.
- Conservar todos los periféricos en autoload y APIs Arduino.
- OPENPLC/HMI_DESIGNER/TFT_NEW_FEATURES fuera de alcance de Alpha13.
- No publicación, merge, rebase destructivo, reset/clean forzoso de worktree, ni cambios de configuración de seguridad.
- El CI solo garantiza lo que sus pruebas realmente ejercen; no aprobar hardware por un verde.
- Si bloqueado, **STOP y dejar evidencia**; no ejecutar un bucle ilimitado de tests, no pasar a otra alpha.

## Presupuesto operativo
- Máximo 3 iteraciones CI por problema técnico, incluyendo diagnósticos.
- Preferir N1/N2 y logs acotados; N3 solo al final y una vez por candidato estable.
- No ejecutar los 7 sketches del onboarding repetidamente ni reinstalar Arduino.
- No asignar tareas independientes simultáneas que editen el mismo overlay.
- Tokens/uso de Codex pueden agotarse: la tarea no garantiza continuidad nocturna indefinida.

## Condición de finalización
`READY_FOR_HARDWARE` **solo** con todas las rutas de B03 y bloqueos G10-I cerrados con pruebas source-backed reproducibles, CI N1/N2/N3 en HEAD de la rama de tarea, y revisión de evidencias. De lo contrario `REVIEW_STOP` y enumerar exactamente qué falta. La autorización de hardware queda para la mañana.

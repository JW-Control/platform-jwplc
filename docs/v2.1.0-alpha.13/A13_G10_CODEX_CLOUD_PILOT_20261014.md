# Alpha13 — piloto Codex Cloud de G10-I (B03)

**Preparado:** 2026-10-14. **Estado:** NOT_STARTED (requiere entorno Codex Cloud conectado por el usuario). **Ningún permiso físico concedido.**

## Prompt inicial para la tarea Codex Cloud

Trabaja únicamente sobre `JW-Control/platform-jwplc`, partiendo de la rama `v2.1.0-alpha.13/feature/cleanup-robustness`. Lee primero `docs/v2.1.0-alpha.13/ALPHA13_STATUS.md`, `docs/v2.1.0-alpha.13/A13_G10_B03_DEFERRED_CLOSE_CONTRACT_20261014.md`, `docs/v2.1.0-alpha.13/A13_G10_INTEGRATED_MILESTONES_20261013.md` y `docs/v2.1.0-alpha.13/A13_G10_CI_LEVELS_AND_SOURCE_FIRST_20261014.md`.

Tarea: implementar en el **overlay candidato aislado** una solución robusta a B03 UDP/DNS/DHCP cuando `SPI_BUSY` impide cerrar un socket. Preservar ownership/token/epoch, retry desde un servicio realmente alcanzable aun después del fin lógico DNS/DHCP, evitar reutilizar un socket cuyo cierre sigue pendiente y nunca bloquear SPI durante esperas. Conservar APIs Arduino y autoload de todos los periféricos.

Avanza por cambios pequeños *en una rama de tarea nueva*, documenta por qué cada cambio modifica comportamiento real y añade pruebas source-backed positivas y negativas que fallen si se reintroduce el defecto. Ejecuta N1 y N2 en el workflow `alpha13-g10-integrated.yml` del repositorio. Comprueba HEAD exacto, jobs completos, pruebas nativas reales y procedencia de ELF/objetos. Si N1/N2 pasan y B01–B08 están cerrados técnicamente, solicita N3 mediante `workflow_dispatch regression_level=full`; N3 aún no representa PASS físico.

**STOP inmediato** si los tests solo prueban mocks ajenos a fuentes reales, la lectura del CI es parcial, no puedes observar un CI, falla una precondición, aparece riesgo de colisión socket/epoch o se requiere cambiar fuera del overlay. No convertir PASS de detección de riesgo B03 en PASS de corrección.

**Nunca** ejecutar flash, serial, GPIO4 reset, cambiar los cinco archives congelados, modificar precompiled definitivo, hacer merges, publicar releases o tocar OPENPLC, HMI_DESIGNER y TFT_NEW_FEATURES. No tocar worktree local del responsable. No usar secretos ni subir logs confidenciales.

Al detenerte, crear PR de revisión o dejar commits de rama de tarea, junto con resumen compacto: rama, HEAD, archivos cambiados, CI run URLs, N1/N2/N3, bloqueos abiertos, evidencia real vs mocks y siguiente trabajo. No afirmar que G10-I terminó si queda algún bloqueador.

## Condiciones para empezar
- Crear y publicar entorno Codex Cloud asociado a GitHub con permisos limitados al repositorio.
- Confirmar que la tarea ve rama Alpha13, documentos y workflows.
- Autorizar únicamente operaciones remotas de rama/PR y CI; ninguna prueba física.
- El MCP local Windows no se conecta automáticamente a Codex Cloud. Para Cloud utilizar GitHub autorizado y evidencia de Actions.

## Estado inicial
`G10-I=OPEN`
`B03=OPEN`
`A13-006=ADOPTION_BLOCKED`
`G10-H=NOT_STARTED`

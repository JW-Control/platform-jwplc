# Apertura de `v2.1.0-alpha.15/OpenPLC-HMI/main` (24/10/2026)

Rama principal de la alpha **OpenPLC + HMI** (etiqueta de trabajo ALPHA14 de Kenyi, se publica como
`2.1.0-alpha.15`). Se crea en los dos repos con el mismo nombre, sobre lo último publicado, y trae las ramas
vivas más actualizadas de la línea. Desde aquí parten las features `v2.1.0-alpha.15/OpenPLC-HMI/feature/<tema>`.

```text
A15_OPENPLC_HMI_MAIN_PLATFORM=CREADA  7476cd6ca42edae1111409be9c59d7af60ff8eca
A15_OPENPLC_HMI_MAIN_EDITOR=CREADA    adde17d74e2e400b2d5881f1c50b874624ad2e69
CONFLICTOS=2 (platform-jwplc), 0 (openplc-editor), resueltos
TESTS_DESIGNER=PASS [ejecutado]  (tests/run-all.cjs + modo OpenPLC simulado en Chromium)
VPP=2.1.0-alpha.32 sin cambios · EDITOR=4.2.8-jwplc.3
```

## 1. platform-jwplc

Base: `release/v2.1.x` @ `f3f47aeeaaef4435783fc9a62827862383cdec42` (2.1.0-alpha.14 publicada, PR #157).

| Paso | Commit | Qué entra |
|---|---|---|
| 1 | `83abf3c` | `v2.1.0-alpha.14/HmiDesigner/main` @ `7e7a1412c98fe76e6d78004024766c57ece88a21`. Su árbol es igual al de `release/v2.1.x` (el PR #157 se fusionó con squash): sólo se trae la historia, para que la línea de Kenyi —que parte de commits de André— se fusione con 2 conflictos en vez de reabrir toda la alpha. |
| 2 | `7476cd6` | `v2.1.0-alpha.14-OpenPLC-HMI` @ `20ef597e8c59ddb82236dbb0bc0f49f44a71731f` (Kenyi): VPP 2.1.0-alpha.32, `designer-openplc-host.js`, gates A14_F0b/F0c/F0d, contrato F3, informe HMI v1 y traspaso del 24/10. |

Conflictos (los mismos que anticipaba el traspaso con `git merge-tree`):

- `.gitattributes`: se conservan las dos reglas nuevas (`vpp/screens/hmi.json text eol=lf` y el `.cmd` del Designer
  con `eol=crlf`).
- `tools/jwplc-hmi-designer/poc/desktop.html`: lista de scripts del Designer de 2.1.0-alpha.14 (`?v=` de André) y se
  vuelve a cargar `designer-openplc-host.js?v=1793801900` después de `designer-project-integration-fix.js`.

Ajustes en el mismo merge: `designer-openplc-host.js` en la lista `CORE` de `service-worker.js` (`CACHE_NAME`
`jwplc-hmi-designer-a16-v48`) y `tools/jwplc-hmi-designer/CLAUDE.md` al día. `CLAUDE.md` raíz: se queda el de
`release/v2.1.x` (la regla de autoría de Kenyi ya estaba).

Verificación `[ejecutado]`:

- `node --check` de `poc/*.js`: sin errores.
- `node tools/jwplc-hmi-designer/tests/run-all.cjs`: 16/16 PASS (codegen, golden, inspector, MENU, ICON, sketch,
  CLI, Live Bridge, host-sim). El flujo Arduino no cambia (regla 6).
- Modo OpenPLC simulado en Chromium (`window.JWPLCHMIHost = { version: 1, mode: 'openplc', listVariables }`, como
  en el §5 del traspaso):
  - VALUE: aparece «Variable PLC» en Datos, sólo con variables numéricas; al elegir `CONTADOR` queda
    `variable = contador` y `plcBinding = { var: 'CONTADOR', access: 'R' }`;
  - MENU: ítem «Ver» nuevo, selector en el editor del nodo, `LIMITE` → `variable = limite`, `valueType = int32`,
    `plcBinding` guardado (el getter `JWPLCHMIMenu.selectedNode` sigue en `designer-menu.js:1316`);
  - sin errores de consola.
- `[no verificado]`: cadena completa dentro de la app OpenPLC y en la placa (pendiente 3 del traspaso).

## 2. openplc-editor

Base: `develop/alpha14-openplc-hmi-v1` @ `c0bf9ac07a209ec7d10310de7fcf487336fdb4c5` (Kenyi: HMI v1 F0–F0d y F3,
contiene `develop/alpha12-backplane-closure` @ `1f12dc549`).

| Paso | Commit | Qué entra |
|---|---|---|
| 1 | `adde17d74` | `claude/awesome-goldberg-7sj2vx` @ `df6b74d6cc30eebd6abf1ea2d3d83c3570454a5d`: versión 4.2.8-jwplc.3 (`package.json`, `release/app`), workflows del instalador Windows y del pre-release (atados a su rama original: no se disparan en la nueva), notas `docs/jwplc/releases/jwplc-editor-v4.2.8-jwplc.3.md` y la regla de autoría en `CLAUDE.md` (responde la duda del traspaso §4: también aplica en openplc-editor). |

Sin conflictos y sin cambios de código fuente. `[no verificado]`: `npm run lint`/`npm run test` (no hay
`node_modules` en la sesión en la nube); el merge sólo toca versiones, workflows y documentación.

## 3. Ramas contenidas en las nuevas `…/main` (a borrar por el propietario)

La sesión en la nube no puede borrar ramas remotas (HTTP 403). Para recuperar una: `git push origin <sha>:refs/heads/<rama>`.

| Repo | Rama | SHA | Estado |
|---|---|---|---|
| platform-jwplc | `v2.1.0-alpha.14/HmiDesigner/main` | `7e7a1412c98fe76e6d78004024766c57ece88a21` | contenida; alpha publicada (§5 paso 6: se borra) |
| platform-jwplc | `v2.1.0-alpha.14-OpenPLC-HMI` | `20ef597e8c59ddb82236dbb0bc0f49f44a71731f` | contenida |
| openplc-editor | `develop/alpha14-openplc-hmi-v1` | `c0bf9ac07a209ec7d10310de7fcf487336fdb4c5` | contenida |
| openplc-editor | `claude/awesome-goldberg-7sj2vx` | `df6b74d6cc30eebd6abf1ea2d3d83c3570454a5d` | contenida; el tag `jwplc-editor-v4.2.8-jwplc.3` queda en `d3ed3335f` |
| openplc-editor | `develop/alpha12-backplane-closure` y anteriores de Alpha7/12 | `1f12dc549…` | contenidas en `develop/alpha14-openplc-hmi-v1` |

Ramas viejas de openplc-editor **no** traídas (julio/agosto, revisar con Kenyi antes de borrar):

| Rama | SHA | Commits sin equivalente | Nota `[inferido]` |
|---|---|---|---|
| `develop/jwplc-backplane-remote` | `75272a4fb826e973bd618c1f91ac6e877810c48c` | 1 (3 archivos) | "update blackplane modbud rtu", anterior a Alpha12 |
| `develop/jwplc-native-discovery` = `develop/jwplc-device-discovery` | `42825f82139bf77816ce95208067b27677f23da8` | 12 | sustituidas por `develop/jwplc-device-discovery-clean` (contenida) |
| `develop/jwplc-modbus-fix` | `51dfaebf8a89ccfae247a233afe336c96c61f7e4` | 10 | Modbus TCP sobre W5500 de julio; base de las de discovery |

Las demás ramas remotas de openplc-editor son del upstream (Autonomy-Logic) y no se tocan.

## 4. Siguiente

Lo pendiente sigue el orden del traspaso `JWPLC_NEW_CHAT_START_ALPHA14_OPENPLC_20261024.md` §3, con el punto 1 ya
hecho: el siguiente es el puente ampliado (contrato 1.1) en openplc-editor.

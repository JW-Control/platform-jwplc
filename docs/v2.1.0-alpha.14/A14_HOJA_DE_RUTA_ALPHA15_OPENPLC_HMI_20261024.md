# Hoja de ruta · alpha OpenPLC + HMI (publica 2.1.0-alpha.15) · 24/10/2026

Qué está hecho, qué se probó y qué falta, en orden. Estado vivo: `ALPHA14_OPENPLC_STATUS.md`.

## 1. Ramas y PR

| Repo | Rama | Último commit | Contenido | PR / estado |
|---|---|---|---|---|
| platform-jwplc | `v2.1.0-alpha.15/OpenPLC-HMI/main` | `7476cd6` | 2.1.0-alpha.14 publicada + línea de Kenyi | rama principal |
| platform-jwplc | `…/feature/apertura-rama-principal` | (esta) | apertura, STATUS, CLAUDE.md, esta hoja de ruta | PR #175 → `…/main` |
| platform-jwplc | `…/feature/vpp-version-alpha15` | `e434646` | VPP `2.1.0-alpha.15`, `.bat` (llave y copia a `openplcProbe`) | sin PR: falta la firma |
| platform-jwplc | `…/feature/designer-alpha14-en-openplc` | `57c9070` | modo OpenPLC del Designer 2.1.0-alpha.14, contrato 1.1 y 1.2, test | sin PR |
| openplc-editor | `v2.1.0-alpha.15/OpenPLC-HMI/main` | `adde17d74` | HMI de Kenyi + pre-release 4.2.8-jwplc.3 | rama principal |
| openplc-editor | `…/feature/designer-alpha14-en-openplc` | `d1532d82c` | puente 1.1, variables sin dirección (1.2), compilar sin header | sin PR |

Las cuatro features tocan archivos distintos: se pueden fusionar en cualquier orden sin conflictos `[inferido]`.

## 2. Hecho

1. **Rama principal** en los dos repos sobre lo publicado: la línea de Kenyi más el HMI Designer de 2.1.0-alpha.14.
   Hubo 2 conflictos y se resolvieron.
2. **VPP con la numeración de la alpha:** `2.1.0-alpha.32` → `2.1.0-alpha.15`, mismo payload.
   - El `.bat` encuentra la llave en `Documents\openplcProbe` y deja ahí el `.vpp` firmado.
3. **Designer de 2.1.0-alpha.14 dentro de OpenPLC** (contrato 1.1):
   - «OpenPLC: proyecto ▾» y «Actualizar HMI»;
   - abre el HMI guardado al iniciar;
   - pregunta al cerrar con cambios sin guardar;
   - selector de puerto serie para Live;
   - si el Designer del checkout es viejo, avisa en lugar de abrirlo.
4. **Variables sin dirección** (contrato 1.2):
   - las del `main` hecho en Ladder aparecen en «Variable PLC» como «variable del programa»: BOOL, enteros y REAL, lectura y escritura;
   - se enlazan por la tabla de depuración de STruC++, resuelta después de compilar el ST.
5. **Compilar sin pantalla:** un `.jwhmi` sin header ya no detiene la compilación (era el error del 24/10); ahora avisa.
6. **Guardado al salir:** al cerrar OpenPLC o cambiar de proyecto se guardan juntos el header y el `.jwhmi`.

## 3. Pruebas

| Prueba | Resultado | Dónde |
|---|---|---|
| Tests del Designer (`run-all.cjs`) | 17/17 PASS `[ejecutado]` | platform-jwplc, incluye `openplc-host-smoke.cjs` |
| Tests de `compile` del editor | 284/284 PASS `[ejecutado]`; `jwplc-hmi.ts` y `jwplc-hmi-host.ts` al 100 % | openplc-editor |
| `tsc` del editor | sin errores `[ejecutado]` | openplc-editor |
| HAL del VPP + HMI (header F0c) contra el package 2.1.0-alpha.14, ESP32 | PASS, 447 781 B `[ejecutado]` | `A14_VPP_VERSION_ALPHA15` §5 |
| Igual, header del Designer 2.1.0-alpha.14 y medición de tiempos | PASS, 448 225 B `[ejecutado]` | ídem |
| Igual, 7 enlaces **sin dirección** (tabla de depuración simulada) | PASS, 448 525 B `[ejecutado]` | contrato §8 |
| Firma 2.1.0-alpha.15 + `verifyPackageSignature` (llave de prueba) | PASS `[ejecutado]` | `A14_VPP_VERSION_ALPHA15` §5 |
| Firma con `jwcontrol-2026` e instalación en OpenPLC | PENDING `[no verificado]` | PC del propietario |
| Designer en la app (abrir, enlazar, Actualizar HMI, cerrar/reabrir) | PENDING `[no verificado]` | guía `A14_F3_1` §3 |
| Placa: Ladder con variables sin dirección (BOOL piloto, INT en MENU con escritura) | PENDING `[no verificado]` | guía `A14_F3_1` §3 pasos 4–7 |
| Placa: F0, F0b, F0c, F0d (Kenyi, VPP alpha.30–32) | PASS en placa | informe `A14_HMI_V1_OPENPLC` |

Fuera de esta alpha, ya estaban antes:
- `npm run validate:arch` falla por un import de `device-port.ts` (línea de Kenyi);
- `eslint` marca `pipeline.ts:698`.

## 4. Falta, en orden

1. **Firmar el VPP** en la PC (`build-jwplc-vpp-v2.bat`), commit de `vpp/signature.json` en `…/feature/vpp-version-alpha15`.
2. **Probar en la app y en la placa** con las dos features `designer-alpha14-en-openplc` (guía `A14_F3_1` §3):
   - proyecto nuevo con Ladder;
   - variables sin dirección (BOOL e INT);
   - Actualizar HMI, compilar, subir;
   - cerrar y reabrir el Designer.

   Anotar los flags `A14_F3_1_*` / `A14_F3_2_*`.
3. **PR de cada feature hacia `…/main`** (los dos repos) y fusionar con el OK del propietario. Después:
   - borrar las ramas de las features;
   - actualizar `ALPHA14_OPENPLC_STATUS.md` con los SHA.
4. **Revisión con André (A15-6):** contrato 1.1 y 1.2, y los cambios en `designer-sketch-menu.js`,
   `designer-native-project.js`, `designer-project-integration-fix.js` y `desktop-responsive.js`.
5. **Para que lo use alguien sin los repos:** hoy OpenPLC toma el Designer del checkout vecino de platform-jwplc.
   - Hay que meterlo en el VPP firmado o en el instalador del editor.
   - Es requisito para publicar.
6. **Mejoras del puente** (cada una su feature):
   - lista de variables en vivo (hoy es la del momento de abrir);
   - pestaña en vez de ventana (F4);
   - TEXT ↔ STRING;
   - LREAL y TIME sin dirección;
   - programa instanciado varias veces (hoy se usa la primera instancia).
7. **Package (Alpha13/jefe):** el cambio de página de la TFT atrasa un scan hasta ≈ 65 ms (F0d, `jwplcSystemTask`).
8. **Cierre de la alpha:**
   - versión visible del Designer `2.1.0-alpha.15`;
   - versión del package, `README` y `PRE_RELEASE.md`;
   - pre-release del editor (siguiente `4.2.8-jwplc.N`, con `APP_VERSION` y `package.json`);
   - feature de cierre → `…/main` → PR hacia `release/v2.1.x`, que sólo se fusiona con autorización;
   - borrar las ramas viejas listadas en `A14_APERTURA_RAMA_ALPHA15_OPENPLC_HMI_20261024.md` §3.

# A15 · Republicación de 2.1.0-alpha.15 (27/10/2026)

```text
A15_REPUBLICACION_PREPARADA=PASS
A15_REPUBLICACION_PUBLICADA=PASS     (27/10/2026, OK del propietario; release/v2.1.x @ cbeced0d)
```

## Por qué

El propietario decidió (27/10/2026) que dos cambios que quedaron fuera de la Release del mismo día entren en la
alpha 15 y no en una alpha 16:

| Cambio | PR | ¿Cambia el zip? |
|---|---|---|
| Snapshot JW Libraries: `JW_DWIN` 0.2.0 nueva, `JW_RS485` y `JW_ModbusRTU` 0.1.1 | #178 | Sí |
| Gate SIM_REGRESSION de JWPLC Sim antes de publicar y en el CI de PR | #190 | No |

Primera publicación, que será reemplazada: Release `v2.1.0-alpha.15` del 27/10/2026 17:21 UTC, sobre
`release/v2.1.x` = `803390f7`. Su zip `jwplc-esp32-2.1.0-alpha.15.zip` tenía SHA-256
`c10ce24bdb1bd2f8615c01924c3392d6a80da769eceee79620046ad6db95319b` y 25 182 944 bytes.

La rama `v2.1.0-alpha.15/OpenPLC-HMI/main` se recreó desde `release/v2.1.x` (`803390f7`) para recibir estas features.

## Hallazgo del gate de JWPLC Sim (27/10/2026)

El PR #192 (`v2.1.0-alpha.15/OpenPLC-HMI/main → release/v2.1.x`) dio SIM_REGRESSION en rojo por tres causas:

1. Paridad: `JW_DWIN` es una librería nueva y el registro del simulador (`sim/package-parity.json`) no la tenía.
2. Matriz: 14 ejemplos nuevos sin prueba en el simulador (la plantilla, las 11 lecciones del curso y los dos de
   `JW_DWIN`).
3. Matriz: `JW_RS485/Status` falló. El snapshot de JW Libraries (#178) devolvió los ejemplos y READMEs de
   `JW_RS485` y `JW_ModbusRTU` a su versión anterior a alpha.14: pines 16/17 escritos a mano en lugar de
   `JWPLC_RS485_RX_PIN`/`JWPLC_RS485_TX_PIN` (#148) y, en `Status`, un segundo bus en GPIO25/26/27 (TFT DC,
   zumbador y EN_IO), que alpha.14 había retirado (#142).

Arreglo: las causas 1 y 2 se resuelven en `jwplc-sim` (registro y 14 pruebas nuevas) y se fija su commit nuevo en
los workflows. La causa 3 se resuelve en el package restaurando esos 9 archivos a su versión de alpha.14 (rama
`v2.1.0-alpha.15/OpenPLC-HMI/feature/rs485-ejemplos-alpha14`). El repositorio `JW-Control/JW-Libraries` necesita
el mismo cambio para que el próximo snapshot no lo vuelva a revertir.

## Procedimiento

1. Fusionar en `v2.1.0-alpha.15/OpenPLC-HMI/main`: #178, #190 y esta feature (notas y este documento).
2. PR `v2.1.0-alpha.15/OpenPLC-HMI/main → release/v2.1.x`. Su CI corre el smoke del package y SIM_REGRESSION.
   Fusionar con **merge commit**. `README.md` no cambia, así que no dispara `auto-release-jwplc-on-readme.yml`.
3. **Con OK explícito del propietario**: borrar la Release y el tag `v2.1.0-alpha.15`
   (`gh release delete v2.1.0-alpha.15 --cleanup-tag`). El workflow se niega a sobrescribir una Release
   existente, por inmutabilidad.
4. Publicar de nuevo: `gh workflow run release-jwplc-package.yml --ref release/v2.1.x -f version=2.1.0-alpha.15`.
   - Corre SIM_REGRESSION.
   - Regenera el zip y crea la Release con `docs/v2.1.0-alpha.15/PRE_RELEASE.md`.
   - Sube los `.exe` del HMI Designer.
   - Abre el PR de índices hacia `main`. Con `--replace-existing-index-entry` se reemplaza la entrada de la
     alpha.15 con el checksum nuevo.
5. Fusionar el PR de índices con **merge commit** y avanzar `release/v2.1.x` por fast-forward a `main`, para que
   las dos ramas queden en el mismo commit.
6. Borrar `v2.1.0-alpha.15/OpenPLC-HMI/main` y las ramas de las features. Anotar aquí el SHA, el checksum nuevo
   y el resultado.

No cambia: OpenPLC Editor 4.2.8-jwplc.8, VPP 2.1.0-alpha.15 y su Release en `openplc-editor`. El VPP no fija
el checksum del zip del package.

## Efecto en quien ya instaló la primera publicación

El Arduino IDE guarda el zip descargado en `staging/packages`. Con el checksum nuevo en el índice, una
reinstalación puede dar error de checksum hasta borrar ese zip. Las notas de la Release lo explican. Quien
ya tenga instalada la primera versión no recibe `JW_DWIN` hasta desinstalar y volver a instalar
2.1.0-alpha.15.

## Resultado

- #192 (`v2.1.0-alpha.15/OpenPLC-HMI/main → release/v2.1.x`) fusionado con merge commit: `cbeced0d`.
  `auto-release-jwplc-on-readme.yml` corrió y no publicó (README sin cambio de versión).
- La Release y el tag `v2.1.0-alpha.15` los borró el propietario a mano (la sesión en la nube no puede).
- `release-jwplc-package.yml` (corrida 40599208706): SIM_REGRESSION PASS, publicación PASS y `.exe` del HMI
  Designer PASS. Release `v2.1.0-alpha.15` sobre `cbeced0d`, pre-release.
- Zip nuevo `jwplc-esp32-2.1.0-alpha.15.zip`: SHA-256
  `2fafabf0b16402ecd4c0b44500e33dbe71c3ea5d4d7b5335ca08386d9f61d55b`, 25 213 982 bytes.
- PR de índices #194 fusionado en `main` con merge commit (`f9ce8c19`): solo cambian el checksum y el tamaño de la
  entrada 2.1.0-alpha.15 en `package_jwplc_index_dev.json`.

### Incidente: Release de la alpha.14 borrada por error

Al borrar la Release de la alpha.15 también se borró la de la alpha.14 (el tag `v2.1.0-alpha.14` @ `f3f47aee` no
se tocó). Se recreó a mano el 27/10/2026 sobre el mismo tag con los archivos originales, descargados de los
artefactos de la corrida que la publicó (40123199627, 24/10/2026): `jwplc-esp32-2.1.0-alpha.14.zip` con SHA-256
`588f7f77a365358f77c635469ae00100758c333a215330a81abc9227c802347b` y 25 142 970 bytes, igual al índice, y los
tres archivos del HMI Designer 2.1.0-alpha.14. El índice no cambió.

### Ramas a borrar (último SHA)

| Rama | SHA | Motivo |
|---|---|---|
| `v2.1.0-alpha.15/OpenPLC-HMI/main` | `e0348f18` | Línea cerrada, contenida en `release/v2.1.x` |
| `v2.1.0-alpha.15/OpenPLC-HMI/feature/gate-simulador` | `e1c56337` | #190 fusionado |
| `v2.1.0-alpha.15/OpenPLC-HMI/feature/republicacion` | `67f23a00` | #191 fusionado |
| `v2.1.0-alpha.15/OpenPLC-HMI/feature/rs485-ejemplos-alpha14` | `55aef639` | #193 fusionado |
| `sync/jw-libraries-v2.1` | `3c56854d` | #178 fusionado |
| `claude/jwplc-basic-simulator-8dgcen` | `b8906d83` | Su commit de workflows entró por #190 |
| `ci/release-index-v2.1.0-alpha.15` | `b942a684` | #194 fusionado |

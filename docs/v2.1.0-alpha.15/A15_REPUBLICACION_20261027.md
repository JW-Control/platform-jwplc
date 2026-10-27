# A15 · Republicación de 2.1.0-alpha.15 (27/10/2026)

```text
A15_REPUBLICACION_PREPARADA=IN_PROGRESS
A15_REPUBLICACION_PUBLICADA=PENDING   (requiere OK explícito del propietario)
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

Pendiente.

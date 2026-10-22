# Alpha13-G7 — Cierre desde PHYSICAL_PASS ya verificado

**Estado:** PREPARED_NOT_EXECUTED (2026-10-11).

La subida y prueba FRAM fueron completadas en
`g7_20261011_164228_e5e86ddc`, con firmware SHA-256
`30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a`.
El operador obtuvo `G7_RECOVER_STATUS=PHYSICAL_PASS`,
checksum de bloques y restauración completa de la ventana
`0x1F00..0x1F7F` mediante backup NVS. No es necesario
repetir upload ni escrituras.

Herramienta:
`tools/alpha13/gates/a13_g7_close_from_physical_pass.py`.

Verifica antes de tocar producto:
- Git y CLI oficial 1.5.1 `jwplc_local:esp32:jwplcbasic`.
- Manifiesto histórico G7 R3 y ausencia de adopción/commits anteriores.
- SHA del firmware realmente compilado, archive temporal reconstruido,
  fuentes candidatas, CLI, objeto, logs del enlace y builds candidatos.
- Prueba física única; `G7_FRAM_PHYSICAL_RECOVERY.json` y log íntegro,
  con hashes y confirmación de backup NVS, restauración y errores=0.
- Una ejecución opcional `--verify-only` repite únicamente estas
  comprobaciones, no modifica archivos de producto ni hardware.

Solo tras pasar:
1. Copia el archive verificado a un nuevo directorio temporal.
2. Adopta `JW_FRAM.cpp`, `JW_FRAM.h`, `libJW_FRAM.a` con respaldo.
3. Compila dos ejemplos normales y verifica `precompiled=full`,
   enlace `-L/-lJW_FRAM`, SHA y scope de producto.
4. Ejecuta auditoría final y commits/push no-force de producto/docs.
5. Reporta `G7=CLOSED_PASS`; no merge, PR o release.

Si cualquier comprobación falla: `REVIEW_STOP`, sin re-flash ni
repetir `G7_FRAM_RUN`. Si falla antes de Git después de adopción,
se restauran los tres archivos desde backup, respetando el HEAD.
Si Git avanzó, no hay reset/clean forzado; revisar manualmente.

**Pendientes de Alpha13:** F120 conserva causa raíz
del freeze post-upload desconocida, y A13-009/013 no se asignan
semánticamente sin la auditoría canónica.

## F121: autocheck del esquema documental antes de la adopción

El primer intento de cierre realizó regresiones normales 2/2 y
auditoría final PASS, pero `close_documents()` produjo
`KeyError('product_regressions')`, debido a que el productor
guardó `product_regressions_regressions`.
Rollback PASS; cero commits y ningún acceso físico.

La corrección elimina el sufijo duplicado. El nuevo
`a13_g7_closure_document_selftest.py` genera los documentos
con las funciones reales y compilación simulada, incluye
caso negativo de clave ausente y detiene el cierre ANTES de
adoptar archivos si falla. `--verify-only` ahora ejecuta
esta autoprueba. El origen de F120 sigue como pendiente
sin inferir un fallo de FRAM.

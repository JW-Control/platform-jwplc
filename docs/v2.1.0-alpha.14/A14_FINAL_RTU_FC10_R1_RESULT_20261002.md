# Alpha14 — R-FC10 R1 — resultado 2026-10-02

## Resultado

`HARNESS_FAIL / PRODUCT_PASS_SO_FAR`

Ventana física: 600 s, 500 kbaud, Master FIFO 9, Slave FIFO 8, BULK,
QUEUED, Master GAP y Slave STRUCTURAL.

Resultados principales:

- FC10_SUCCESS=158400
- FC03_SUCCESS=158399
- CYCLES=19799
- TRANSACTION_MAX_US=6874
- FC10_FAILED=0
- FC03_FAILED=0
- VERIFY_FAILS=0
- REQUEST_REJECTED=0
- CRC Master=0
- CRC Slave=0
- timeout Master=0
- exceptions Slave=0
- todas las cantidades 1/2/4/8/16/32/64/123 cubiertas
- source-first PASS
- precompiled marker NO
- Master/Slave sin reboot

El único check fallido fue `SLAVE_CROSS_COUNT`.

## Diagnóstico

El runner enviaba `X` al finalizar exactamente los 600 s y el firmware hacía
`running=false` de inmediato. Si el Slave ya había procesado/respondido la
última request mientras el Master aún no había consumido la respuesta, el
Master dejaba de ejecutar `task()` y quedaba una transacción de diferencia.

La evidencia es consistente con ese caso: FC10 y FC03 difieren exactamente en
una operación y no aparece ningún error funcional.

## Corrección R2 del mismo gate

`X` pasa a solicitar un stop quiescente. El Master continúa atendiendo el par
FC10+FC03 en vuelo y sólo confirma `A14_RTU_FC10_MASTER_STOP=PASS` cuando
vuelve a `PHASE_WRITE_START`, antes de iniciar otra escritura.

No se relaja ningún criterio: `SLAVE_CROSS_COUNT` sigue exigiendo igualdad
exacta. No se modifica la librería RTU ni el perfil FAST en esta corrección.
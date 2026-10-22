# Alpha13 — G2 PRE1 — A13-002 TCA startup / EN_IO

Fecha: 2026-10-06

## Clasificación

```text
FINDING=A13-002
CATEGORY=ROBUSTNESS_HARDENING
CLASSIFICATION=ROBUSTNESS_FIX
PRIORITY=P0
CONFIDENCE=HIGH
PRE1=PASS
PRODUCT_STATE=REVIEW_CONFIRMED_RISK
PRODUCT_CHANGE=NO
```

## Alcance

Validar si el arranque del TCA6424A puede declarar I/O listo y habilitar
`EN_IO` después de una configuración parcial.

G3 / A13-004 (atomicidad RMW/shadow) permanece separado.

## Flujo actual

En `cores/jwcontrol/peripherals_init.cpp`:

1. configura `EN_IO` como salida;
2. fuerza `EN_IO=LOW`;
3. llama `jwplcSystemInitState()`;
4. inicia I2C;
5. inicializa periféricos no críticos;
6. prueba conexión TCA mediante `TCA6424A_init()`;
7. limpia shadow lógico de salidas;
8. ejecuta cinco operaciones TCA sin comprobar resultado;
9. fuerza `EN_IO=HIGH`;
10. marca `g_jwplc_peripherals_initialized=true`.

## Riesgo confirmado estáticamente

Las cinco operaciones ignoradas son:

```text
TCA6424A_writeBank(address, 1, 0x00)
TCA6424A_writeBank(address, 2, 0x00)
TCA6424A_setBankDirection(address, 0, 0xFF)
TCA6424A_setBankDirection(address, 1, 0x00)
TCA6424A_setBankDirection(address, 2, 0xFF)
```

Todas propagan fallo correctamente desde el driver TCA/I2C, pero
`initPeripherals()` descarta esos resultados.

Por tanto, un NACK/error posterior al probe inicial puede alcanzar actualmente
la instrucción `EN_IO=HIGH`.

## Readiness

`jwplcSystemInitState()` ejecuta:

```cpp
g_ioState.initialized = true;
```

antes de `jwplcI2C_begin()` y antes de configurar el TCA.

La API:

```cpp
JWPLC_IOView::ready()
```

retorna ese mismo flag. El estado público puede por tanto indicar I/O listo
aunque el arranque I2C/TCA haya fallado o esté incompleto.

## Build real del producto

El board normal usa:

```text
jwplcbasic.build.core=jwcontrol_precompiled_stub
jwplcbasic.build.extra_libs=precompiled/core/JWPLCBASIC/core.a
```

Así, cambiar sólo `cores/jwcontrol/peripherals_init.cpp` no basta para validar
el producto normal.

El archive no cambió entre el baseline Alpha13 y este preflight. Se hereda la
identidad cerrada en Alpha12:

```text
FILE=JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
BYTES=3042444
SHA256=78d0c0ab14f156b96116529e88872340d51877af40d24ba3559f6081e0bf34fb
```

Regla aplicable:

```text
SOURCE_CHANGE
-> ARCHIVE_INVALIDATED
-> SOURCE-FIRST PASS
-> REBUILD
-> ARCHIVE LINK/PARITY
-> PHYSICAL
```

## Contrato G2-P1 — reproducción baseline segura

No se energizarán salidas deliberadamente para demostrar el defecto.

El harness versionado debe:

1. comprobar branch, HEAD, clean/staged y hashes;
2. preservar `core.a`;
3. compilar el core real desde source con el perfil completo `jwplcbasic`;
4. usar instrumentación temporal/restaurable, no un cambio productivo;
5. ejecutar un control normal y cinco piernas de fallo;
6. en cada pierna de fallo impedir físicamente `EN_IO=HIGH`, pero registrar si
   el código baseline intentó habilitarlo;
7. registrar:
   - operación seleccionada;
   - resultado inyectado;
   - `EN_IO_HIGH_REQUESTED`;
   - nivel físico real de `EN_IO`;
   - snapshot de registros TCA;
   - `JWPLC_IO.ready()`;
8. restaurar cualquier modificación temporal byte-for-byte incluso ante error;
9. mantener builds en `%TEMP%`;
10. dejar working tree idéntico al inicio.

Criterio que confirma el defecto baseline para cada una de las cinco operaciones:

```text
OP_RESULT=FAIL
EN_IO_HIGH_REQUESTED=YES
EN_IO_ACTUAL=LOW
IO_READY=TRUE
```

`EN_IO_ACTUAL=LOW` es el interlock del harness, no el comportamiento del
producto baseline. El hallazgo se demuestra porque el código intentó
habilitarlo aun después del fallo.

## Criterio de decisión

```text
G2-P1 PASS -> defecto baseline reproducido -> diseñar fix mínimo A13-002
G2-P1 REVIEW -> harness/precondición insuficiente -> no tocar producto
G2-P1 FAIL_PRODUCT -> sólo si la evidencia contradice el contrato con harness válido
```

No se abre G3 hasta cerrar G2.

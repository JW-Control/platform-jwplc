# Alpha13-G8 P1/P2 — Candidato RTU/RS-485 validado (2026-10-11)

**Estado:** `CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED`, **no es cierre G8**.

## Ejecución del operador

```text
RUN_ID=g8_p1p2_20261011_190500_d59b4022
PREFLIGHT=PASS
NATIVE_CONTRACT=PASS
SOURCE_OVERLAY=PASS
SOURCE_BUILD=PASS
ARCHIVE_REBUILD=PASS
ARCHIVE_MEMBER_BYTE_PARITY=PASS
ARCHIVE_SHA256=f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416
CANDIDATE_REGRESSIONS=PASS_3_OF_3
RTU_ARCHIVE_LINK_SLAVE=PASS_EXACT_L_PATH_L_FLAG_SHA
RTU_ARCHIVE_LINK_MASTER=PASS_EXACT_L_PATH_L_FLAG_SHA
FINAL_AUDIT=PASS
STATUS=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED
PRODUCT_CHANGE=NONE
PHYSICAL_ACCESS=NONE
```

Las compilaciones normales con candidato `01.ModbusRTU_Slave_Holding`,
`02.ModbusRTU_Master_Read` y `03.RS485_Status` terminaron
con código 0. La biblioteca Modbus RTU es `precompiled=full` y
su archive regenerado tiene paridad miembro→objeto y enlace confirmado.
`JWPLC_RS485` se compiló desde fuente, sin archive propio.

La verificación de cambios y 20 casos C++ nativos independientes obtuvo
PASS en CI [38186444071](https://github.com/JW-Control/platform-jwplc/actions/runs/38186444071);
la comprobación de sintaxis del ejecutor terminó PASS en
[38186574868](https://github.com/JW-Control/platform-jwplc/actions/runs/38186574868).
Los fixtures nativos usan mocks y **no equivalen a prueba física**.

## Decisión de continuación

- Preparar adopción **con respaldo y rollback** de tres archivos y
  nuevo hash exacto de archive: `JWPLC_ModbusRTU.cpp`,
  `JWPLC_RS485.cpp`, `libJWPLC_ModbusRTU.a`.
- Al adoptar, compilar consumidores en ruta normal del package
  sin overlays: `--libraries JWPLC/2.1.0/libraries` y exigir rutas
  seleccionadas de librerías, objeto RS-485 recompilado y archive RTU
  precompilado con `-L/-l`.
- No modificar los headers, core precompilado, TFT, FRAM, biblioteca
  RS-485 AutoDirection, autoload ni sus APIs.
- No commit/push productivo, upload, puerto COM o FRAM en la fase
  de adopción. Decidir prueba de banco RS-485 antes de cerrar G8.
- Posteriormente realizar commit Git exacto y actualizar estado;
  no confundir candidato o package local con ZIP publicado.

```text
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_PASS
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
G8_PRODUCT=UNCHANGED
G8_NEXT=CONTROLLED_PRODUCT_ADOPTION
G8_RELEASE_PR_PUBLICATION=NO
```

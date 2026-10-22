# Alpha13-G8 — Adopción productiva controlada preparada (2026-10-11)

## Estado y alcance

El operador confirmó `G8-P0=CLOSED_PASS_READ_ONLY` y
`G8-P1/P2=CANDIDATE_BUILD_PASS_PRODUCT_UNCHANGED`:
`g8_p1p2_20261011_190500_d59b4022`, archive candidato
SHA-256 `f239100fbb6770a3a6f336179fbfa3a4afd27cc16e12c6f239cf05f28f7a1416`,
paridad objeto-archive, enlaces de Slave y Master y 3/3 regresiones PASS.

El candidato corrige A13-010 (contar una vez cada byte realmente consumido,
incluso el que provoca overflow de RX no-bulk) y A13-011
(verificar `HardwareSerial::operator bool()` antes de fijar `_ready`,
mantener enum/API existente). CI nativo: 20 casos PASS.

## Integración real, sin repetir Alpha12

**Runner versionado:** `tools/alpha13/gates/a13_g8_adopt_local.py`.
**BAT:** `tools/alpha13/gates/run_a13_g8_adopt.bat`.

Secuencia atómica bajo control del ejecutor:

1. Preflight de Git limpio, rama exacta y HEAD sincronizado, CLI
   1.5.1 con SHA original, archives core/TFT/FRAM y manifesto
   previo `A13-G8-P1P2-CANDIDATE-1` con todos los PASS requeridos.
2. Validar SHA del archive en `%TEMP%` y los dos candidatos frente
   a los hashes de la corrida P1/P2. Rechazar reconstrucciones
   implícitas que pudieran cambiar el binario.
3. Respaldar con SHA idéntico **solo tres** archivos productivos:
   - `JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.cpp`;
   - `JWPLC/2.1.0/libraries/JWPLC_RS485/src/JWPLC_RS485.cpp`;
   - `JWPLC/2.1.0/libraries/JWPLC_ModbusRTU/src/esp32/libJWPLC_ModbusRTU.a`.
4. Adoptar exactamente los tres bytes candidatos.
   Autotest de contrato exacto tras copiar; Git debe mostrar
   exactamente esos tres archivos sucios, ninguno en staging.
5. Tres regresiones con los archivos del **package productivo normal**,
   no overlay, sin `--library` de candidato, `--libraries` del
   package, build limpio: Slave, Master y RS485 Status.
   Verificar que `JWPLC_RS485.cpp.o` se recompila y que el RTU
   se enlaza exclusivamente con `-L/-lJWPLC_ModbusRTU` y SHA.
6. Auditoría final de archivos modificados e inalterados,
   sin commits ni accesos al hardware.
7. **Si falla algo después del backup:** restauración byte a byte
   de los tres archivos y auditoría de limpieza Git;
   ante problema de rollback, detener y pedir revisión humana.

## Condiciones y riesgos

- `PRODUCT_ADOPTED_REGRESSIONS_PASS` no significa G8 cerrado.
- Por cambiar readiness de UART/RS485, **valorar prueba física**
  de inicio y comunicación real RS485/Modbus RTU antes del cierre.
  El CI de mocks no demuestra señal real ni colisiones.
- No asumir segunda placa/peer, transceptor USB-RS485 ni
  hardware conectado. Preguntar antes de pruebas físicas.
- **No ejecutar `git pull` tras adopción exitosa:** el árbol quedará
  intencionalmente sucio hasta el cierre Git.
- Cualquier commit productivo posterior debe versionar exactamente
  las dos fuentes y el archive RTU con los hashes del manifiesto;
  la publicación ZIP sigue pendiente G10.

```text
G8_P0=CLOSED_PASS_READ_ONLY
G8_P1P2=CANDIDATE_PASS
G8_PRODUCT_ADOPTION=RUNNER_VERSIONED_NOT_EXECUTED_WINDOWS
G8_PHYSICAL_RS485=NOT_EXECUTED
G8_GIT_PRODUCT_COMMIT=NONE
A13-010=CANDIDATE_VALIDATED_NOT_ADOPTED
A13-011=CANDIDATE_VALIDATED_NOT_ADOPTED
G8=CURRENT_NOT_CLOSED
NEXT=RUN_G8_PRODUCT_ADOPTION_BAT
```

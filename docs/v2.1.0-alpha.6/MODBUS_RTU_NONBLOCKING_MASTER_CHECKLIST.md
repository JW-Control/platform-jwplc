# Checklist — JWPLC Modbus RTU Master no bloqueante

## Diseño

- [x] Rama dedicada creada.
- [x] Plan técnico inicial documentado.
- [x] API asíncrona mínima definida.
- [x] Máquina de estados implementada.
- [x] Una sola transacción activa protegida.
- [x] Timeout cooperativo sin espera activa.
- [x] Recepción por frame gap reutilizando el transporte actual.
- [x] FC03 asíncrono implementado en source.
- [x] FC06 asíncrono implementado en source.
- [x] Ejemplo de validación no bloqueante agregado.
- [x] Source fallback temporal activado para no enlazar el archive Alpha5 obsoleto.

## Compatibilidad

- [ ] `readHoldingRegisters()` existente sin regresión.
- [ ] `writeSingleRegister()` existente sin regresión.
- [ ] Slave `task()/poll()` sin regresión.
- [x] `JWPLC_RS485` sin cambios incompatibles en esta iteración.
- [ ] Arduino IDE PASS.
- [ ] Arduino CLI PASS.

## Validación física

- [ ] FC03 asíncrono PASS.
- [ ] FC06 asíncrono PASS.
- [ ] Timeout sin congelar `loop()` PASS.
- [ ] Reconexión PASS.
- [ ] Reset Slave durante transacción PASS.
- [ ] Reset Master PASS.
- [ ] Contador de aplicación continúa avanzando durante timeout.

## Precompilado antes de cierre

- [ ] Regenerar `src/esp32/libJWPLC_ModbusRTU.a`.
- [ ] Auditar símbolos del archive nuevo.
- [ ] Restaurar `precompiled=full` solo con archive validado.
- [ ] Confirmar Arduino IDE usando el archive nuevo.

## Después del motor base

- [ ] Evaluar FC01.
- [ ] Evaluar FC02.
- [ ] Evaluar FC05.
- [ ] Evaluar FC15.
- [ ] Integrar Remote I/O scheduler.
- [ ] Integrar Process Image.
- [ ] Definir watchdog de salidas.

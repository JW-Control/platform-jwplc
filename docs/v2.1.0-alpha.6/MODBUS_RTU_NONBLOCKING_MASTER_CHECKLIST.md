# Checklist — JWPLC Modbus RTU Master no bloqueante

## Diseño

- [x] Rama dedicada creada.
- [x] Plan técnico inicial documentado.
- [ ] API asíncrona mínima definida.
- [ ] Máquina de estados implementada.
- [ ] Una sola transacción activa protegida.
- [ ] Timeout cooperativo sin espera activa.
- [ ] Recepción por frame gap reutilizando el transporte actual.

## Compatibilidad

- [ ] `readHoldingRegisters()` existente sin regresión.
- [ ] `writeSingleRegister()` existente sin regresión.
- [ ] Slave `task()/poll()` sin regresión.
- [ ] `JWPLC_RS485` sin cambios incompatibles.
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

## Después del motor base

- [ ] Evaluar FC01.
- [ ] Evaluar FC02.
- [ ] Evaluar FC05.
- [ ] Evaluar FC15.
- [ ] Integrar Remote I/O scheduler.
- [ ] Integrar Process Image.
- [ ] Definir watchdog de salidas.

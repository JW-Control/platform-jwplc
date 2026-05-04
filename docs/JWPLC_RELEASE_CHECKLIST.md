# JWPLC_RELEASE_CHECKLIST.md

## Checklist para cerrar un alpha o release

### Identificación

- [ ] Confirmar número de alpha/release.
- [ ] Confirmar branch.
- [ ] Confirmar objetivo.
- [ ] Confirmar qué queda fuera.
- [ ] Confirmar si es alpha, beta o release oficial.

---

## Compilación

- [ ] ESP32 Board compila.
- [ ] JWPLC Basic compila.
- [ ] JWPLC Basic Core compila.
- [ ] Sketch vacío compila.
- [ ] Sketch con `Serial.begin` compila.
- [ ] Ejemplos nuevos compilan.
- [ ] Arduino IDE compila.
- [ ] Arduino CLI compila.
- [ ] `Used platform` apunta a la versión correcta.
- [ ] `Used library` no muestra rutas equivocadas.

---

## Pruebas de periféricos

### I/O

- [ ] `digitalRead(I0_0)`.
- [ ] `digitalWrite(Q0_0, HIGH/LOW)`.
- [ ] `digitalReadBlock(I0_X)`.
- [ ] `digitalWriteBlock(Q0_X, bitmap)`.
- [ ] `JWPLC_readInputs()`.
- [ ] `JWPLC_writeOutputs(bitmap)`.
- [ ] Salidas apagadas al arranque.
- [ ] `EN_IO` correcto.

### Display

- [ ] IDLE.
- [ ] USER.
- [ ] retorno timeout.
- [ ] retorno ESC.
- [ ] modo disabled.
- [ ] LEDs correctos.
- [ ] sin superposición.

### Ethernet

- [ ] arranque sin RJ45 no bloquea.
- [ ] arranque con RJ45.
- [ ] conexión posterior.
- [ ] desconexión sin error falso.
- [ ] LED ETH correcto.

### SD / FRAM / RTC

- [ ] SD detectada.
- [ ] SD no bloquea si no está.
- [ ] FRAM lee/escribe.
- [ ] RTC lee hora.

### RS-485 / Modbus

- [ ] `JWPLC_RS485.begin()`.
- [ ] bridge USB-RS485.
- [ ] CRC Modbus.
- [ ] slave holding registers.
- [ ] master read.
- [ ] master write.

---

## Ejemplos clave

- [ ] `DigitalIO_BlockWrite`.
- [ ] `DigitalIO_BlockRead`.
- [ ] `DigitalIO_BlockMirror`.
- [ ] `JWPLC_IO_BlockMirror`.
- [ ] `Ethernet_Display_Status`.
- [ ] `Ethernet_SPI_Coexistence`.
- [ ] `RS485_USB_Bridge`.
- [ ] `ModbusRTU_CRC_Test`.
- [ ] `ModbusRTU_Slave_HoldingRegisters`.
- [ ] `ModbusRTU_Master_ReadHoldingRegisters`.
- [ ] `ModbusRTU_Master_WriteSingleRegister`.

---

## Documentación

- [ ] README principal actualizado.
- [ ] README de librerías actualizado.
- [ ] Docs del alpha actual.
- [ ] Changelog/PreRelease.
- [ ] Limitaciones conocidas.
- [ ] Pendientes marcados.

---

## Versionado y package

- [ ] Actualizar versión en `platform.txt`.
- [ ] Crear ZIP del package.
- [ ] Calcular SHA-256:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-alpha.xx.zip SHA256
```

- [ ] Calcular tamaño:

```powershell
(Get-Item .\jwplc-esp32-2.0.0-alpha.xx.zip).Length
```

- [ ] Actualizar `package_jwplc_index.json`:
  - `version`.
  - `url`.
  - `archiveFileName`.
  - `checksum`.
  - `size`.
  - `toolsDependencies`.

---

## GitHub

- [ ] Commit.
- [ ] Push.
- [ ] PR.
- [ ] Merge si corresponde.
- [ ] Tag.
- [ ] GitHub PreRelease/Release.
- [ ] Subir ZIP.
- [ ] Copiar release notes.

---

## Boards Manager

- [ ] Arduino IDE actualiza índice.
- [ ] Aparece versión correcta.
- [ ] Instala correctamente.
- [ ] Compila sketch mínimo.
- [ ] Sube sketch.
- [ ] Ejemplos aparecen.

---

## Cierre

Antes de cerrar:

- [ ] resumir probado.
- [ ] resumir pendientes.
- [ ] confirmar siguiente alpha.
- [ ] documentar decisiones.

### Alpha30 - Build speed/cache

- [ ] Tabla de tiempos actualizada.
- [ ] Prueba app-only documentada.
- [ ] Conclusión app-only documentada.
- [ ] Prueba A/B con/sin bootloader realizada.
- [ ] SHA-256 de bootloader comparado entre sketches.
- [ ] Decisión sobre bootloader precompilado documentada.
- [ ] FlashFreq final definida o marcada como pendiente explícito.
- [ ] FlashMode/build.boot final definido o marcado como pendiente explícito.
- [ ] Revisión de reducción de menús documentada.
- [ ] PR en español.
- [ ] PreRelease en español.
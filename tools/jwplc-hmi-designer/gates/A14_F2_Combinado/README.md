# Gate A14 fase 2 · prueba física combinada (2.1.0-alpha.14)

Prueba en placa del package integrado, todo precompilado salvo `JWPLC_ModbusRTU`.

- `A14_F2_Combinado.json`: proyecto en forma corta (CLI `jwhmi`); `A14_F2_Combinado.jwhmi`: el mismo, para abrir en el Designer.
- `JWPLC_HMI_Generated.h`: generado con `jwhmi build` (Designer real en Chromium headless).
- `A14_F2_Combinado.ino`: Master en **COM4** (pantalla + Ethernet + RS-485).
- Slave en **COM14**: ejemplo `JWPLC_ModbusRTU/01.ModbusRTU_Slave_Holding` (ID 2, 115200 8N1).

Compila en JWPLC Basic (498 728 B) y Basic Core `[ejecutado 24/10, nube]`. Serial a 115200 en ambas placas.

## Checklist (marcar PASS/FAIL y anotar en `docs/v2.1.0-alpha.14/PRE_RELEASE.md`)

**A. Preparación**
- [ ] Package de la rama `v2.1.0-alpha.14/feature/hmi-designer` enlazado como `jwplc_local`; sketchbook sin copias de librerías JWPLC.
- [ ] COM14 ← `01.ModbusRTU_Slave_Holding`; COM4 ← `A14_F2_Combinado` (FQBN `jwplc_local:esp32:jwplcbasic`). Cable Ethernet a una red con DHCP.

**B. Pantalla (COM4)**
- [ ] Arranque sin destellos ni basura (DISPON diferido); aparece la IDLE.
- [ ] Una tecla despierta al HMI **sin** actuar sobre el contenido ni el MENU.
- [ ] Pestañas PROC / COM / AJUST: ◀ ▶ «posicionada», OK «seleccionada», color por página.
- [ ] Proceso: hora del RTC avanza cada segundo; «°C» se ve bien; BAR sube sin parpadeo; ICON del motor cambia con la bomba.
- [ ] Ajustes (MENU): Consigna, Bomba y Comunicación › Escribir HR1 / Pitido; OK confirma, ESC descarta; «Ir al inicio» vuelve a IDLE.
- [ ] Buzzer: pitido al arrancar y al confirmar en el MENU; Pitido = NO lo silencia.
- [ ] Tras 120 s sin teclas vuelve a IDLE (también desde dentro del contenido).

**C. Comunicaciones**
- [ ] COM: Ethernet = verde e IP en el Serial; `TCP pet.` sube al leer desde la PC (Unit ID 1, puerto 502, HR0..HR3).
- [ ] Escribir HR10 = 1 / 0 desde la PC enciende / apaga la bomba (piloto, ICON y MENU lo siguen).
- [ ] RTU: `RTU OK` sube ~2/s, `RTU error` = 0 y `COM14 HR0` = segundos del Slave; Slave con `CRC=0`.
- [ ] MENU › Escribir HR1 = 250 → Serial COM4 «RTU FC06 HR1=250 OK» y el Slave imprime `HR1=250`.
- [ ] 10 min con TCP y RTU a la vez: sin reinicios, `RTU error` estable, la pantalla sigue respondiendo.

**D. Live Bridge y app**
- [ ] Regenerar e instalar la app: `Generar-e-Instalar-App-Escritorio.cmd` (cambió `electron/main.js`).
- [ ] «⚡ Instalar Firmware Live» graba el Live Bridge nuevo (`JWPLC_HMI_Live_Bridge.ino.bin`, 440 752 B,
      sha256 `d7d839d5…c2e5425a3e91`) sin Arduino IDE.
- [ ] LIVE conectado: el lienzo se ve en la TFT y sigue los cambios.
- [ ] Volver a subir `A14_F2_Combinado` desde el Arduino IDE (el flasheo del Live Bridge reemplaza el programa).

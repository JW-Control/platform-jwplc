# Alpha13-G5 — TCP correctness, candidato integrado

Estado: **PREPARED_NOT_EXECUTED**. Fecha: 2026-10-10.

## Punto de partida y hallazgos observables

G1, G2, G3, G4 y TFT-CLOSURE se encuentran `CLOSED_PASS`.
La rama contiene el `core.a` G3 y `libJWPLC_TFT.a` G4
ya validados; no se vuelven a construir ni alterar.

Las fuentes reales en `JWPLC_Ethernet/src` muestran:

- **Envío parcial/tamaño:** `socketSend` limita cada escritura a
  `W5100.SSIZE`; `EthernetClient::write(buf,size)` anteriormente
  declaraba `size` como escrito cuando la función de socket devolvía
  cualquier valor no cero. Con `size > SSIZE` no queda garantizada la
  transmisión completa aunque la API responda tamaño completo.
- **Lectura con conversión insegura:** `EthernetClient::read(uint8_t*,size_t)`
  pasa tamaño a `socketRecv(...,int16_t)` sin protección; un tamaño
  permitido de `size_t` puede desbordar el argumento firmado.
- **Servidor false-positive:** `EthernetServer::write` devolvía el tamaño
  solicitado aunque no existiera ningún socket `ESTABLISHED` o no se
  confirmaran los envíos.

La planificación anterior agrupa `A13-005 + A13-006` bajo G5. El informe
canónico original con las descripciones individuales no está actualmente
en esta rama ni entre los documentos de transferencia identificados; no
inventar la correspondencia exacta de cada ID. Antes de cierre G5, revisar
dicha correspondencia o documentar explícitamente este límite. La revisión
de código sustenta por sí misma los tres casos probados.

## Corrección candidata y compatibilidad

`tools/alpha13/candidates/g5/EthernetClient.cpp`:

- Conservar firmas Arduino `write` y `read`.
- Fragmentar escrituras TCP en bloques `<= W5100.SSIZE`, contar bytes
  confirmados, acotar todos los fragmentos a **un único timeout total**.
  Ante éxito parcial o fallo, detenerse y `setWriteError`; no retransmitir
  a ciegas bytes potencialmente enviados.
- Limitar `size_t` a `INT16_MAX` antes de llamar a `socketRecv`.

`tools/alpha13/candidates/g5/EthernetServer.cpp`:

- Reutilizar el `write` del cliente para broadcast por sockets válidos,
  retornar 0 sin clientes y el mínimo número confirmado entre los peers.
  Se mantiene API pública y escritura broadcast; el valor retornado pasa a
  reflejar los bytes efectivamente confirmados, no sólo solicitados.

**No modificar** `socket.cpp`, Wi-Fi/ESP-NOW, autoload, código del W5500
de rendimiento Alpha14, Modbus RTU ni APIs públicas. Se conserva el
cliente Modbus TCP cooperativo y sus estados.

## Gate integrado y criterios de seguridad

`tools/alpha13/gates/run_a13_g5_integrated.bat --serial-port COM4`

1. Preflight: rama, worktree limpio, core.a/TFT.a históricos, candidatos
   y contrato source-only de `JWPLC_Ethernet`.
2. Ejecutar `a13_finalizer_portability_selftest.py` por Python.exe explícito
   (F112), validar Arduino CLI.
3. Crear overlay temporal `JWPLC_Ethernet` con los dos candidatos.
   Compilar ejemplos reales Ethernet StaticIP, coexistencia SPI y
   ModbusTCP Client, comprobando que se compilan los objetos modificados
   y se enlaza el `core.a` G3.
4. Compilar sketch temporal con token serial único.
5. Detenerse y solicitar `BANCO_TCP` antes de upload: JWPLC
   desconectado de actuadores, LAN W5500 activa y PC en misma red.
   Se requieren puerto TCP **5008 de entrada al PC** y **5018 en JWPLC**;
   el firewall local puede bloquear la prueba y debe registrarse como
   `REVIEW_ENVIRONMENT`, nunca tratarse como fallo confirmado del producto.
6. Test físico PC↔JWPLC: `5000` bytes de TX de cliente en patrón exacto,
   `128` bytes de respuesta con `read(...,32768)`, broadcast
   servidor de `64` bytes a PC y retorno de 0 sin clientes; logs
   serial, hash de sketch y token irrepetible.
7. **Solo tras PASS físico**, adoptar en producto los dos archivos de
   fuente con respaldo en `%TEMP%`; compilar las tres regresiones
   **normales** desde la librería productiva (no el overlay).
8. Auditoría de bytes candidatos/producto, dos archivos dirty sin staged,
   archive core/TFT intactos; dejar los archivos productivos localmente
   modificados y sin commit para cierre separado autorizado.

En `REVIEW/FAIL` tras adopción se intentará rollback byte a byte. El
firmware temporal ya subido al dispositivo **no** se restaura
automáticamente. Sin red o puerto físico, se detiene antes de adoptar
fuentes productivas. `--skip-physical` prueba compilación pero **no**
satisface aceptación física ni cambia producto.

Resultados bajo `tools/alpha13/results/g5_integrated_*/`, incluyendo
`SUMMARY.log`, `MANIFEST.json`, compilaciones, `UPLOAD_G5.log`
y `PHYSICAL_G5_PC.log`.

Resultado objetivo:

```text
STATUS=PASS_PHYSICAL_NORMAL_SOURCE
PRODUCT_DIRTY_FILES=2
NEXT_GATE=G5_CLOSURE_AFTER_REVIEW
GIT_COMMIT=NO
RELEASE_MERGE=NO
```

Este tooling y candidato no están validados aún por Arduino CLI ni hardware
en la máquina del operador. Si falta alguna ruta, compilación, comunicación
Ethernet o prueba de portabilidad, detener y distinguir causa. No repetir
pruebas anteriores G1–G4.

`OPENPLC=OUT_OF_SCOPE`; `HMI_DESIGNER=OUT_OF_SCOPE`;
`TFT_NEW_FEATURES=OUT_OF_SCOPE`.

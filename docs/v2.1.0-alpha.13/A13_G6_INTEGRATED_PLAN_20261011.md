# Alpha13-G6 — Datagramas: propuesta de alcance y ensayos

**Estado:** PREPARATION_ONLY. Fecha: 2026-10-11.
Rama: `v2.1.0-alpha.13/feature/cleanup-robustness`.

## Línea base y dependencias

G5 está `CLOSED_PASS` en commit documental
`cc965e89311c7a54465818bf8fdda7cca1b59ff7`.
El `JWPLC_Ethernet` en la ruta normal Arduino **compila desde fuentes**.
No volver a probar ni editar G1–G5, `core.a`, `libJWPLC_TFT.a`
ni APIs de TCP salvo necesidad demostrada.

La fuente UDP actual de `JWPLC_Ethernet/src/EthernetUdp.cpp` incluye
hardening heredado de Alpha14 para `parsePacket()` con un solo intento
de drenar, y motor `beginEndPacketAsync()/pollEndPacketAsync()`.
**No** remover ni reescribir estos contratos ya probados.

## Riesgos de fuente revisados

- `EthernetUDP::write(const uint8_t*,size_t)` llama a
  `socketBufferData(...,uint16_t len)` sin limitar previamente
  `size_t`. El offset TX y el tamaño de la memoria W5500
  necesitan límites coherentes.
- `EthernetUDP::read(buf,size_t)` termina llamando a
  `socketRecv(...,int16_t len)` sin acotar tamaños grandes, incluida
  la ruta de descarte `read(nullptr,remaining)`.
- `parsePacket()` puede leer/interpretar encabezados cuando RX
  tenga menos de 8 bytes. El protocolo W5500 antepone 8 bytes.
- `EthernetUDP` inicializa solo `sockindex`, no todos los metadatos
  de puerto/offset/remaining antes de `begin()`; `stop()` deja
  contadores residuales.

Estos son riesgos identificados en fuente, **no afirmaciones de fallos
físicos reproducidos**. El informe canónico original aún debe
consultarse para atribuir exactamente A13-007 frente a A13-008.
En ausencia de esa comprobación, A13-008 queda review/no implementado.

## Prueba G6 prevista

Una única validación combinada, tras calificar los candidatos:

- Contrato estático y test unitario del harness sobre rangos
  `0`, `1`, `2048`, `2049`, `65536` y `size_t` grande.
- Compilación Arduino normal/source-first de Ethernet UDP y ejemplos
  SPI/Modbus TCP, sin recurrir a binario antiguo.
- Prueba PC↔JWPLC en banco: TX UDP acotado al buffer real,
  RX de datagrama completo, lectura parcial (8 de 256),
  recuperación de un siguiente datagrama, `stop/begin`
  y envío cooperativo heredado.
- Logs, hashes, token de firmware y métricas de integridad,
  sin aceptar mera compilación como PASS físico.
- Adopción reversible del producto **después** de los tests,
  regressiones normales, auditoría final y cierre Git automático
  no-force sin `AUTORIZO_COMMIT_G6`.
- Cualquier fallo bloquea el cierre; los errores del harness se
  documentan antes del siguiente intento. No reset/clean ni ZIP.

**Seguridad:** antes de un upload del firmware de ensayo, el operador
confirma que el JWPLC está en banco, fuera de servicio y sin actuadores.
Esta condición no es una autorización Git.

`OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`,
`TFT_NEW_FEATURES=OUT_OF_SCOPE`.

## Implementación de prueba integrada preparada (2026-10-11)

Archivos versionados fuera del producto:

- `tools/alpha13/candidates/g6/EthernetUdp.cpp` y
  `JWPLC_W5x00_Ethernet.h`: límites TX `W5100.SSIZE`, RX
  `INT16_MAX`, socket inválido, header 8 bytes y estado de
  constructor/stop/reopen. `jwplcReadPacketFastDeferred` ya no
  trunca un `size_t` que exceda `UINT16_MAX`.
- `tools/alpha13/firmware/a13_g6_udp_probe/a13_g6_udp_probe.ino`:
  prueba física con token único, escritura solicitada de 65 536 bytes,
  confirmación real de 2 048 bytes, datagrama de 256 bytes parcialmente
  leído, descarte cooperativo, siguiente datagrama de 16 bytes,
  `stop()/begin()` y errores de estado.
- `tools/alpha13/gates/a13_g6_udp_pc.py`: PC UDP puerto 5021,
  JWPLC 5020, validación del payload de ida, lecturas, token y retornos.
- `tools/alpha13/gates/a13_g6_contract_selftest.py`: verificación
  offline de sintaxis Python, nombres de serial, fuente y esquema de
  manifiesto antes de compilación.
- `tools/alpha13/gates/a13_g6_integrated.py` y wrapper
  `run_a13_g6_integrated.bat`: un único gate de inicio a cierre Git.

El runner solo selecciona el source overlay si Arduino CLI confirma
`Using library JWPLC_Ethernet ... in folder` esperado y encuentra el
objeto `EthernetUdp.cpp.o`. Conserva SHA de `core.a` (G3) y TFT.a (G4).
Todas las fases deben marcar PASS antes de adoptar los dos fuentes.
Regresiones Arduino: Ethernet_StaticIP, Ethernet_SPI_Coexistence y
ModbusTCP_Client, 3/3 con overlay y 3/3 con producto.

El flujo de cierre no usa una confirmación `AUTORIZO`: revisa que
la rama remota no haya avanzado, que el alcance exacto sea de dos
archivos productivos y tres documentales, que no exista índice staged,
y hace commits/push no-force. Si falla antes del primer commit tras
adopción, restaura bytes del backup. Si falla después del primer
commit, conserva estado y requiere revisión, **sin reset/clean/rebase**.

Los selftests automáticos están versionados pero todavía no se
han ejecutado en el Windows/Arduino real del operador.

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

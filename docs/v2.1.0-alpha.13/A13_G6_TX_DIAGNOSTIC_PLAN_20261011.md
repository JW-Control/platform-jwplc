# A13-G6 — Diagnóstico TCP/UDP TX con firmware ya cargado

**Estado:** DIAGNOSTIC_ONLY, 2026-10-11.

El run G6 con Arduino CLI 1.5.1 verificó correctamente COM4,
preflight, auto-tests, las tres compilaciones overlay y el upload
con `APP_SHA256=03ea7fbefab1269c3f6de6a51efa39e26d7c50ff3e8cdfd8b3f955a3dc490581`.
El PC recibió un datagrama pero el script terminó en `TX_PAYLOAD_MISMATCH`.
La condición antigua combinaba `address[0] != DUT_IP` con
`data != EXPECTED_TX` y **no imprimía qué predicado falló**.

`a13_g6_udp_tx_diagnose.py` usa el **mismo sketch actualmente cargado**
y el token guardado en un único manifiesto elegible. No hay compilación,
upload, adopción de fuentes ni Git mutado. Repite solamente `G6_TX`,
con PC UDP 5021, JWPLC 5020, y registra:

- IP y puerto de origen UDP, comparados con la IP anunciada del DUT;
- longitud real vs 2048 solicitados en el ensayo;
- hash SHA-256 de bytes recibidos y primera discrepancia;
- los primeros 32 bytes; los indicadores seriales de `G6_TX`;
- salida JSON en `tools/alpha13/results/g6_integrated_*/G6_TX_DIAGNOSTIC.json`.

La prueba física G6 **permanece REVIEW** y nada productivo se adopta
hasta identificar la causa exacta. Debe cerrarse el monitor serie de
COM4 antes de iniciar el diagnóstico. El JWPLC continúa aislado de
actuadores: no es una prueba en máquina productiva.

F117 se reserva para este fallo de diagnóstico ambiguo del harness y
la posible diferencia de datagrama, sin prejuzgar su causa.

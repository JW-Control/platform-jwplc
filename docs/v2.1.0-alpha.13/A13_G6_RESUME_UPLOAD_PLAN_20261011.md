# Alpha13 G6 — reanudación tras diagnóstico de fragmentación UDP

Estado: **RESUME_TOOLING_PREPARED_NOT_EXECUTED** (2026-10-11).

En G6 R2 ya se validaron Arduino CLI 1.5.1 oficial, core
`jwplc_local:esp32`, regresiones overlay 3/3 y subida de firmware
`03ea7fbefab1269c3f6de6a51efa39e26d7c50ff3e8cdfd8b3f955a3dc490581`.
El receptor anterior falló al exigir **un** datagrama de 2048 bytes.
El diagnóstico posterior recibió dos datagramas UDP del JWPLC
(1472 y 576 bytes), con concatenación exacta. Esto concuerda con
la documentación W5500 de división automática de UDP mayor al MTU.

No se interpreta división por UDP como reensamblaje IP transparente.
Las aplicaciones de nivel usuario que esperen **un único mensaje UDP**
deberán respetar el MTU o definir fragmentación/reensamblaje propios.
Este hito conserva el comportamiento W5500 ya observado, lo documenta
y NO reescribe las funciones cooperativas de Alpha14.

El host `a13_g6_udp_pc.py` acepta formas estrictas
`[2048]` o `[1472,576]`, exige bytes en orden, mismo origen
IP/puerto y coincidencia exacta. El reanudador
`a13_g6_resume_uploaded.py` ejecuta ocho fixtures de contrato,
verifica manifiesto, SHA de candidatos, firmware y ejecutable CLI,
nueve logs originales con exit 0 y el JSON del diagnóstico. No
recompila el overlay, no repite upload ni modifica firmware cargado.

Sólo tras PASS físico completo PC↔JWPLC, adopta dos fuentes
protegidas con copia de seguridad, ejecuta 3 regresiones normales,
audita core.a/TFT.a y Git, genera cierre documental y commits/push
**no-force automáticos**, sin `AUTORIZO_COMMIT_G6`.

Los resultados de reanudación se conservan en
`tools/alpha13/results/g6_resume_*/`; el fallo anterior permanece
intacto. Si falla después del primer commit no se ejecutan comandos
destructivos y el estado queda para recuperación manual.

Prueba aún **no realizada en el entorno Windows del operador**.

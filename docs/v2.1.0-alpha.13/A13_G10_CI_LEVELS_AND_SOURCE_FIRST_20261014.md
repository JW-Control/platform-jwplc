# Alpha13 G10-I — aplicación CI por niveles y política de precompilados

**Decisión:** 2026-10-14, aprobada por el responsable JWPLC. **Estado:** diseño operativo; implementación del cambio de workflow todavía pendiente. 
**Norma interna:** `jwplc-dev-mcp/knowledge/global/workflows/THREE_LEVEL_CI_AND_PRECOMPILED_SOURCE_FIRST_20261014.md` (commit `e57b68c9` en rama privada `docs/private-knowledge-foundation`).

## Niveles y criterios
- **N1 — dirigido por cambio:** cuerpo real Ethernet / Modbus TCP, mutaciones negativas, contención SPI y ownership/epoch. Cada push relevante. No requiere 11 sketches.
- **N2 — integración compacta:** como objetivo 3 sketches de prueba versionados: TCP+Modbus TCP, UDP+DNS+DHCP, Ethernet+SPI/coexistencia; compilan fuentes y verifican link y autoload. Ninguno de estos sketches se considera creado por esta decisión.
- **N3 — regresión de cierre:** conservar 11 ejemplos representativos en baseline y candidato (22 compilaciones) antes de presentar candidato físico, al cerrar G10-I y frente a cambios amplios de plataforma/bus/precompilados. No ejecutarlos en cada cambio documental o ajuste pequeño del harness.

## Comprobación de precompilados
- `JWPLC_Ethernet/library.properties`: no declara `precompiled=full`; actual se compila desde fuentes. Para G10-I garantizar que el overlay concreto se selecciona, y que `.d`, objeto y ELF corresponden al HEAD editado.
- `JWPLC_ModbusRTU/library.properties`: `precompiled=full`.
- `JWPLC_Display/library.properties`: `precompiled=full`, `dot_a_linkage=true`.
- Otros archives congelados del package: `JWPLC_TFT`, `JW_SD`, `SPI`; preservar hashes existentes si no son editados.
- **Cuando X precompilada se modifique**, en el package *aislado* de desarrollo forzar X source-first durante TODO el desarrollo del alpha. Asegurar que no existe fallback al `.a` anterior. Si hace falta retirar archive y modificar `library.properties`, hacerlo únicamente en stage temporal reproducible; **no** borrar el archive histórico del producto.
- Al terminar los cambios, reconstruir archive X desde fuentes finales con receta versionada, verificar hashes/ELF, ejecutar N3 y preparar benchmark real de compilación precompilada en PC. La regeneración de un archive por sí sola no lo autoriza como definitivo.

## Riesgo y autorización
Se mantiene `G10-I=OPEN`, `B03=OPEN`, `A13-006=ADOPTION_BLOCKED`, `G10-H=NOT_STARTED`. Los cinco archives y GPIO4 no se tocan. CI verde por compilación prueba compatibilidad de build, **no** corrección de B03 ni coexistencia física. Aplicar cambios de YAML en commit separado y revisar que N3 permanezca disparable manualmente antes de reducir su frecuencia.

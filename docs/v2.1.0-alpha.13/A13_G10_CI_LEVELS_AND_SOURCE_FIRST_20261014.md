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

## Implementación inicial CI — 2026-10-14

Commit `e53dbb11a95da5aa5ca9d2b7a20553cf6e7335f5` modifica `.github/workflows/alpha13-g10-integrated.yml`:

- El push normal ejecuta N1 (overlay-compose, dos sistemas operativos con detector B03) y N2 (Arduino CLI baseline y candidato para tres sketches **existentes**: `01.ModbusTCP_Server`, `Ethernet_SPI_Coexistence`, `01.Ethernet_DHCP_Basic`). Son seis compilaciones de bibliotecas, no todavía tres sketches nuevos que ejerciten todas las funciones.
- N3 `full-platform-review` conserva exactamente 11 sketches por variante, pero se restringe a `workflow_dispatch` con entrada `regression_level=full`. Valor por defecto del disparo manual: **full**, para reducir el riesgo de saltarse la regresión de cierre.
- No se deshabilitaron archives ni se modificó firmware. Ethernet continúa source-first según `library.properties`.
- El push de este workflow debe revisarse en Actions antes de declarar PASS. El modo automático para cambios amplios de core/SPI/precompilados queda **pendiente**; por ahora activar N3 manualmente frente a dichos cambios, antes de hardware y cierre. También siguen pendientes sketches N2 dedicados UDP/DNS/DHCP y la corrección B03.

**Precaución:** el CI verde existente para el riesgo B03 reconoce la reproducción de un defecto, no verifica su corrección.

## N2 ampliado con cobertura de API DNS/UDP (2026-10-14)

- Sketch de integración de API `tools/alpha13/sketches/G10_UDP_DNS_DHCP_Integration/G10_UDP_DNS_DHCP_Integration.ino` incorporado al candidato de CI: commit `f6ff67fd`.
- Matriz N2 baseline/candidato actualizada para compilarlo: commit `8925b348`. Ahora el job incluye **cuatro** sketches por variante, todavía menos de once.
- Sketch intencionalmente no activa tráfico sin disparador serie; su PASS prueba compilación/enlace de métodos DNS y UDP, no el comportamiento de DHCP en red ni la resolución de B03. La cobertura DHCP funcional continúa en harness y hardware futuro.
- No se cambió la librería productiva, el overlay B03, los cinco precompilados ni GPIO4.
- **CI de este nuevo commit: PENDIENTE DE CONFIRMACIÓN.** Verificar Actions antes de declarar PASS. B03 permanece OPEN y A13-006 ADOPTION_BLOCKED.

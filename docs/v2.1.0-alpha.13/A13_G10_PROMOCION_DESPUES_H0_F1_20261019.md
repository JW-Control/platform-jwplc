# Alpha13 — Integración G10 al package después de H0-F1

**Decisión del propietario:** F1 aprobado; primero H0 extendido (10 EN + 10 power-cycle **solo COM4**); **después** integración G10.  
**Estado actual:** PLAN DE PROMOCIÓN / **CÓDIGO NO MODIFICADO / G10 NO INTEGRADO**.  
**Base productiva aprobada para H0:** \`v2.1.0-alpha.13/feature/cleanup-robustness\` @ \`2b5802720df552e01a0b4f9d4fe1a7bd2de16daf\`.  
**Bloqueo de inicio:** \`H0_F1_EXTENDED_10_EN=PASS\` y \`H0_F1_EXTENDED_10_POWER=PASS\`, o revisión explícita del operador si existe resultado parcial. Las 20 maniobras **aún no se han presentado como ejecutadas**.

## 1. Qué ya está demostrado

- F1 productivo corregido en \`JWPLC_Ethernet/src/utility/w5100.cpp\` y compilación source-first de esa corrección.
- Early-link físico \`H0F1_EARLY_LINK_RETURN=2\`, \`STAGE=2\`, 15 min reportados sin watchdog, PHY LinkOFF/LinkON, y una reconexión \`LINK=1→2→1\` con mismo BOOT y contador hasta 1600.
- G10-I y G10-H registran pruebas nativas, N2/N3 y un smoke físico limitado sobre **overlays experimentales**, sin adopción del package. Esos PASS se conservan con su SHA y límites; el PR de producto G10 no se declara verde por mera existencia de un overlay.

## 2. Inventario de fuente de G10 y separación de laboratorio

**Compositor G10 funcional** \`tools/alpha13/gates/a13_g10_integrated.py::compose()\` parte de las fuentes reales y combina, en orden, los parches de cliente/servidor, capa \`BusScope\`, socket leases y owner/epoch, UDP/cancel/stop, servicios cooperativos y ModbusTCP. Los siguientes archivos destacan en el inventario: 
- \`JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.h\` y \`.cpp\` (2 nuevos, candidato);
- \`JWPLC_Ethernet/src/EthernetClient.cpp\`, \`EthernetServer.cpp\`, \`EthernetUdp.cpp\`, \`socket.cpp\`, \`JWPLC_W5x00_Ethernet.h\`, \`JWPLC_Ethernet.cpp\`;
- \`JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp\`, \`JWPLC_ModbusTCP_Client.cpp\`, y sus cabeceras; 
- fuentes adicionales reales que el compositor reporte mediante su manifest completo. **La lista de adopción definitiva se genera por diff compuesto, no se infiere de estos ejemplos.**

**Compositor G10-R experimental** \`a13_g10_r_build_candidate.py\` genera **15 fuentes** al añadir al overlay G10-I \`trace\`, \`lab\`, \`TCP124\`, \`B2 DHCP\` y etapas \`H0\`. Es explícito en los comentarios del propio script que:
- \`h0_w5100_stage\` y \`h0_init_stage\` **modifican experimentalmente** \`utility/w5100.cpp\` para checkpoints 60–77;
- \`h0_link_stage\` incluye marcas RTC y sentinela \`LINK=-9\`;
- \`G10RLabDhcpProbe\` introduce \`friend class\` en cabecera **solo** para banco;
- \`LabSocketSnapshot\`, \`labCaptureSockets\`, trazas de propietario, \`G10R_R3_* \` y observación UART son instrumentación.

**No copiar los 15 archivos experimentales al producto.** En particular, NO sobrescribir el \`w5100.cpp\` corregido por F1, NO promover el \`friend\` de laboratorio ni los RTC checkpoints, y NO dejar expuesto un reset GPIO4 o declaraciones instrumentales que no correspondan a API de usuario.

## 3. Plan técnico posterior a la campaña H0

| Paso | Trabajo | Salida verificable |
|---|---|---|
| G10-P0 | Aislar rama desde HEAD actualizado de Alpha13; generar diff compuesto \`G10-I\` vs package F1, listar todos los paths y hashes; clasificar funcional/laboratorio/precompilado | Inventario exacto, matriz API, conflictos y decisión de alcance |
| G10-P1 | Promover SafeAccess/ownership y SPI mutex **no recursivo** con negativos de doble lock y sockets stale; mantener APIs públicas y configuración HW desde package | N0 source diff y N1 source-backed + negativos |
| G10-P2 | Promover EthernetClient/Server/UDP, close diferido, DHCP/DNS y lifecycle de sockets; evitar volver a leer runtime legacy y anular wrappers previos (riesgo B08) | N1 dirigido y tests de regresión composicional |
| G10-P3 | Promover interoperabilidad Modbus TCP server/client y mantener coexistencia RTU; mantener autoload de todos los periféricos integrados | N1 consumidor real, N2 Arduino FQBN/objetos/ELF |
| G10-P4 | Ejecutar N3 del **package en rama** sobre 11 sketches representativos y pruebas negativas del CS F1, comparar hashes de los cinco archives congelados; comprobar Arduino IDE o fijar su gate | N3 source-first del package, API heredada intacta, sin divergencia de archivos congelados |
| G10-P5 | Solicitar nueva autorización para pruebas físicas **de G10** (no reutilizar autorización EN/power de H0 como si fuese permiso R1–R4). Ejecutar campaña R1–R4 conforme a protocolos y seguridad B4 | Evidencia de recuperación DHCP/DNS, lifecycle de sockets, concurrencia y endurance |
| G10-C | Reconciliar fallos, scope, \`ALPHA13_STATUS.md\`, FAILURE, PR en español y conocimiento privado; lista de pendientes de promoción vacía o explícita | PR listo para revisión, gates y bloqueo/decisión de release |

## 4. Invariantes obligatorios

- **Estabilidad y APIs** primero. No desactivar autoload normal de Display, Ethernet, SD, FRAM, RTC, botones, RS-485, Modbus RTU y TCA/I/O.
- No tocar variante para configurar hardware del usuario. \`JWPLC_ETH_CS=5\` y guarda GPIO6–11 deben seguir vigentes; no sobrescribir F1 con \`utility/w5100.cpp\` experimental.
- No introducir \`GPIO4\` reset, stack OpenPLC, OTA supuesto, cambio FlashFreq definitivo ni \`bootloader.bin\` final.
- No confundir candidata \`SafeAccess\` con tests que usen mutex recursivo cuando el producto usa mutex **no recursivo**.
- Nada de promoción sin diff revisado y gates que demuestren **fuente → .d → .o → ELF**; los \`.a\` congelados se conservan con SHA.
- Hasta nueva autorización, **sin flasheos G10, sin COM14, sin R1–R4, sin borrado de flash ni publicación**.
- Incorporar todos los cambios al package real del alpha antes de declararlo cerrado, no dejarlos solo en overlay. Documentar cualquier excepción como \`PENDIENTE_DE_PROMOCIÓN\`.

\`\`\`text
F1_APPROVED_BY_OWNER=YES
H0_F1_EXTENDED_EN_10=AUTHORIZED_NOT_RUN
H0_F1_EXTENDED_POWER_10=AUTHORIZED_NOT_RUN
G10_PRODUCT_PROMOTION=PLANNED_BLOCKED_ON_H0_EXTENDED
G10_BASE_F1_SS_PIN=PROTECTED
G10_DIAG_W5100_RTC_FRIEND=EXCLUDED_FROM_PRODUCT
G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
NEXT_GATE=H0_F1_EXTENDED_REPORT_THEN_G10_P0_EXACT_OVERLAY_INVENTORY
\`\`\`

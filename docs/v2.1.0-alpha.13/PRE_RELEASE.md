# v2.1.0-alpha.13 — Robustez Ethernet W5500, SPI compartido y sockets Modbus TCP

## Evidencia de auditoría e integración previa a este lanzamiento (21/10/2026)

La auditoría de G1–G10, H0-F1 y F137, sus límites y las decisiones reproducibles están en [A13_C_AUDITORIA_GLOBAL_Y_DISTRIBUCION_20261021.md](A13_C_AUDITORIA_GLOBAL_Y_DISTRIBUCION_20261021.md). El workflow antiguo de composición G10 conserva el fallo histórico de harness `CLIENT_CTOR_CHANGED` previo a la compilación, sin implicar una regresión del producto. El gate release-like [#39641744857](https://github.com/JW-Control/platform-jwplc/actions/runs/39641744857) verificó ZIP byte-a-byte, dos `.a`, dos `library.properties`, índice simulado y compilación Arduino CLI en ambos FQBN. La **verificación de instalación desde Boards Manager publicado** constituye un gate postpublicación distinto; consultar su evidencia en el estado vivo de la rama base. Estas notas describen el contenido de la PreRelease y no inventan un checksum o estado de instalación que corresponde al asset efectivo.

## F137 — integridad de los archivos precompilados incluidos (21/10/2026)

Los archives `libJWPLC_Ethernet.a` y `libJWPLC_ModbusTCP.a`, sus flags `precompiled=full` y la política Ethernet `weak Basic / strong Basic Core` fueron integrados al package mediante [PR #125 MERGED](https://github.com/JW-Control/platform-jwplc/pull/125), commit `23fae3a3febf8e6a9c19e7e8cb63b7b6a62cdb11`. El árbol funcional de referencia antes de los nuevos README es `cc17716f173509b1e0f3c0df5eb99549014c1d61` (N3). El árbol completo posterior, con **solo dos README de librería ampliados**, es `6b498f4a0797e2a1bb0054b005b91f985bc071e2`; las fuentes, propiedades y archivos `.a` aprobados no cambiaron. Se mantienen autoload, configuración interna de pines y APIs. Los hashes de los assets distribuidos se verifican contra los valores publicados en el índice real.

**Mediciones de compilación de CI, mediana fría fuente → archive, tres pares por sistema/perfil (no confundir con PC local):**
- Ubuntu Basic: 27,245 → 18,976 s (−30,4 %); Basic Core: 35,065 → 26,141 s (−25,4 %).
- Windows Basic: 85,163 → 51,945 s (−39,0 %); Basic Core: 95,054 → 79,908 s (−15,9 %).
- Las compilaciones calientes cambiaron poco; **NO se midió ahorro de tiempo de upload**. El bootloader del build local no queda fijado como binario definitivo del package.

**Evidencias:** N3 13/13 Windows+Ubuntu [#39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047); compilaciones Master/Slave Windows+Ubuntu [#39622285204](https://github.com/JW-Control/platform-jwplc/actions/runs/39622285204); H física dual por operador con COM4 Master y COM14 Slave, RS-485, IP distintas, `PHY=ETH=TCP=1`, FC03 a ambos **4/4 registros**, eco UDP COM4 **3/3**, heartbeat y periféricos observados. Ver `A13_F137_H_DICTAMEN_FISICO_20261021.md`. **H=PASS_FUNCIONAL_ACOTADO**, no representa un test de Basic Core físico ni R4 prolongado, DNS/DHCP renewal, E1/E2 o versión literal de UART BOOT no capturada. No inventar validaciones post-publicación.

La integración productiva F137 y la huella del package están verificadas. La aceptación técnica pre-release no sustituye la verificación posterior del ZIP publicado, índice dev, instalación Arduino IDE/CLI y cualquier prueba física adicional expresamente autorizada.

---

**Canal:** PreRelease de desarrollo para Arduino IDE/CLI. **Origen del código:** rama Alpha13 tras [PR #124](https://github.com/JW-Control/platform-jwplc/pull/124), merge `3467a375ccaf7c7ee27220ed0bc348056e5e8219`. Los metadatos de publicación (tag, ZIP y checksum) pertenecen al GitHub Release y al índice de Boards Manager una vez generados; estas notas no los anticipan.

## Guías para un primer JWPLC Basic

Para empezar por el nivel básico y avanzar sin configurar pines ni buses,
consulta `JWPLC_Ethernet/README.md` (DHCP, TCP/UDP, luego operaciones cooperativas
avanzadas opcionales) y `JWPLC_ModbusTCP/README.md` (Server/Client, FC03 y
después los ocho FC implementados, sin atribuir cobertura física a los que
no la tienen). Las operaciones avanzadas se separan de la API recomendada.

**Benchmark Windows local final:** el operador ejecutó en su PC la campaña de tres parejas frío/caliente por perfil, fuentes contra dos `.a`, y aportó el manifiesto que se conservó en [evidencia local](evidence/A13_LOCAL_PC_BENCHMARK_MANIFEST_20261021.json). **JWPLC Basic**: fría 52,120 → 40,190 s (**−22,89 %**), caliente 9,251 → 9,055 s. **JWPLC Basic Core**: fría 54,964 → 47,958 s (**−12,75 %**), caliente 9,175 → 9,146 s. Todos son valores de **mediana de 3** por escenario en **su PC**, diferentes de Windows/Ubuntu CI. La evaluación íntegra está en [A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md](A13_LOCAL_PC_BENCHMARK_RESULTADOS_20261021.md). `LOCAL_PC_BENCHMARK=PASS_LOCAL_PC_THREE_PAIRS_TWO_FQBN`, con 12 runs, 24 tiempos y hashes `.a` confirmados. **No se midió velocidad de subida USB**; el ensayo solo aplica a compilación.

## Alcance

Alpha13 consolida mantenimiento, robustez y corrección de fallos del package Arduino JWPLC Basic v2.0.0, respetando las APIs existentes y el autoload de Display, Ethernet W5500, SD, FRAM, RTC, botones, RS-485, Modbus RTU, Modbus TCP y TCA/I/O.

- **F131/F1, chip-select W5500:** se adopta `JWPLC_ETH_CS=5` como fuente del SS de Ethernet en el código `utility/w5100.cpp` y guarda de compilación contra GPIO6–11 (flash del ESP32). Se corrige el riesgo latente de que el fallback a GPIO10 provoque watchdog en enlace temprano.
- **SPI y ownership de sockets:** dos fuentes nuevas `JWPLC_W5500_SafeAccess.h/.cpp`, mutex SPI no recursivo, propietario y generación de socket, retorno explícito de bus ocupado o token obsoleto, y cierre diferido cuando el bus no está disponible.
- **Ethernet:** mejoras internas en `EthernetClient`, `EthernetServer`, `EthernetUDP`, `JWPLC_Ethernet`, `JWPLC_W5x00_Ethernet` y `socket.cpp` para TCP/UDP cooperativo, tratamiento seguro de `BUS_BUSY`, lifecycle y preservación de sesiones.
- **Modbus TCP:** mejoras en cliente/servidor y cabeceras correspondientes para cierre, lectura de estado, reconexión y coexistencia con RTU. Se mantienen las APIs y funciones Modbus ya soportadas en Alpha12.

**Alcance productivo exacto G10:** 10 archivos modificados y 2 añadidos en `JWPLC/2.1.0/libraries/JWPLC_Ethernet` y `JWPLC/2.1.0/libraries/JWPLC_ModbusTCP`; los cambios del overlay de laboratorio no se promueven. `Dns.cpp` y `Dhcp.cpp` no fueron modificados directamente durante la promoción G10.

## Validación de ingeniería

- N0: contrato exacto y preservación F1/archives; CI Windows y Ubuntu.
- N1: contratos nativos C++ y paridad SHA 14/14 con el compositor funcional; CI Windows/Ubuntu.
- N2: dos perfiles FQBN JWPLC Basic y Basic Core, compilaciones sobre fuentes del package real, dependencia `.d`, objetos y ELF verificados.
- N3: 11 sketches de integración con Display, Ethernet, I/O, RS-485, Modbus RTU/TCP, FRAM, RTC y SD. Autoload normal intacto.
- PH0: upload y arranque reales COM4 Master/COM14 Slave, dos IP DHCP de router aislado, FC03 Modbus TCP en ambos y comunicación RS-485 RTU.
- R1: **605 s** con 589 lecturas FC03 correctas por cada servidor, 589 ecos UDP y contadores UART posteriores coherentes (RTU, SD, FRAM, RTC, TFT, I/O).
- R2: **3/3 ciclos de red** con recuperación TCP+UDP en 1,69 / 10,05 / 6,03 s; **2 transiciones PHY reales** adicionales en UART con mismo BOOT y progreso RTU/periféricos.

Fuentes y criterios de no repetición: `A13_G10_PROMOTED_N0_N3_20261019.md`, `A13_G10_H_DICTAMEN_CIERRE_ACOTADO_20261020.md` y `ALPHA13_STATUS.md`.

## Limitaciones reconocidas

- R3 externo: cierre y reapertura **secuencial** de sesiones TCP queda observado en las 589 lecturas R1 por PLC, pero **no** se completó fault injection física interna para `BUS_BUSY`, leases `Stale`, close diferido concurrente ni cancelación async DNS/DHCP/TCP.
- R4 canónico: duración de 1805 s **NO ejecutada** por decisión del operador de abreviar. Los 605 s de R1 no se presentan como equivalentes a endurance/ausencia absoluta de starvation SPI.
- No se probó resolución DNS ni renovación/rebind de DHCP interna; `PHY=1` y DHCP READY no acreditan por sí solos estos protocolos.
- E1 `PENDING_GDB_UNAVAILABLE`; E2 `PENDING_SDKCONFIG_NOT_LOCATED`. La prueba H0 extendida 20/20 estricta conserva brechas de captura aceptadas con observación, no se transforma en PASS perfecto.
- No existen afirmaciones de determinismo duro de RTU, OTA definida, OpenPLC integrado, FlashFreq universal definitivo o `bootloader.bin` publicable.
- **Cobertura de distribución distinta de ingeniería:** la instalación desde el índice publicado, la compatibilidad Arduino IDE GUI y la subida desde el package distribuido requieren evidencias postpublicación independientes. Ningún N3 o release-like se anuncia como sustituto.

## Seguridad, compatibilidad y distribución

Se mantiene el perfil de hardware y periféricos de JWPLC Basic sin configuración de pines por el sketch. No tocar `boards.txt`, `platform.txt`, `GPIO4` ni los archives precompilados congelados solo por velocidad. No cambiar el canal estable: Alpha13 es **PreRelease de desarrollo**, dev index únicamente.

**Distribución:** versión `2.1.0-alpha.13`, carpeta raíz esperada dentro del ZIP `2.1.0/`, canal **dev** de Boards Manager (no sustituye `v2.0.0` estable). El GitHub Release debe exponer el ZIP y su SHA-256/tamaño real; el índice dev se actualiza mediante PR hacia `main`. La instalación, compilación y carga desde el **package ya distribuido** forman parte del gate postpublicación y no deben confundirse con las pruebas PH0 realizadas sobre la fuente candidata.

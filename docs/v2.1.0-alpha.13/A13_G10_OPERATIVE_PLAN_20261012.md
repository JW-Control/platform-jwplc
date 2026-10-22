# Alpha13 G10 — Plan operativo de conciliación, verificación y freeze

**Fecha:** 2026-10-12.  
**Estado:** `PREPARED_P0_READ_ONLY_PENDING`; no cambios de producto ni release.
**Rama:** `v2.1.0-alpha.13/feature/cleanup-robustness`.
**Previo:** `G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS`.

## Alcance exacto de auditoría canónica

La auditoría `Markdown pegado.md` de 22 hallazgos define:

| Hallazgo | Área canónica y referencia | Tratamiento actual |
|---|---|---|
| A13-005 | `EthernetClient::write()` cuenta bytes enviados | G5 tiene pruebas 5000/128; reconciliar como ADOPT corregido |
| A13-006 | `JWPLC_ModbusTCP_Client::closeSession()` pierde referencia al socket si falla SPI | **REVIEW abierto**; G5 corregía EthernetClient/Server, no esta ruta |
| A13-007 | `DhcpClass::parseDHCPResponsePacket()` usa lecturas/longitudes sin validación completa | **REVIEW abierto**; G6 UDP no prueba DHCP malformado |
| A13-008 | `EthernetUDP::endPacket()` espera sin deadline software | **REVIEW abierto**; G6 prueba UDP normal, no stall indefinido |
| A13-009/013 | FRAM lectura robusta y limpieza interna | G7C cerrado; comprobar resolución real en fichas específicas antes de freeze |
| A13-015 | Smoke CI `ci-jwplc-package-smoke.yml`: faltan TCP, RTU Master, Core y fuente/archives | ADOPT abierto |
| A13-016 | Gate de frescura `alpha12_precompiled_global_audit.ps1` se basa en fechas y no en todos los insumos | ADOPT abierto |
| A13-018 | `Run-JWPLCBuildBenchmark.ps1` etiqueta `BinaryBytes` sin demostrar tamaño de aplicación | ADOPT abierto |
| A13-019/021 | Consumidores externos de profiler y ownership de SPI FRAM | DEFER explícito, no intervenir APIs sin contrato |
| A13-020/022 | Qualification ModbusTCP y includes de autoload | REJECT; preservar funcionalidades e includes |

Las decisiones `REVIEW` no son `PASS` ni `DEFER` por defecto.
Cada una requiere evidencia nueva y resolución **ADOPT, EXPERIMENTAL
CONCLUSION o DEFER aprobado con riesgo explícito**. No confundir
la aprobación de un gate de red con todos sus hallazgos asignados.

## Orden de ejecución

### G10-P0 — Baseline de procedencia, sin cambios (siguiente)

Capturar en Windows (y CI Win/Linux) HEAD, Git limpio, SHA-256
de fuentes relevantes y archives cerrados; señalar mediante
criterios **read-only** si persisten los patrones A13-006,
A13-007 y A13-008. Enumerar y registrar el estado
A13-015/016/018. Documentar `G7C` y decisiones
existentes sin inferirles nuevos PASS.

El gate P0 **no compila, no sube, no cambia contenido,
no modifica índices Git y no toca COM4/COM14**. Un PASS
es únicamente una foto reproducible del estado.

### G10-P1 — Conciliación de riesgos de red

Primero A13-006: diseñar y verificar ownership/cierre
diferido frente a fallo del lock SPI sin pérdida de socket.
Después A13-007: DHCP robusto con cabecera, opciones y
longitudes válidas antes de aplicar lease, tests byte-a-byte
y renovaciones. Finalmente A13-008: experimental con
flags pendientes perpetuos, éxito tardío, timeout hardware
y deadline parametrizado; decidir corrección interna.
**Una variable principal por gate**, pruebas nativas negativas
y, cuando haya cambios, regresiones de Ethernet y ModbusTCP,
sin tocar API. No saltar a `G10_FINAL` si no existe
decisión/aceptación explícita para cada REVIEW.

### G10-P2 — Robustez de verificación del package

- A13-015: añadir smoke CI que realmente seleccione
  biblioteca y compile TCP, RTU Master y Core; registrar
  paridad de source/archives en cada paquete cambiado,
  conservando el bootstrap Arduino del workflow histórico.
- A13-016: **nuevo** fingerprint que incluya fuente,
  headers transitivos, boards/platform flags y toolchain.
  Fechas Git son indicio histórico, no prueba SHA.
  Mantener script Alpha12 intacto y admitir
  bibliotecas precompiled full con maintenance especial.
- A13-018: medir tamaños de `.elf`, `.bin`, flash/RAM
  con unidades, origen y estado de dato; reportar `UNKNOWN`
  cuando faltan artefactos. Diferenciar tamaño del paquete
  de tamaño de la aplicación; no inventar mejorías.

### G10-P3 — Regresión final y freeze

Matriz 22/22 hallazgos con evidencia, política de
autoload y APIs, variantes Basic/Core, core y archivos
precompilados; pruebas físicas ya registradas con límites
(G8 RTU 12×FC03 y G9 microSD raíz read-only/RTC/UP);
tabla de tiempos y tamaños del package donde sean medibles,
reproducibilidad en Arduino CLI/IDE, logs y hashes.
No descartar o sobreescribir evidencias anteriores.

Resolver la situación de los dos bancos: COM4 conserva
sketch G9, COM14 sketch G8. **No restauración automática**,
y no retorno a maquinaria sin reprogramar aplicación.

Mantener `F120_POST_UPLOAD_FREEZE_ROOT_CAUSE=UNKNOWN_PENDING`
si no aparece una prueba específica. La actividad durante
tres heartbeats no demuestra ausencia de fallos posteriores.

### G10-P4 — Documentación de release, sujeta a aprobación

Propuesta de PR, texto PreRelease y checklist final **en español**,
únicamente cuando pase P3 y se resuelvan los `REVIEW` canónicos
o exista decisión excepcional aceptada. **No abrir PR ni
publicar/mergear** sin aprobación explícita del usuario.

## Reglas de control

- `OPENPLC=OUT_OF_SCOPE`, `HMI_DESIGNER=OUT_OF_SCOPE`,
  `TFT_NEW_FEATURES=OUT_OF_SCOPE`.
- No quitar periféricos del autoload para acelerar build.
- No cambiar APIs públicas ni reescribir historial Alpha12.
- No asumir OTA, FlashFreq final ni bootloader definitivo.
- Un único BAT versionado para cada gate; con rollback
  cuando se adopte un archivo productivo.
- CI PASS de harness no equivale a hardware PASS.
- No se etiqueta release mientras F120 o REVIEW
  carezcan de disposición aceptada.

```text
G9_FINAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
G10_P0=PREPARED_NOT_RUN
G10_A13_006=REVIEW_OPEN
G10_A13_007=REVIEW_OPEN
G10_A13_008=REVIEW_OPEN
G10_A13_015=ADOPT_OPEN
G10_A13_016=ADOPT_OPEN
G10_A13_018=ADOPT_OPEN
G10_FINAL=NOT_CLOSED
ALPHA13=IN_PROGRESS
RELEASE_MERGE_PR=NO
```

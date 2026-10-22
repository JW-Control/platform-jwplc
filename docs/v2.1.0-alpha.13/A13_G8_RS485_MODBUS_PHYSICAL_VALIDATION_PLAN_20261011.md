## Conciliación con prueba realizada — G8-P4 PASS (2026-10-11)

Este documento nació como propuesta de banco. Posteriormente se
ejecutó una validación positiva real con **dos JWPLC Basic**:
Master COM4, Slave COM14, 115200 8N1, FC03 y 12/12
solicitudes/respuestas correctas. Las métricas observadas fueron
Master RX 156 bytes y Slave RX 96 bytes, CRC y timeouts cero.
Los firmwares de banco permanecen cargados.

La prueba cubrió **transmisión y recepción física**, inicialización
positiva de UART y contabilidad `rxBytes` no-bulk en ambas placas.
La negativa de fallo de instalación UART y `begin/end/begin` se
conservan como evidencias de fixtures nativos, sin atribuirles
ejecución física. No se hizo el ensayo opcional de reinicialización
UART ni restauración de aplicación anterior.

El resultado canónico está en
[A13_G8_P4_PHYSICAL_CLOSURE_20261011.md](A13_G8_P4_PHYSICAL_CLOSURE_20261011.md);
este plan histórico se conserva para trazabilidad.

```text
G8_PHYSICAL_FC03=CLOSED_PASS_12_OF_12
G8_PHYSICAL_RX_MASTER=156_EXPECTED_156
G8_PHYSICAL_RX_SLAVE=96_EXPECTED_96
G8_PHYSICAL_UART_BEGIN_POSITIVE=PASS
G8_PHYSICAL_UART_FAULT_INJECTION=NOT_PERFORMED
G8_FUNCTIONAL=CLOSED_PASS_WITH_EXPLICIT_COVERAGE_LIMITS
ALPHA13=IN_PROGRESS
```

---

# Alpha13-G8 — Plan seguro de validación física RS-485 y Modbus RTU

**Fecha:** 2026-10-11. **Estado:** `PLAN_ONLY_NO_UPLOAD_NO_HARDWARE_TEST`.
**Commit de producto:** `fb71d65eb2c842b3f46e6949760e693f3b73d95e`.
**G8 Git:** PASS. **G8 funcional:** pendiente de decisión/prueba física.

## Objetivo

Demostrar que las correcciones A13-010/A13-011 funcionan en la
placa real, sin limitar la prueba a mocks, compilación o enlace de
`libJWPLC_ModbusRTU.a`. Ninguna API, autoload o periférico debe
eliminarse por motivos de prueba o velocidad.

La placa JWPLC Basic usa RS-485 con transceptor de autodirección;
no se configurarán GPIO DE/RE manualmente ni se reconfigurará
`Serial2` fuera de `JWPLC_RS485`. La consola USB CH340 y el
transporte UART RS-485 no son el mismo puerto físico.

## Precondiciones obligatorias (no asumir que están disponibles)

1. Confirmar placa JWPLC Basic compatible, variante de package
   `jwplc_local:esp32:jwplcbasic`, fuente adecuada y permiso
   explícito para cargar firmware de diagnóstico. **No hacer upload
   sobre una máquina en servicio**.
2. Identificar un peer compatible: segundo JWPLC Basic, o
   adaptador USB↔RS-485 conectado a PC con herramienta Modbus
   configurada como Master/Slave. Para una prueba Master con
   un único JWPLC el programa en PC debe funcionar como Slave;
   para prueba Slave JWPLC el PC debe ser Master.
3. Verificar bornes y etiquetas RS-485 de ambas partes:
   en JWPLC Basic `P=A`, `N=B`; el fabricante externo
   puede invertir la denominación A/B, por lo que primero
   comprobar señales/documentación, no conectar por intuición.
   Usar un par trenzado y referencia común/aislamiento
   compatibles con los equipos, sin unir tierras de
   potencial distinto. No conectar RS-485 a bornes 24 V
   ni usar el bus de máquina activa.
4. Terminar/biasar el bus según cableado y documentación de
   los transceptores, evitando duplicar resistencias o crear
   cargas excesivas. No añadir 120 Ω/terminación por defecto
   sin revisar la topología.
5. Fijar para los ejemplos existentes **115200, 8N1**,
   Slave ID **2**, direcciones Holding Register **0..7**
   (base cero en API). Registrar baud/config del peer.

## Matriz mínima de banco

| Prueba | Montaje | Evidencia PASS requerida |
|---|---|---|
| RS-485 begin positivo | JWPLC + consola USB | `begin()==true`, `isReady()==true` y estado UART coherente; ningún reset o bloqueo |
| RS-485 TX/RX | JWPLC + peer compatible | Mensajes intercambiados; bytes observados de ambos lados; sin mezcla de CH340 y RS-485 |
| Modbus RTU Slave | JWPLC con ejemplo `01.ModbusRTU_Slave_Holding`, peer Master | Leer FC03 HR0..3 del ID2 sin CRC; HR0 evoluciona con tiempo, HR1 inicia 100 si no se escribió |
| Modbus RTU Master | JWPLC con ejemplo `02.ModbusRTU_Master_Read`, peer Slave ID2 | Al menos diez lecturas FC03 correctas; `READ OK` y valores compatibles |
| `rxBytes` Slave y Master | Instrumentación diagnóstica limitada, a preparar tras confirmar peer | Las cuentas progresan según bytes consumidos; un byte no se cuenta dos veces ni se pierde en ruta no-bulk |
| Re-inicialización UART | Solo si banco aislado y ensayo aprobado | `begin/end/begin` regresa a estado listo, sin afectar USB u otros periféricos |
| Falla simulada de driver UART | CI nativo ya ejecutado | No forzar una falla real invasiva del driver en placa sin plan separado; la prueba con mocks no se presentará como física |

Los dos ejemplos de RTU compilan y utilizan el archive productivo;
las compilaciones **no sustituyen** ejecución física. El sketch
`03.RS485_Status` envía `PING` cada dos segundos y permite
observar telemetría TX/RX.

## Procedimiento por etapas — pendiente de autorizar

- **P4-0:** identificar peer, cableado, COM de la consola CH340,
  fuente y si el JWPLC puede recibir un firmware de banco.
  Generar runner de preflight y backup de la aplicación
  existente, sin hacer upload todavía.
- **P4-1:** establecer sesión RS-485 TX/RX en 115200 8N1;
  registrar estado UART y eventos RX/TX sin tocar relés
  ni actuadores. Determinar link PASS o causa reproducible.
- **P4-2:** verificar RTU Slave FC03 con ID2 y dirección base cero.
- **P4-3:** verificar RTU Master FC03 contra Slave real/PC.
- **P4-4:** comprobar `stats.rxBytes` en ambas rutas no-bulk
  y revisar CRC/errors/timeout. Mantener rutas bulk
  cubiertas por pruebas C++ nativas; decidir aparte si
  se exige cobertura física bulk.
- **P4-5:** restaurar firmware de aplicación si se realizó
  upload, corroborar `READY`, registrar resultados,
  actualizar checklist G8 y FAILURES si surge incidencia.

## Stop conditions

Detener y registrar `REVIEW_STOP` ante wiring sin identificar,
ausencia de peer apropiado, COM ocupado, pérdida de comunicación
después del upload, reinicios, errores CRC sistemáticos, respuesta
de RS-485 no determinista o restauración de firmware no confirmada.
No deshabilitar protecciones, autoload, Ethernet, TFT, FRAM, RTC
ni funciones para forzar un PASS.

```text
G8_PRODUCT_GIT=PASS
G8_PHYSICAL_PLAN=PREPARED
G8_PHYSICAL_EVIDENCE=NONE
P4_HARDWARE_PEER=UNKNOWN
P4_UPLOAD_AUTHORIZATION=NOT_GIVEN
G8_FINAL_STATUS=IN_PROGRESS
NEXT_REQUIRED_INPUT=TYPE_OF_AVAILABLE_RS485_PEER_AND_TEST_BENCH_AUTHORIZATION
```

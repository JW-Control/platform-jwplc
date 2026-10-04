# Alpha14 — Hoja de ruta autónoma Codex para Ethernet

Fecha: 2026-09-29  
Rama obligatoria: `v2.1.0-alpha.14/feature/modbus-tcp`  
Alcance: investigación histórica Alpha14 sobre Ethernet/W5500 del JWPLC Basic v2.0.0.

> Esta rama es histórica. No mezclar ni portar automáticamente estos cambios a
> Alpha29/Alpha30, `release/v2.1.x` ni `main`.

---

## 1. Objetivo de esta sesión autónoma

Durante la ausencia temporal del usuario, continuar de forma controlada la
investigación para:

1. cerrar las pruebas pendientes de TCP RX;
2. identificar cuellos de botella reales del package canónico;
3. maximizar la velocidad Ethernet **en bruto** dentro del límite físico ya
   fijado de SPI a 26 MHz;
4. mantener **0 fallos SPI**, 0 corrupción de payload, 0 resets inesperados y
   0 errores de transporte;
5. terminar de mapear/afinar las APIs TCP cooperativas sin romper las APIs
   Arduino existentes;
6. dejar todos los resultados documentados para revisión humana posterior.

No sacrificar estabilidad por throughput.

---

## 2. Reglas duras

### 2.1 Package-first

CURRENT y todos los candidatos se construyen desde:

`JWPLC/2.1.0`

No reconstruir producto en `%TEMP%`.

Los temporales solo pueden usarse para:

- build directories;
- logs;
- comparación histórica;
- instrumentación desechable que no represente producto.

Todo candidato que deba ejecutarse como producto debe existir detrás de un
flag en el package canónico y estar OFF por defecto hasta su validación.

### 2.2 Una variable por gate

Nunca mezclar dos optimizaciones funcionales en el mismo A/B.

Cada gate debe declarar explícitamente:

`ONLY_VARIABLE=...`

Si una corrida descubre otro cuello, anotarlo y abrir el gate siguiente.

### 2.3 Estabilidad obligatoria

Rechazar inmediatamente un candidato si aparece cualquiera de:

- `TCP_SPI_LOCK_ERRORS > 0`;
- error SPI explícito;
- corrupción FNV/hash/payload;
- `TRANSPORT_ERRORS > 0`;
- reset inesperado;
- W5500 colgado;
- pérdida de link causada por el firmware;
- deadlock;
- timeout que no existía en baseline;
- inconsistencia de socket que impida recuperar comunicación.

Objetivo de estabilidad:

`SPI_FAILURE_RATE=0%`

No aceptar “casi cero”.

### 2.4 Frecuencia W5500 cerrada

`SPI_ETHERNET_SETTINGS = 26 MHz`

No volver a:

- 30 MHz;
- búsqueda de 27/28/29 MHz;
- overclock;
- cambiar FlashFreq para compensar Ethernet.

30 MHz ya falló físicamente en el historial y este frente está cerrado.

### 2.5 No retirar periféricos

No quitar del autoload normal:

- TFT;
- Ethernet;
- SD;
- FRAM;
- RTC;
- botonera;
- RS-485;
- Modbus RTU;
- TCA/I/O.

Un benchmark raw puede concentrar tráfico en Ethernet, pero no debe modificar
el package para excluir periféricos.

### 2.6 Compatibilidad

No romper:

- `EthernetClient::connect()`;
- `write()`;
- `read()`;
- `flush()`;
- `stop()`;
- UDP legacy `parsePacket()/read()`;
- APIs ya probadas.

Las optimizaciones nuevas deben ser:

- internas; o
- aditivas; o
- motores cooperativos que conserven la semántica Arduino legacy.

### 2.7 No publicar ni cerrar alpha

Durante esta sesión autónoma:

- no mergear a `release/v2.1.x`;
- no mergear a `main`;
- no tag;
- no release;
- no PreRelease;
- no restaurar precompilación final;
- no declarar Alpha14 cerrada.

Solo commits/push en:

`v2.1.0-alpha.14/feature/modbus-tcp`

---

## 3. Política de revisión física mientras el usuario está ausente

El usuario autoriza continuar las pruebas suponiendo que la TFT/periféricos se
mantienen operativos, pero la comprobación visual se hará al regresar.

Por tanto:

- **NO escribir `PHYSICAL_STABILITY=PASS` sin observación humana**;
- usar:
  `PHYSICAL_STABILITY=PENDING_USER`;
- un gate puede terminar:
  `PASS_DATA_ONLY`;
- la revisión física final queda pendiente.

No responder automáticamente `S` a prompts físicos.

Los gates nuevos deben aceptar una opción no interactiva equivalente a:

`--defer-physical-review`

y registrar el pendiente.

---

## 4. Estado confirmado antes de esta sesión

### 4.1 Package/source-first

Confirmado:

- `JWPLC_Display` source-first;
- `JWPLC_TFT` source-first;
- `SPI` source-first mientras está bajo desarrollo;
- `JWPLC_Ethernet` se compila desde source;
- no usar `.a` antiguos de Display/TFT/SPI mientras se modifiquen.

Al cierre del alpha habrá que regenerar también el `.a` de SPI si estos
cambios permanecen.

### 4.2 P1 — profiling TCP RX

Resultado relevante:

- BASE TCP RX mediana: ~13.175 Mbps;
- PROFILE: ~12.994 Mbps;
- intrusión profiler: -1.374%;
- `socketRecv()`: 90.90% del hold SPI;
- lectura SPI de payload: 93.52% de `socketRecv()`;
- `available()`: 6.09% del hold;
- `socketStatus()`: 2.16%;
- commit RX_RD + Sock_RECV: 3.86% de `socketRecv()`;
- payload efectivo: ~15.68 Mbps con SPI nominal 26 MHz.

Cuello principal confirmado:

`W5500_PAYLOAD_SPI_READ`

### 4.3 P2 — DIRECT_RX

Comparó:

`memset + SPI.transfer(buf,len)`

vs

`SPI.transferBytes(nullptr,buf,len)`

Resultado:

- mejora interna payload aproximada: +1.362%;
- throughput TCP end-to-end demasiado variable para atribuir +5.6%;
- `memset` NO es el cuello principal;
- el gran overhead sigue más abajo, en la ruta HAL SPI/chunks de 64 B.

`JWPLC_W5500_RX_DIRECT_TRANSFER_BYTES` permanece OFF por defecto.

### 4.4 A1 — TCP ASYNC lifecycle

Validado en ejecución:

- `beginConnectAsync()`;
- `pollConnectAsync()`;
- `connectAsyncInProgress()`;
- `cancelConnectAsync()`;
- `beginStopAsync()`;
- `pollStopAsync()`;
- `stopAsyncInProgress()`;
- `cancelStopAsync()`;
- `beginFlushAsync()`;
- cancel de flush en estado ya completado.

Resultados:

- connect begin máximo: 588 us;
- connect poll máximo: 22 us;
- connect total mediana: 1 ms;
- stop begin máximo: 60 us;
- stop poll máximo: 28 us;
- PC accepted=4;
- PC closed=4;
- resets TCP=0.

Pendiente:

- estado realmente pending de `flushAsync`;
- depende de tener TX pendiente;
- `write()` sigue síncrono.

No abrir TX antes de cerrar el frente RX salvo que ya no quede ningún gate RX
razonable o quede tiempo sobrante.

---

## 5. Punto exacto de arranque

Primero:

```powershell
git fetch origin
git checkout v2.1.0-alpha.14/feature/modbus-tcp
git pull --ff-only origin v2.1.0-alpha.14/feature/modbus-tcp
git status --short
```

Debe quedar limpio.

Ejecutar contrato:

```powershell
& "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe" -B `
tools\modbus-tcp-benchmark\gates\a14_package_promotion_contract.py
```

Si falla un contrato estático:

1. comprobar primero si el producto realmente está mal;
2. si el producto está correcto y el matcher quedó stale, clasificar:
   `HARNESS_FAILURE=YES`;
3. corregir únicamente el contrato;
4. rerun del mismo gate;
5. no avanzar hasta resolverlo.

---

# FASE 1 — Ejecutar y cerrar H4A0.4-P3

## 6. Gate P3 actual

Objetivo:

medir el costo de recargar el FIFO MOSI dummy cada 64 B.

Comparación:

`DIRECT_RX`

vs

`FIFO_REUSE`

`FIFO_REUSE`:

- sigue usando el mismo bus Arduino;
- mismo CS;
- mismo SPI a 26 MHz;
- mismo loop de 64 B;
- no precarga `data_buf[]` con dummy MOSI en cada chunk;
- lee MISO y copia el FIFO RX a RAM.

El flag permanece OFF por defecto:

`JWPLC_W5500_RX_FIFO_REUSE=0`

## 6.1 Ejecutar unattended

```powershell
& "C:\Users\jeykc\AppData\Local\Programs\Python\Python311\python.exe" -B `
tools\modbus-tcp-benchmark\gates\a14_h4a04p3_w5500_fifo_reuse_ab.py `
--defer-physical-review
```

## 6.2 Criterios ya fijados

Integridad FNV:

- obligatoriamente PASS.

Payload spread:

- máximo 0.5%.

Interpretación:

- >= +1.0% payload y mejora coherente us/byte:
  `FIFO_REUSE_GAIN_CONFIRMED`;
- dentro de +/-0.5%:
  `NO_MATERIAL_FIFO_REUSE_GAIN`;
- <= -1.0%:
  `FIFO_REUSE_REGRESSION`;
- resto:
  `FIFO_REUSE_SMALL_OR_INCONCLUSIVE_EFFECT`.

TCP end-to-end es secundario.

## 6.3 Decisión después de P3

### Si FNV falla o aparece cualquier fallo SPI

- detener la candidata;
- no promocionar;
- documentar;
- dejar default OFF;
- no intentar “arreglar el benchmark” para ocultar corrupción.

### Si FIFO_REUSE gana >=1% con 0 errores

No convertirlo todavía en default.

Crear confirmación P3R:

- 5 repeticiones o soak >= 10 min;
- FNV activo en una corrida separada;
- 0 SPI errors;
- 0 transport errors;
- 0 resets;
- revisión física: `PENDING_USER`.

Si P3R confirma:

`FIFO_REUSE_VALIDATED_PENDING_PHYSICAL=YES`

### Si no hay ganancia material

Mantener OFF y pasar directamente a Fase 2.

---

# FASE 2 — Descomponer el costo restante del chunk SPI de 64 B

## 7. Objetivo de P4

No saltar inmediatamente a DMA.

Primero medir, dentro del helper classic-ESP32, cuánto cuesta por chunk:

1. configuración de longitud;
2. arranque/espera `dev->cmd.usr`;
3. copia de `data_buf[]` a RAM;
4. overhead de loop/administración.

Instrumentación:

- compile-time only;
- OFF por defecto;
- nunca activa en build performance normal.

El profiling debe ejecutarse en una corrida corta y separada.

## 7.1 Métricas requeridas

Por chunk y total:

- `CHUNK_COUNT`;
- `WIRE_WAIT_TOTAL_US`;
- `COPY_OUT_TOTAL_US`;
- `SETUP_TOTAL_US`;
- `OTHER_TOTAL_US`;
- bytes;
- tamaño medio;
- us/byte.

Calcular también:

`IDEAL_WIRE_US = bytes * 8 / 26MHz`

y:

`EXCESS_OVER_WIRE_US`

## 7.2 Regla

No usar un profiler pesado para decidir throughput.

P4 sirve para localizar.

Después crear P5 con una sola mejora en el bloque dominante.

---

# FASE 3 — Candidatos P5 según P4

## 8. Si COPY_OUT domina

Probar una sola optimización por gate:

- copy loop explícitamente desenrollado; o
- método de copia más barato y seguro desde registros volatile.

No asumir que `memcpy` sobre registros volatile es correcto.

Verificar payload con FNV antes de throughput.

## 9. Si SETUP/START/WAIT overhead domina

Entonces sí investigar una ruta de transferencia grande/DMA.

Restricciones:

- no registrar un segundo dueño independiente del mismo bus;
- no inicializar de nuevo el bus SPI compartido por debajo de `SPIClass`;
- no romper `jwplcSPI_acquire()/release()`;
- no cambiar pines;
- no cambiar frecuencia;
- no dejar CS de otro periférico activo;
- no tocar TFT/SD/FRAM para “hacer sitio”.

Antes de implementar DMA:

1. documentar cómo coexistirá con el bus Arduino actual;
2. demostrar que no hay doble ownership;
3. mantener candidate OFF por default;
4. prueba FNV obligatoria;
5. si hay un solo SPI failure: REJECT.

Si una integración DMA segura no puede hacerse sin re-arquitecturar el bus,
NO improvisar. Documentar el bloqueo y pasar a Fase 4.

---

# FASE 4 — Optimizar overhead TCP por encima del payload

Estos puntos ya están medidos y deben atacarse después del payload.

Orden sugerido:

## 10. `available()/Sn_RX_RSR` — ~6.09% del hold

Investigar ruta interna/aditiva que fusione:

`available -> read`

sin hacer un RSR redundante.

No reemplazar transparentemente la API legacy.

Posible forma:

`jwplcReadTcpFast(...)`

solo si la semántica puede mantenerse clara.

## 11. Commit RX_RD + Sock_RECV — ~3.5% del hold total

Investigar commit diferido/coalescido para TCP, análogo conceptualmente al
camino UDP rápido.

Debe conservar:

- liveness;
- recuperación;
- compatibilidad legacy.

No diferir tanto que el W5500 deje de liberar RX y frene al emisor.

## 12. `socketStatus()/connected()` — ~2.16%

Eliminar lecturas realmente redundantes dentro del scheduler/benchmark o crear
un helper interno que no consulte dos veces el mismo estado en la misma pasada.

No cachear estado TCP indefinidamente.

## 13. Batch raw

Solo para identificar ceiling:

- comparar 8 / 16 / 32 chunks por ownership;
- no promocionar automáticamente el valor mayor;
- medir hold máximo y fairness.

La máxima velocidad raw no justifica bloquear periféricos de forma permanente.

## 14. Socket RX buffer

Como experimento de ceiling:

- 2 KB actual;
- 4 KB / 8 KB para un socket si la topología W5500 lo permite.

No cambiar la topología global del producto sin revisar:

- número de sockets;
- Modbus TCP;
- servicios concurrentes;
- compatibilidad.

---

# FASE 5 — Terminar TCP ASYNC/TX cuando RX quede mapeado

## 15. Objetivo

El lifecycle CONNECT/STOP ya está validado.

Lo pendiente natural es TX.

Diseñar de forma aditiva:

- `beginWriteAsync(...)`;
- `pollWriteAsync()`;
- `writeAsyncInProgress()`;
- `cancelWriteAsync()`.

No cambiar todavía la firma/semántica Arduino de:

`write()`

## 15.1 Requisitos

El motor debe dividir claramente:

1. esperar TX free space sin bloquear;
2. copiar payload al W5500;
3. emitir SEND;
4. esperar SEND_OK/TIMEOUT por poll.

Cada poll debe ser corto.

## 15.2 Gate A2

Probar:

- begin/poll write;
- estado pending real;
- cancel;
- timeout;
- peer close;
- reconnect;
- `flushAsync` con TX realmente pendiente.

Solo entonces cerrar:

`FLUSH_PENDING_PATH=PASS`

No modificar RX durante A2.

---

# FASE 6 — Soak de estabilidad del mejor candidato

## 16. Acceptance de 0% SPI failures

Antes de recomendar cualquier default:

- >= 10 min de tráfico continuo;
- payload integrity;
- 0 lock errors;
- 0 transport errors;
- 0 resets;
- recovery después de cerrar/reabrir conexión;
- link sigue operativo;
- package sigue compilando source-first.

Mientras el usuario esté fuera:

`PHYSICAL_STABILITY=PENDING_USER`

No declarar cierre final.

Si hay tiempo, ejecutar además un soak compartido con actividad real de otros
periféricos SPI usando tooling existente del proyecto, sin retirarlos del
autoload.

---

# 17. Política de decisión autónoma

Codex puede:

- añadir instrumentación OFF por default;
- crear gates;
- crear runners;
- corregir harness bugs demostrados;
- crear candidatos detrás de flags OFF;
- commit/push a la rama histórica;
- repetir gates;
- documentar.

Codex NO puede de forma autónoma:

- mergear;
- publicar release;
- cambiar SPI >26 MHz;
- eliminar periféricos;
- cambiar APIs legacy;
- poner un candidato como default si falta revisión física del usuario;
- declarar alpha cerrada;
- inventar un PASS físico.

---

# 18. Clasificación obligatoria de fallos

## Harness

Ejemplos:

- parser;
- matcher textual stale;
- timeout incorrecto del runner;
- ruta errónea;
- prompt interactivo en modo unattended.

Resultado:

`HARNESS_FAILURE=YES`

Corregir harness y repetir el mismo gate.

## Product

Ejemplos:

- corrupción;
- error SPI;
- deadlock;
- reset;
- API async no cumple;
- pérdida reproducible de comunicación.

Resultado:

`PRODUCT_FAILURE=YES`

Detener esa candidata y analizar antes de avanzar.

## Hardware/environment

Ejemplos:

- COM14 desaparece;
- cable desconectado;
- firewall impide servidor;
- link físico cae sin relación causal con firmware.

No reinterpretar como product bug.

Reintentar como máximo dos veces si el motivo ambiental es evidente. Si no,
detener y documentar.

---

# 19. Commits

Un cambio conceptual por commit.

Mensajes en español, por ejemplo:

- `test(alpha14): perfilar overhead por chunk SPI W5500`
- `test(alpha14): añadir candidato RX ...`
- `fix(alpha14): corregir contrato del gate ...`
- `docs(alpha14): registrar resultados P4`

Push únicamente a:

`v2.1.0-alpha.14/feature/modbus-tcp`

No rebase destructivo.

---

# 20. Reporte que debe quedar al terminar la sesión

Crear o actualizar:

`docs/v2.1.0-alpha.14/A14_CODEX_AUTONOMOUS_ETHERNET_REPORT_20260929.md`

Debe incluir una tabla:

| Gate | Baseline | Candidate | Payload Mbps | TCP Mbps | Delta | SPI errors | FNV | Resets | Physical | Decisión |
|---|---|---|---:|---:|---:|---:|---|---:|---|---|

Además:

1. HEAD final;
2. commits creados;
3. gates ejecutados;
4. logs/rutas de evidencia;
5. candidatos rechazados;
6. candidatos validados;
7. candidatos aún OFF;
8. puntos de optimización restantes;
9. estimación del ceiling medida, no inventada;
10. lista exacta de verificaciones físicas pendientes para el usuario.

---

# 21. Prioridad si solo quedan ~3 horas

Orden:

1. P3 FIFO_REUSE;
2. P3R si gana;
3. P4 microprofile 64 B;
4. P5 una sola optimización sobre el bloque dominante;
5. profiling/optimización TCP `available/read` si P5 ya cerró;
6. A2 TX async solo si RX quedó suficientemente cerrado;
7. informe final.

Si el tiempo/uso se agota en medio de un gate:

- no iniciar otro;
- commit de tooling si compila;
- dejar reporte con `CURRENT_STATE`;
- indicar el comando exacto para continuar.

---

# 22. Prompt corto para iniciar Codex

Leer este archivo completo y continuar desde CURRENT STATE siguiendo un gate a
la vez. Priorizar estabilidad y evidencia. Usar únicamente el package canónico
`JWPLC/2.1.0`. Mantener SPI W5500 en 26 MHz. No retirar periféricos. Exigir
0 fallos SPI, 0 corrupción y 0 resets. Durante la ausencia del usuario registrar
la revisión física como `PENDING_USER`, nunca como PASS. Corregir fallos de
harness y repetir el mismo gate; detener candidatos con fallo de producto.
Commit/push solo a `v2.1.0-alpha.14/feature/modbus-tcp`. No mergear ni cerrar
el alpha. Al terminar, actualizar
`A14_CODEX_AUTONOMOUS_ETHERNET_REPORT_20260929.md` con toda la evidencia.

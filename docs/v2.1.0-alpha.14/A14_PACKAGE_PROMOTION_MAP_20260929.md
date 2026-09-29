# Alpha14 — Mapa de promoción de cambios temporales al package

Fecha: 2026-09-29  
Rama: `v2.1.0-alpha.14/feature/modbus-tcp`

## Objetivo

Evitar que una mejora validada en un worktree/copia temporal quede fuera de
`JWPLC/2.1.0` y sea confundida posteriormente con comportamiento real del
package.

Desde este punto se adopta la regla:

- **CURRENT se valida desde el package canónico.**
- Un cambio de producto aprobado se integra primero en `JWPLC/2.1.0`.
- Los worktrees temporales se reservan para:
  - reproducción histórica;
  - experimentos aún no aprobados;
  - instrumentación que no debe existir en producción.
- Al cerrar el alpha se restaurará la precompilación solamente después de:
  - regenerar los `.a`;
  - comprobar hashes/source;
  - comparar tiempos de compilación;
  - repetir smoke/qualification de Arduino IDE.

---

## 1. Política de precompilación durante desarrollo

### JWPLC_Display

Estado anterior:

```txt
dot_a_linkage=true
precompiled=full
```

Estado de desarrollo:

```txt
dot_a_linkage=(ausente)
precompiled=(ausente)
```

Resultado:

- el source `.cpp` vuelve a ser la fuente autoritativa durante las pruebas;
- cambios visuales/API no pueden quedar ocultos detrás de un `.a` antiguo;
- los archives históricos pueden permanecer físicamente en `src/esp32`,
  pero no se habilita su uso mientras `precompiled` esté ausente;
- el smoke de compilación debe comprobar además que los nombres
  `libJWPLC_Display.a` / `libJWPLC_TFT.a` no aparezcan en el build;
- la precompilación se restaurará únicamente al cerrar el alpha.

### JWPLC_TFT

Misma política que `JWPLC_Display`.

Además se actualizó la descripción para no afirmar que el backend está
precompilado mientras el alpha siga abierto.

---

## 2. Marca física de JWPLC_TFT/TFT_eSPI

Se cambió el título del IDLE para que use un bloque de fondo explícito y un
color de texto independiente.

### Antes

- texto azul/celeste;
- fondo negro;
- sin bloque de fondo.

### Ahora

- rectángulo de fondo configurable;
- color de texto configurable;
- título `JWPLC Basic`.

Los valores exactos de posición, dimensiones y color quedan deliberadamente
fuera del contrato de validación para poder ajustarlos visualmente sin romper
los gates de Ethernet.

Objetivo:

- confirmar visualmente en hardware que el firmware está compilando el source
  actual de `JWPLC_Display`;
- evitar confundir un binario precompilado antiguo con la variante
  `JWPLC_TFT -> TFT_eSPI`.

No cambia la API pública.

---

## 3. Estado de las mejoras Ethernet investigadas

### 3.1 Lectura estable de Sn_RX_RSR

Cambio:

`W5100.readSnRX_RSRStable()`

Estado:

**YA ESTABA INTEGRADO EN PRODUCTO.**

Mejora:

- evita usar una lectura inestable del contador RX del W5500;
- da una base más segura para disponibilidad TCP/UDP.

Riesgo/falla observada:

- ninguna regresión conocida en la qualification actual.

---

### 3.2 Comando W5500 con timeout

Cambio:

`W5100.execCmdSnChecked()`

Estado:

**YA ESTABA INTEGRADO EN PRODUCTO.**

Mejora:

- evita waits infinitos al ejecutar comandos de socket;
- permite detectar timeout/error en lugar de bloquear indefinidamente.

Riesgo/falla observada:

- ninguna regresión conocida en la qualification actual.

---

### 3.3 P3B — BATCH2

Cambio experimental:

procesar hasta dos datagramas por ownership del SPI.

Estado:

**NO SE COPIA COMO POLÍTICA GLOBAL DEL PACKAGE.**

Motivo:

- pertenece al scheduler/consumer cooperativo;
- el número óptimo depende de fairness y del resto de periféricos;
- el backend de producto no debe decidir por sí solo cuántos paquetes consume
  una aplicación por tick.

Mejora observada:

- confirmó que amortizar lock/administración SPI era relevante;
- el boundary 1016 B permite dos registros W5500 exactos de 1024 B dentro del
  RX socket de 2 KB.

Riesgo:

- un batch demasiado largo puede aumentar retención del SPI y perjudicar
  display/SD/FRAM/RTC.

Decisión:

`BATCH2` queda como estrategia validada del consumidor cooperativo, no como
cambio transparente de la API Arduino.

---

### 3.4 P3G — guía por INTn GPIO15

Cambio experimental:

- uso de INTn del W5500 en GPIO15;
- `SnIMR`, `SIR`, `SIMR`;
- ISR mínima que solo marca pending;
- trabajo SPI fuera de ISR.

Estado:

**SOPORTE DE REGISTROS PROMOVIDO AL PACKAGE.**

Se integraron:

- `SIR_W5500`;
- `SIMR_W5500`;
- `SnIMR`.

La ISR/política concreta de scheduling queda fuera del backend común por ahora.

Mejora observada:

- redujo los service holds vacíos a prácticamente cero;
- no mostró pérdida material de throughput.

Riesgo:

- limpiar/rearmar `RECV` en el momento incorrecto puede perder liveness.

---

### 3.5 P3H — fused UDP receive

Cambio experimental:

leer en una ruta integrada:

1. pseudo-header W5500 de 8 B;
2. payload;
3. actualizar metadatos del datagrama.

Estado:

**PROMOVIDO COMO BASE DEL CAMINO INTERNO DE ALTO RENDIMIENTO.**

En producto queda como:

`EthernetUDP::jwplcReadPacketFastDeferred()`

y backend privado:

`EthernetClass::socketRecvUDPFastDeferred()`.

Mejora observada:

- ~3–4 % relativa frente al camino legacy en pruebas pareadas P3.

Compatibilidad:

- `parsePacket()` y `read()` no se sustituyen;
- la API Arduino legacy sigue funcionando igual.

---

### 3.6 P3I — single-CS burst

Cambio experimental:

reducir toggles CS/headers SPI.

Estado:

**RECHAZADO / NO PRODUCTIZADO.**

Resultado:

- redujo aproximadamente 17 % las lecturas SPI por paquete;
- no produjo ganancia de throughput repetible.

Decisión:

```txt
P3I_SINGLE_CS_PRODUCTIZE=NO
```

---

### 3.7 P3J — commit coalescido

Cambio:

diferir:

- `Sn_RX_RD`;
- comando `Sock_RECV`;

y hacer un solo commit después del batch elegido por el consumidor.

Estado:

**PRIMITIVA PROMOVIDA AL PACKAGE.**

En producto:

- `EthernetUDP::jwplcCommitRxFast()`;
- `EthernetClass::socketCommitUDPFast()`.

Mejora observada en P3J-R2:

- +8.39 % relativa contra FUSED en comparación pareada.

Nota metodológica:

los Mbps absolutos históricos estaban inflados por el tail-accounting;
la mejora relativa del A/B pareado sigue siendo evidencia útil, pero no se
debe reutilizar el antiguo ~13.87 Mbps como ceiling sostenido.

Compatibilidad:

- la API UDP legacy no cambia;
- el commit diferido solo ocurre si un consumidor llama explícitamente a la
  extensión JWPLC.

---

### 3.8 P3J-R1 — orden correcto de clear/rearm

Problema encontrado:

el primer commit coalescido podía perder liveness del INT al limpiar
`Sn_IR(RECV)` antes de liberar espacio RX.

Secuencia validada:

```txt
drain
-> commit RX_RD + Sock_RECV
-> clear Sn_IR(RECV)
-> one-shot Sn_RX_RSR
-> rearm pending si RSR > 0 o INT sigue LOW
```

Estado:

**ALGORITMO VALIDADO Y DOCUMENTADO; NO SE FUERZA GLOBALMENTE.**

Motivo:

el rearm depende del consumidor que habilita INTn y conoce el socket concreto.
El package ya contiene las primitivas necesarias para construirlo sin volver a
parchear `socket.cpp`.

Falla que corrigió:

- pérdida de liveness/interrupciones bajo socket lleno o evento solapado.

---

### 3.9 P3J-R2 — barrera serial IDLE / quiescence

Estado:

**BENCHMARK-ONLY.**

Mejora:

- separa corridas;
- permite verificar quiescencia;
- evita contaminar la siguiente medición con paquetes residuales.

No pertenece al producto.

---

### 3.10 H4A0.3A — instrumentación mínima

Estado:

**BENCHMARK-ONLY, PERO DEFINE UNA REGLA DE PRODUCTO.**

Se eliminó de la ruta caliente de performance:

- contadores globales por lectura W5100;
- timers `micros()` por packet/hold;
- profiling de loop;
- contadores detallados de ISR.

Hallazgo:

la instrumentación puede modificar el mismo rendimiento que intenta medir.

Regla:

los builds PERFORMANCE deben conservar solo contadores esenciales. El profiling
detallado debe estar detrás de hooks/flags apagados por defecto.

---

### 3.11 R1 — tail accounting

Estado:

**CORRECCIÓN DE METODOLOGÍA, NO DE PRODUCTO.**

Hallazgo:

el benchmark histórico sumaba bytes recibidos durante un tail posterior al envío
pero dividía solo entre el tiempo de envío.

Efecto confirmado:

- UDP con tail 400 ms sobre ventana 5 s: ~+8 % aparente;
- TCP con tail 500 ms sobre ventana 5 s: ~+10 % aparente.

Decisión:

no volver a usar snapshot posterior como numerator con denominator truncado.

---

### 3.12 R3 — freeze temporal

Estado:

**BENCHMARK-ONLY.**

Mejora:

- detiene accounting productivo antes del snapshot;
- permite calcular bounds request->ACK;
- evita atribuir tiempo de Serial/snapshot al throughput.

Resultado matched con TFT actual:

- HIST: 12.438351 Mbps;
- CURRENT: 12.456846 Mbps;
- diferencia: -0.148 %;
- sin residual material histórico/actual.

---

## 4. Estado de la API UDP después de la promoción

Se conserva:

```cpp
parsePacket();
read(...);
```

Se añade como extensión JWPLC:

```cpp
jwplcReadPacketFastDeferred(...);
jwplcCommitRxFast();
```

La nueva ruta:

- es aditiva;
- no se activa transparentemente;
- permite construir consumers cooperativos optimizados;
- evita volver a modificar copias temporales de la librería Ethernet.

---

## 5. Regla para profiling TCP a partir de ahora

El profiling TCP se implementará **package-first**:

- hooks en `JWPLC_Ethernet`;
- desactivados por defecto;
- sin overhead en builds normales;
- activados solo por un flag de build/gate;
- BASE y PROFILE usarán el mismo package canónico.

No se aceptará una optimización TCP como cerrada hasta que:

0. el smoke `a14_package_promotion_compile_smoke.py` confirme source-first;
1. esté integrada en `JWPLC/2.1.0`;
2. compile en Arduino CLI/IDE;
3. pase benchmark BASE;
4. pase runtime/periféricos;
5. quede documentada.

---

## 6. Checklist para cierre del alpha

Antes de decir **"procedemos con cerrar el alpha"**:

- [ ] congelar cambios funcionales;
- [ ] ejecutar qualification final;
- [ ] medir tiempos de compilación source-first;
- [ ] regenerar `libJWPLC_Display.a`;
- [ ] regenerar `libJWPLC_TFT.a`;
- [ ] restaurar `dot_a_linkage=true` donde corresponda;
- [ ] restaurar `precompiled=full`;
- [ ] verificar que el build realmente selecciona los nuevos `.a`;
- [ ] comparar tiempos source vs precompiled;
- [ ] repetir smoke de display/Ethernet/periféricos;
- [ ] actualizar documentación/release notes.

---

## Conclusión

El riesgo identificado era real: el workflow histórico permitía que una mejora
validada quedara solo en una copia temporal.

La política actual corrige ese problema:

```txt
VALIDADO COMO PRODUCTO
-> PROMOVER A JWPLC/2.1.0
-> COMMIT
-> RE-TEST

SOLO DIAGNÓSTICO
-> tools/
-> nunca asumirlo como capacidad del package
```

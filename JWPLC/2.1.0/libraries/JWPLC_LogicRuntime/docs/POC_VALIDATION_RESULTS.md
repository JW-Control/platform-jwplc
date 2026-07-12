# Resultados de validación — JWPLC Logic Runtime

**Rama:** `feature/logic-runtime-poc`  
**Hardware:** JWPLC Basic  
**Estado:** PoC 0, 1, 2, 2.1, 2.2, 3 y 4 validados; PoC 5 preparado para prueba física reversible.

## PoC 0 — estructura inicial

Compilación informada:

```text
Flash: 416761 bytes / 3145728 bytes (13 %)
RAM global: 27932 bytes / 327680 bytes (8 %)
```

Resultado:

- Librería detectada correctamente por Arduino IDE.
- Perfil automático de FRAM de 8192 bytes.
- Límite inicial de 100 bloques.
- Ciclo de vida básico compilado.

## PoC 1 — motor lógico y E/S físicas

Programa probado:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Resultado informado:

- Activación retardada correcta.
- Reinicio del TON al perder la condición.
- Apagado inmediato de `Q0_0`.
- Salidas no utilizadas apagadas.
- Validación física: PASS.

## PoC 2 — autopruebas del validador

Compilación informada:

```text
Flash: 418001 bytes / 3145728 bytes (13 %)
RAM global: 27916 bytes / 327680 bytes (8 %)
```

Resultado:

```text
10 PASS, 0 FAIL
VALIDACION COMPLETA: PASS
```

Casos cubiertos:

- Programa válido.
- Puntero nulo.
- Programa vacío.
- Exceso de bloques.
- Tipo inválido.
- Fuente ausente.
- Fuente fuera de rango.
- Referencia no anterior.
- Recurso fuera de rango.
- Salida duplicada.

## PoC 2.0 — E/S individuales

Medición aproximada con ocho lecturas y ocho escrituras individuales por scan:

```text
Mínimo:   7231 us
Promedio: 7680 us
Máximo:  43217 us
```

Conclusión: el costo dominante no era el motor de seis bloques, sino el acceso repetido al expansor TCA6424A.

## PoC 2.1 — acceso por snapshot y banco

Compilación informada:

```text
Flash: 419589 bytes / 3145728 bytes (13 %)
RAM global: 31188 bytes / 327680 bytes (9 %)
```

Después del calentamiento de tres segundos:

```text
Mínimo:    380 us
Promedio:  414 us aproximadamente
Máximo:   3889 us
```

Comparación del promedio:

```text
7680 us -> 414 us
Reducción aproximada: 94.6 %
Mejora aproximada: 18.6 veces
```

La lógica física continuó funcionando correctamente durante cambios de `I0_1` y reinicios del TON.

## PoC 2.2 — escritura Q0 solo cuando cambia

Validación prolongada informada después de más de 3.5 millones de scans:

```text
Mínimo:      5 us
Promedio:    5 us
Máximo estable sin cambio Q0: 24 us
Máximo al cambiar Q0: 419–425 us
```

Contador de escrituras observado:

```text
Q0 estable encendida: escrituras Q0 = 3
Transición a apagada: escrituras Q0 = 4
Transición a encendida: escrituras Q0 = 5
```

Resultado:

- El bitmap estable no genera nuevas transacciones físicas.
- Cada transición real agrega una escritura de banco.
- La lógica `TON` conserva el comportamiento esperado.
- El scan interno normal cae de aproximadamente 414 us a 5 us.
- Una transición de salida conserva el costo de la operación I2C, próximo a 0.4 ms.

Interpretación importante:

- Los `5 us` corresponden al trabajo interno medido por `runtime.tick()`.
- El ejemplo mantiene `delay(1)`, por lo que el periodo total del loop sigue próximo a 1 ms.
- Las entradas consumen el snapshot lógico que el core actualiza por defecto cada 20 ms.
- El runtime no se declara hard real-time.

## PoC 3 — codec binario

Formato:

```text
Cabecera: 64 bytes
Bloque:   12 bytes
Total:    64 + 12 × N
```

Tamaños relevantes:

| Bloques | Imagen |
|---:|---:|
| 6 | 136 B |
| 100 | 1264 B |
| 400 | 4864 B |

Resultado ejecutado:

```text
14 PASS, 0 FAIL
CODEC BINARIO: PASS
```

Casos cubiertos:

- Serialización.
- Deserialización.
- Conservación de nombre, bloques y metadatos.
- Validación del programa reconstruido.
- Corrupción de payload.
- Corrupción de cabecera.
- Imagen truncada.
- Buffer insuficiente.
- Nombre demasiado largo.
- Capacidad de 100 bloques dentro del slot inicial.

Resultado de capacidad:

```text
Imagen de 6 bloques: 136 bytes
Imagen máxima inicial de 100 bloques: 1264 / 2560 bytes por slot
```

## PoC 4 — almacenamiento A/B simulado

Backend RAM de 8 KiB, sin E/S y sin acceso a la FRAM física.

Resultado ejecutado:

```text
19 PASS, 0 FAIL
ALMACENAMIENTO A/B SIMULADO: PASS
```

Medición de la transacción:

```text
Bytes escritos por actualización completa: 196
Puntos de corte probados: 196
```

Casos cubiertos:

- Detección de almacenamiento sin formato.
- Formateo inicial.
- Guardado y carga de Programa A.
- Guardado y activación de Programa B.
- Reinicio lógico después de cada operación.
- Fallback a Programa A cuando la imagen B está corrupta.
- Uso de la copia anterior cuando el superblock reciente está corrupto.
- Corte simulado después de cada byte de la actualización.
- Conservación de Programa A en todos los cortes parciales.
- Activación de Programa B solo con presupuesto de escritura completo.

Conclusión:

- El algoritmo no modifica el slot activo durante una actualización.
- El nuevo slot se activa únicamente después de escribirlo y verificarlo.
- Los dos superblocks permiten conservar una referencia válida ante una escritura incompleta.
- El comportamiento transaccional queda aprobado en RAM antes de tocar hardware persistente.

## PoC 5 — backend sobre FRAM física

Estado:

```text
Preparado; pendiente de compilación y ejecución.
```

Características de seguridad del ejemplo:

- No inicializa E/S ni conmuta relés.
- Requiere escribir `ERASE` por Serial antes de modificar la FRAM.
- Usa una ventana reducida de 1088 bytes al final de la FRAM.
- Dirección física prevista: `0x1BC0..0x1FFF`.
- Respalda la ventana completa en RAM antes de escribir.
- Ejecuta un flujo A/B reducido con slots de 512 bytes.
- Restaura y verifica byte por byte el contenido original al finalizar.
- No debe reiniciarse ni perder alimentación durante la prueba porque el respaldo temporal reside en RAM.

Objetivos:

- Confirmar que `LogicFRAMStorage` opera sobre la instancia global `JWPLC_FRAM`.
- Confirmar lectura y escritura dentro de una ventana limitada.
- Confirmar convivencia con el bloqueo del bus SPI del package.
- Guardar, reiniciar lógicamente y cargar Programa A.
- Guardar, reiniciar lógicamente y cargar Programa B.
- Restaurar exactamente la región previa.

Ejemplo:

```text
JWPLC_LogicRuntime_FRAM_Storage
```

## Próximos criterios de cierre

- PoC 5 compila con `JWPLC Basic`.
- La FRAM automática reporta 8192 bytes.
- El backend acepta la ventana `0x1BC0..0x1FFF`.
- Programa A y B sobreviven reinicios lógicos del gestor.
- Ambos programas reconstruidos superan el validador.
- La región original se restaura exactamente.
- Después se preparará la prueba de layout completo y reinicio físico controlado.

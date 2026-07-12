# Resultados de validación — JWPLC Logic Runtime

**Rama:** `feature/logic-runtime-poc`  
**Hardware:** JWPLC Basic  
**Estado:** PoC 0, 1, 2, 2.1, 2.2 y 3 validados; PoC 4 preparado para prueba.

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

Preparado en RAM, sin tocar la FRAM física.

Objetivos:

- Backend direccionable por bytes independiente del medio.
- Dos superblocks redundantes.
- Dos slots de programa.
- Descriptor `WRITING` y `VERIFIED` por slot.
- Escritura exclusiva sobre el slot inactivo.
- Verificación de imagen antes de activar.
- Fallback al programa anterior si la imagen activa está corrupta.
- Recuperación si el superblock más reciente está corrupto.
- Inyección de corte en cada byte posible de una actualización.

Ejemplo:

```text
JWPLC_LogicRuntime_AB_Storage
```

Este ejemplo usa un buffer RAM de 8 KiB y no inicializa E/S ni escribe la FRAM real.

## Próximos criterios de cierre

- PoC 4 compila.
- Todos los puntos de corte parciales conservan el programa A.
- Una escritura completa activa el programa B.
- La corrupción de B provoca fallback a A.
- La corrupción del superblock más reciente conserva la copia anterior.
- Después se implementará el backend real para `JW_FRAM`.

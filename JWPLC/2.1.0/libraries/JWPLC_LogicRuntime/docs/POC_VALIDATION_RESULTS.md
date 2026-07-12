# Resultados de validación — JWPLC Logic Runtime

**Rama:** `feature/logic-runtime-poc`  
**Hardware:** JWPLC Basic  
**Estado:** PoC 0, PoC 1 y PoC 2.1 validados; PoC 2.2 y PoC 3 pendientes de compilación/prueba.

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

## PoC 2.1 — acceso por bitmap/banco

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

### Interpretación

- El motor lógico de seis bloques consume una fracción pequeña del tiempo total.
- La escritura de banco Q0 domina el scan de unos 380–400 us.
- Los picos de milisegundos siguen siendo posibles por tareas concurrentes del core, display, FreeRTOS e interrupciones.
- Este resultado no convierte al runtime en hard real-time.
- Las entradas se obtienen del snapshot del sistema, actualizado por defecto cada 20 ms.

## PoC 2.2 — escritura Q0 solo por cambio

Cambio preparado:

- Conservar el último bitmap físico confirmado.
- Omitir la transacción I2C cuando Q0 no cambia.
- Reintentar si el shadow del core no confirma la escritura.
- Contabilizar escrituras físicas con `outputWriteCount()`.

Resultado esperado:

- Scan normal muy inferior a 380 us cuando Q0 permanece estable.
- Una escritura física al iniciar y otra por cada transición real de Q0.
- Mismo comportamiento lógico y seguro.

Estado:

```text
Pendiente de compilación y validación física.
```

## PoC 3 — codec binario

Formato preparado:

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

Pruebas automáticas preparadas:

- Serialización.
- Deserialización.
- Conservación de bloques y metadatos.
- Validación posterior.
- CRC de cabecera.
- CRC de payload.
- Imagen truncada.
- Buffer insuficiente.
- Nombre demasiado largo.

Estado:

```text
Pendiente de compilación y ejecución en hardware.
```

## Próximos criterios de cierre

- PoC 2.2 mantiene la lógica física correcta.
- El contador de escrituras Q0 solo aumenta ante cambios reales.
- PoC 3 termina con cero fallos.
- La imagen de 100 bloques cabe dentro del slot inicial de 2560 bytes.
- No se escribe todavía en FRAM.

# Resultados de validación — JWPLC Logic Runtime

**Rama:** `feature/logic-runtime-poc`  
**Hardware:** JWPLC Basic  
**Estado:** PoC 0 a PoC 5 validados; PoC 6 preparada para prueba entre reinicios reales.

## Resumen

| PoC | Objetivo | Resultado |
|---:|---|---|
| 0 | Estructura inicial de librería | PASS |
| 1 | Motor lógico fijo y E/S físicas | PASS |
| 2 | Validador de programas | 10 PASS, 0 FAIL |
| 2.1 | Acceso a E/S por snapshot y banco | PASS |
| 2.2 | Escritura Q0 solo ante cambios | PASS |
| 3 | Codec binario versionado | 14 PASS, 0 FAIL |
| 4 | Almacenamiento A/B simulado en RAM | 19 PASS, 0 FAIL |
| 5 | Backend A/B sobre FRAM física | 22 PASS, 0 FAIL |
| 6 | Persistencia entre reinicios reales | Pendiente de ejecución |

## PoC 0 — estructura inicial

```text
Flash: 416761 bytes / 3145728 bytes (13 %)
RAM global: 27932 bytes / 327680 bytes (8 %)
```

Validado:

- detección de la librería por Arduino IDE;
- perfil automático de FRAM de 8192 bytes;
- límite inicial de 100 bloques;
- ciclo de vida básico del runtime.

## PoC 1 — motor lógico y E/S físicas

Programa probado:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Validado:

- activación retardada;
- reinicio del TON al perder la condición;
- apagado inmediato de `Q0_0`;
- salidas no utilizadas apagadas.

## PoC 2 — validador

```text
10 PASS, 0 FAIL
VALIDACION COMPLETA: PASS
```

```text
Flash: 418001 bytes / 3145728 bytes (13 %)
RAM global: 27916 bytes / 327680 bytes (8 %)
```

Casos cubiertos:

- programa válido;
- puntero nulo;
- programa vacío;
- exceso de bloques;
- tipo inválido;
- fuente ausente;
- fuente fuera de rango;
- referencia no anterior;
- recurso fuera de rango;
- salida duplicada.

## PoC 2.1 — acceso por snapshot y banco

Primera implementación, con ocho lecturas y ocho escrituras individuales:

```text
Mínimo:    7231 us
Promedio:  7680 us
Máximo:   43217 us
```

Implementación por snapshot de entradas y escritura conjunta del banco Q0:

```text
Mínimo:     380 us
Promedio:   414 us aproximadamente
Máximo:    3889 us
```

Resultado aproximado:

```text
Reducción del promedio: 94.6 %
Mejora: 18.6 veces
```

## PoC 2.2 — escritura Q0 solo cuando cambia

Validación prolongada después de más de 3.5 millones de scans:

```text
Mínimo:      5 us
Promedio:    5 us
Máximo estable sin cambio Q0: 24 us
Máximo al cambiar Q0: 419–425 us
```

Contador observado:

```text
Q0 estable encendida: escrituras Q0 = 3
Transición a apagada: escrituras Q0 = 4
Transición a encendida: escrituras Q0 = 5
```

Conclusión:

- un bitmap estable no genera transacciones físicas redundantes;
- cada transición real genera una escritura de banco;
- el costo normal del motor de seis bloques es de unos 5 us;
- una transición física conserva el costo I2C de aproximadamente 0.4 ms.

Los `5 us` corresponden a `runtime.tick()`. El ejemplo mantiene `delay(1)` y las entradas consumen el snapshot lógico actualizado por defecto cada 20 ms. El runtime no se declara hard real-time.

## PoC 3 — codec binario

Formato:

```text
Cabecera: 64 bytes
Bloque:   12 bytes
Total:    64 + 12 × N
```

| Bloques | Imagen |
|---:|---:|
| 6 | 136 B |
| 100 | 1264 B |
| 400 | 4864 B |

Resultado:

```text
14 PASS, 0 FAIL
CODEC BINARIO: PASS
```

El perfil inicial dispone de 2560 bytes por slot; una imagen de 100 bloques ocupa 1264 bytes.

## PoC 4 — almacenamiento A/B simulado

Resultado:

```text
19 PASS, 0 FAIL
ALMACENAMIENTO A/B SIMULADO: PASS
Bytes escritos por actualización completa: 196
Puntos de corte probados: 196
```

Validado:

- dos superblocks redundantes;
- slots A y B;
- estados `WRITING` y `VERIFIED`;
- escritura sobre el slot inactivo;
- verificación antes de activar;
- fallback al programa anterior;
- recuperación ante superblock corrupto;
- conservación de Programa A en todos los cortes parciales;
- activación de Programa B únicamente con transacción completa.

## PoC 5 — backend FRAM físico

Ventana utilizada:

```text
FRAM detectada:   8192 bytes
Direcciones:      0x1BC0..0x1FFF
Tamaño:           1088 bytes
```

Resultado:

```text
22 PASS, 0 FAIL
BACKEND FRAM FISICA: PASS
La ventana de prueba fue restaurada.
```

Validado:

- backend `LogicFRAMStorage`;
- layout A/B reducido;
- guardado y carga de Programa A;
- guardado y carga de Programa B;
- metadatos, CRC y validador;
- reinicios lógicos del gestor;
- restauración exacta de la ventana original.

Documento detallado:

- `docs/POC5_FRAM_PHYSICAL_RESULTS.md`

## PoC 6 — persistencia entre reinicios reales

Preparada en:

```text
examples/JWPLC_LogicRuntime_FRAM_Persistent/
```

Flujo:

1. respaldar la ventana FRAM en NVS;
2. guardar Programa A;
3. reiniciar físicamente;
4. cargar A y guardar B;
5. reiniciar físicamente otra vez;
6. cargar B;
7. restaurar la ventana original;
8. eliminar el respaldo temporal de NVS.

La prueba conserva una etapa persistente y puede reanudar operaciones críticas tras una interrupción.

Documento detallado:

- `docs/POC6_FRAM_PERSISTENT_TEST.md`

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El formato persistente es binario, versionado y con CRC32.
- La FRAM de 8 KiB permite iniciar con un límite comercial de 100 bloques.
- El límite se valida tanto por cantidad de bloques como por tamaño serializado.
- El gestor usa dos slots y nunca sobrescribe directamente el programa activo.
- La FRAM de 32 KiB ampliará límites y perfiles; no requerirá otro motor.
- El proyecto editable completo no pertenece a la imagen ejecutable.

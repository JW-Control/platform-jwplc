# Resultados de validación — JWPLC Logic Runtime

**Rama:** `feature/logic-runtime-poc`  
**Hardware:** JWPLC Basic  
**Estado:** PoC 0 a PoC 7 validadas.

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
| 6 | Persistencia entre reinicios reales | PASS |
| 7 | Carga desde FRAM y ejecución física | PASS |

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

Programa:

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

Implementación inicial con lecturas y escrituras individuales:

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
- conservación del Programa A en todos los cortes parciales;
- activación del Programa B únicamente con transacción completa.

## PoC 5 — backend FRAM físico

Ventana temporal:

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
- guardado y carga de los Programas A y B;
- metadatos, CRC y validador;
- reinicios lógicos del gestor;
- restauración exacta de la ventana original.

Documento detallado:

- `docs/POC5_FRAM_PHYSICAL_RESULTS.md`

## PoC 6 — persistencia entre reinicios reales

Flujo ejecutado:

```text
Etapa 1: respaldar, formatear y guardar Programa A
Etapa 2: reiniciar, cargar A y guardar Programa B
Etapa 3: reiniciar, cargar B y restaurar la ventana
```

Resultado:

```text
[PASS] Programa A persistido y validado.
[PASS] Programa A cargado despues del reinicio.
[PASS] Programa B persistido en el slot inactivo.
[PASS] Programa B cargado despues del reinicio.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.

PERSISTENCIA ENTRE REINICIOS: PASS
PoC 6 completada.
```

Conclusión:

- A y B sobreviven reinicios completos del ESP32;
- el gestor reconstruye el programa activo desde FRAM;
- la ventana original y su CRC se recuperan exactamente;
- la FRAM actual de 8 KiB permite avanzar sin esperar la variante de 32 KiB.

Documento detallado:

- `docs/POC6_FRAM_PERSISTENT_RESULTS.md`

## PoC 7 — arranque y ejecución desde FRAM

Programa persistido:

```text
I0_0 AND NOT I0_1 -> TON 2 s -> Q0_0
```

Flujo validado:

```text
FRAM -> LogicProgramStore -> LogicProgramBuffer -> RAM -> LogicEngine -> E/S física
```

Resultado de instalación y arranque:

```text
[PASS] Programa persistido y verificado en FRAM.
[PASS] Programa cargado desde FRAM hacia RAM.
[PASS] Runtime iniciado con el programa persistente.
```

Transición física observada:

```text
I0_0=1 I0_1=0 AND=1 TON=0 Q0_0=0
I0_0=1 I0_1=0 AND=1 TON=1 Q0_0=1
I0_0=0 I0_1=0 AND=0 TON=0 Q0_0=0
```

Contador de escrituras:

```text
Q0 estable apagada: 2
Q0 pasa a encendida: 3
Q0 pasa a apagada:  4
```

Rendimiento observado:

```text
scan mínimo:    4 us
scan promedio:  5 us
scan máximo:  460 us
```

Incidente encontrado y corregido:

- `LogicEngine` conservaba un puntero a un descriptor `LogicProgram` temporal;
- el primer `tick()` produjo `PROGRAM_EXECUTION_FAILED`;
- el descriptor se copia ahora por valor dentro del motor;
- los buffers `name` y `blocks` permanecen en `LogicProgramBuffer` durante la ejecución;
- la corrección se validó reutilizando el mismo programa persistido.

Cierre:

```text
Runtime detenido. Q0 apagadas.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.

ARRANQUE Y EJECUCION DESDE FRAM: PASS
PoC 7 completada.
```

Documento detallado:

- `docs/POC7_FRAM_BOOT_RUNTIME_TEST.md`

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El formato persistente es binario, versionado y con CRC32.
- La FRAM de 8 KiB permite iniciar con un límite comercial de 100 bloques.
- El límite se valida tanto por cantidad de bloques como por tamaño serializado.
- El gestor usa dos slots y nunca sobrescribe directamente el programa activo.
- La FRAM de 32 KiB ampliará límites y perfiles; no requerirá otro motor.
- El proyecto editable completo no pertenece a la imagen ejecutable.
- El mapa definitivo de producción aún no está congelado.
- La API final de instalación, activación y rollback todavía debe diseñarse antes de congelar el runtime.

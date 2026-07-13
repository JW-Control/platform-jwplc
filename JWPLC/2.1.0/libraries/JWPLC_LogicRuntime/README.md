# JWPLC_LogicRuntime

Motor lógico por bloques para **JWPLC Basic**.

La librería se integra sobre la placa existente `JWPLC Basic`. No crea una variante física nueva, no reemplaza el uso normal de sketches Arduino y no formatea la FRAM automáticamente.

## Estado

```text
PoC 0 a PoC 7 validadas
Mapa persistente v1 validado
Fase actual: integración de la API persistente pública
```

Se validó el recorrido completo:

```text
programa binario en FRAM
→ carga después de reinicio
→ reconstrucción en RAM
→ ejecución del motor
→ E/S físicas
→ parada segura
→ restauración exacta
```

## Alcance validado

- Ciclo de vida `begin()`, `loadProgram()`, `start()`, `tick()` y `stop()`.
- Perfil automático para FRAM de 8 KiB.
- Límite inicial de 100 bloques.
- Perfil futuro de 32 KiB con límite provisional de 400 bloques.
- Ejecución determinista desde RAM.
- Referencias únicamente hacia bloques anteriores.
- Validador de fuentes, tipos, recursos y salidas duplicadas.
- Entrada digital, salida digital, `NOT`, `AND`, `OR`, `SET/RESET` y `TON`.
- Lectura de entradas desde el snapshot lógico del core JWPLC.
- Escritura conjunta del banco Q0 solo cuando cambia.
- Salidas apagadas al iniciar, detener o detectar fallo.
- Codec binario portable, versionado y con CRC32.
- Almacenamiento transaccional A/B.
- Fallback ante imagen o superblock corrupto.
- Backend RAM con inyección de cortes.
- Backend sobre `JWPLC_FRAM` física.
- Persistencia entre reinicios reales.
- Carga y ejecución física desde una imagen persistida.
- Mapa persistente v1 para FRAM de 8 KiB y 32 KiB.

## Rendimiento medido

Programa de seis bloques:

```text
I0_0 AND NOT I0_1 → TON 2 s → Q0_0
```

Resultado optimizado:

```text
scan mínimo:      4–5 us
scan promedio:    5 us
scan con cambio Q0: aproximadamente 0.4–0.46 ms
```

Las entradas consumen el snapshot lógico que el core actualiza por defecto cada 20 ms. El runtime no se declara hard real-time.

## Formato binario

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

El formato no vuelca estructuras C++ directamente; usa little-endian explícito para no depender de padding, alineamiento o versión del compilador.

## Mapa persistente v1

### FRAM 8 KiB

| Región | Inicio | Fin | Tamaño |
|---|---:|---:|---:|
| Superblocks | `0x0000` | `0x003F` | 64 B |
| Slot A | `0x0040` | `0x0A3F` | 2560 B |
| Slot B | `0x0A40` | `0x143F` | 2560 B |
| Retentivos | `0x1440` | `0x1A3F` | 1536 B |
| Reserva | `0x1A40` | `0x1FFF` | 1472 B |

```text
Capacidad útil por slot: 2528 bytes
Imagen de 100 bloques:   1264 bytes
```

### FRAM 32 KiB

| Región | Inicio | Fin | Tamaño |
|---|---:|---:|---:|
| Superblocks | `0x0000` | `0x003F` | 64 B |
| Slot A | `0x0040` | `0x303F` | 12288 B |
| Slot B | `0x3040` | `0x603F` | 12288 B |
| Retentivos | `0x6040` | `0x703F` | 4096 B |
| Reserva | `0x7040` | `0x7FFF` | 4032 B |

El mapa solo se usa cuando el usuario habilita explícitamente el modo persistente. Un almacenamiento sin formato válido permanece intacto hasta recibir una orden explícita de formateo.

## API en RAM

```cpp
JWPLC_LogicRuntime runtime;

runtime.begin();
runtime.loadProgram(program);
runtime.start();

void loop()
{
  runtime.tick();
}
```

Consultas disponibles:

```cpp
runtime.state();
runtime.lastError();
runtime.validationError();
runtime.storageProfile();
runtime.storageLayout();
runtime.blockValue(index);
runtime.scanCount();
runtime.lastScanMicros();
runtime.minScanMicros();
runtime.averageScanMicros();
runtime.maxScanMicros();
runtime.outputWriteCount();
```

## API persistente pública

La fachada inicial se accede mediante `runtime.storage()`:

```cpp
runtime.storage().begin(JWPLC_FRAM);
runtime.storage().isReady();
runtime.storage().isFormatted();
runtime.storage().status();
runtime.storage().format();
runtime.storage().save(program, programId);
runtime.storage().loadActive();
runtime.storage().activeProgram();
runtime.storage().lastError();
runtime.storage().storeError();
runtime.storage().validationError();
```

El primer formateo siempre es explícito. `begin()` solo detecta el perfil y lee las firmas existentes.

Carga integrada hacia el motor:

```cpp
runtime.storage().begin(JWPLC_FRAM);
runtime.begin();
runtime.loadStoredProgram();
runtime.start();
```

La generación del programa se incrementa automáticamente durante `save()`. El rollback explícito queda como siguiente ampliación después de validar compilación, consumo de RAM y operaciones físicas reversibles de esta fachada.

## Ejemplos principales

- `JWPLC_LogicRuntime_Default`: lógica física y métricas.
- `JWPLC_LogicRuntime_Validation`: validador, 10 PASS.
- `JWPLC_LogicRuntime_Codec`: codec binario, 14 PASS.
- `JWPLC_LogicRuntime_AB_Storage`: A/B simulado, 19 PASS.
- `JWPLC_LogicRuntime_FRAM_Storage`: backend físico reversible, 22 PASS.
- `JWPLC_LogicRuntime_FRAM_Persistent`: persistencia entre reinicios.
- `JWPLC_LogicRuntime_FRAM_Boot`: carga y ejecución desde FRAM.
- `JWPLC_LogicRuntime_Storage_Layout`: validación no destructiva del mapa v1.
- `JWPLC_LogicRuntime_Storage_API_ReadOnly`: validación no destructiva de la fachada pública.

## Documentación

- `docs/LOGIC_PROGRAM_IMAGE_FORMAT_V1.md`
- `docs/LOGIC_PROGRAM_AB_STORE_V1.md`
- `docs/FRAM_MEMORY_MAP_V1.md`
- `docs/PERSISTENT_STORAGE_API_V1.md`
- `docs/POC_VALIDATION_RESULTS.md`
- `docs/POC5_FRAM_PHYSICAL_RESULTS.md`
- `docs/POC6_FRAM_PERSISTENT_RESULTS.md`
- `docs/POC7_FRAM_BOOT_RUNTIME_TEST.md`

## Decisiones vigentes

- El programa activo se ejecuta desde RAM.
- El slot activo no se sobrescribe durante una actualización.
- El orden del arreglo es el orden de ejecución.
- Cada bloque solo referencia bloques anteriores.
- Una salida física solo puede tener un bloque escritor.
- El límite se valida por cantidad de bloques y tamaño serializado.
- La FRAM de 8 KiB soporta el límite inicial de 100 bloques.
- La FRAM de 32 KiB amplía capacidades sin requerir otro motor.
- El proyecto editable completo no forma parte de la imagen ejecutable.
- Retentivos, rollback público, editor TFT, microSD y actualización remota siguen pendientes.

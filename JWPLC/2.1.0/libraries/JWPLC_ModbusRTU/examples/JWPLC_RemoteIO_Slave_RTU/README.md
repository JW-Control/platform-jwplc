# JWPLC Remote I/O Slave RTU

Ejemplo canónico de **JWPLC Basic como Remote I/O Slave Modbus RTU** para validación del Backplane de OpenPLC.

El sketch a cargar directamente en el JWPLC que actuará como Slave es:

```text
JWPLC_RemoteIO_Slave_RTU.ino
```

## Configuración del banco

```text
Slave ID: 2
Baud:     115200
Formato:  8N1
RS-485:   A/P -> A/P, B/N -> B/N y GND común
```

## Mapa expuesto

| Recurso | Dirección lógica | Función Modbus |
|---|---|---|
| Entradas remotas | `I0_0..I0_7` | FC02 Read Discrete Inputs |
| Feedback de salidas | `Q0_0..Q0_7` | FC01 Read Coils |
| Escritura de salidas | `Q0_0..Q0_7` | FC05 / FC15 |

El bitmap usa 8 bits y mantiene la correspondencia directa entre `I0_n` / `Q0_n` y el bit `n`.

## Fail-safe

El ejemplo fuerza todas las salidas a OFF cuando transcurren más de **100 ms** sin una escritura válida de coils mediante FC05/FC15.

Esto permite que una pérdida del Master o del enlace RTU lleve las salidas remotas a un estado seguro durante las pruebas de Backplane.

## Uso con OpenPLC

Para el banco de validación:

```text
JWPLC Master = firmware generado/subido por OpenPLC
JWPLC Slave  = JWPLC_RemoteIO_Slave_RTU.ino
```

No cargar este sketch en el Master cuando se esté validando el Backplane de OpenPLC.

## Estado

El ejemplo fue recuperado como baseline físico de Remote I/O y posteriormente migrado para usar `JWPLC_ModbusRTU` oficial durante Alpha7. Se conserva como ejemplo reutilizable del package y no como sketch temporal de pruebas.

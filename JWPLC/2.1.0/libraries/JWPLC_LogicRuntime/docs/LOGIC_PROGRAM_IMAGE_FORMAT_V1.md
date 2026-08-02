# Formato binario de programa lógico v1

**Estado:** PoC 3, pendiente de congelar después de pruebas con FRAM.

Este formato representa el programa ejecutable de `JWPLC_LogicRuntime`. No guarda el proyecto gráfico completo, posiciones, comentarios extensos ni recursos visuales.

## Principios

- Codificación explícita little-endian.
- No se vuelcan estructuras C++ directamente.
- Cabecera versionada.
- CRC32 separado para cabecera y payload.
- Tamaño fijo por bloque.
- Compatible con FRAM de 8 KiB y futura FRAM de 32 KiB.
- El programa se reconstruye en RAM antes de ejecutarse.

## Tamaño

```text
Tamaño total = 64 + 12 × cantidad de bloques
```

| Bloques | Tamaño de imagen |
|---:|---:|
| 6 | 136 B |
| 50 | 664 B |
| 100 | 1264 B |
| 200 | 2464 B |
| 400 | 4864 B |

El perfil inicial de 8 KiB reserva provisionalmente 2560 bytes por slot, por lo que una imagen de 100 bloques cabe con margen. El límite final también dependerá de retentivos, journal y mapa A/B.

## Cabecera de 64 bytes

| Offset | Tamaño | Campo |
|---:|---:|---|
| `0x00` | 4 | Magic `JWLR` |
| `0x04` | 2 | Versión de formato |
| `0x06` | 2 | Tamaño de cabecera |
| `0x08` | 4 | ID de programa |
| `0x0C` | 4 | Generación |
| `0x10` | 2 | Cantidad de bloques |
| `0x12` | 2 | Tamaño del registro de bloque |
| `0x14` | 4 | Longitud del payload |
| `0x18` | 4 | CRC32 del payload |
| `0x1C` | 4 | CRC32 de cabecera |
| `0x20` | 4 | Flags |
| `0x24` | 24 | Nombre UTF-8 corto, cero rellenado |
| `0x3C` | 4 | Reserva futura |

Para calcular el CRC de cabecera, el campo ubicado en `0x1C` se considera igual a cero.

## Registro de bloque de 12 bytes

| Offset relativo | Tamaño | Campo |
|---:|---:|---|
| `0x00` | 1 | Tipo de bloque |
| `0x01` | 1 | Flags reservados |
| `0x02` | 2 | Recurso físico o lógico |
| `0x04` | 2 | Fuente A |
| `0x06` | 2 | Fuente B |
| `0x08` | 4 | Parámetro |

Los valores `sourceA` y `sourceB` apuntan a bloques anteriores. `0xFFFF` representa ausencia de fuente cuando el tipo de bloque lo permite.

## CRC

Se usa CRC32 IEEE con polinomio reflejado:

```text
0xEDB88320
```

La validación ocurre en este orden:

1. Magic.
2. Versión.
3. Tamaños de cabecera y registro.
4. CRC de cabecera.
5. Cantidad de bloques y longitud.
6. CRC del payload.
7. Tipos de bloque.
8. `LogicValidator` para conexiones y recursos.

## Compatibilidad futura

- Un cambio incompatible incrementará `formatVersion`.
- Agregar nuevos tipos de bloque no debe cambiar el tamaño del registro v1 mientras sus datos quepan en los campos existentes.
- Campos adicionales deberán usar flags, reservas o una versión nueva.
- El firmware no debe ejecutar una imagen con versión desconocida.
- La activación en FRAM exigirá además estado de slot, generación y confirmación transaccional.

## Fuera de este formato

- Posición gráfica de bloques.
- Colores e iconos.
- Comentarios largos.
- Históricos.
- Biblioteca de proyectos.
- Credenciales.

Esos datos pertenecerán al proyecto editable almacenado en microSD, PC o interfaz web.

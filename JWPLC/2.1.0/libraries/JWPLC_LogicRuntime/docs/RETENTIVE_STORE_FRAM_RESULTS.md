# Resultado de validación — store retentivo en FRAM física

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 44 PASS, 0 FAIL
STORE RETENTIVO EN FRAM: PASS
```

## Compilación

```text
Flash:       436081 bytes / 3145728 bytes (13 %)
RAM global:   29644 bytes / 327680 bytes (9 %)
RAM restante: 298036 bytes
```

## Región validada

```text
FRAM física:            8192 bytes
Región retentiva:       0x1440..0x1A3F
Tamaño total:           1536 bytes
Tamaño por copia:        768 bytes
Payload por copia:       704 bytes
```

## Flujo validado

1. Se capturaron guardas de 16 bytes antes y después de la región.
2. Los 1536 bytes originales se respaldaron en NVS con CRC32.
3. Se registró una marca de restauración pendiente.
4. La región retentiva se limpió temporalmente y se verificó byte por byte.
5. El snapshot A se guardó en copia 0 con secuencia 1.
6. Una reapertura física detectó y cargó A por identidad completa.
7. El snapshot B se guardó en copia 1 con secuencia 2.
8. Una nueva reapertura física detectó y cargó B.
9. Se rechazaron Program ID, generación y cantidad de bloques diferentes.
10. Se corrompió de forma controlada el CRC de la copia B.
11. Una reapertura descartó B y recuperó A desde copia 0.
12. Las guardas anterior y posterior permanecieron intactas.
13. Los 1536 bytes originales fueron restaurados exactamente.
14. El respaldo temporal NVS fue eliminado.

## Propiedades confirmadas

- La implementación simulada se comporta igual sobre la FRAM real.
- Una copia válida basta para restaurar el snapshot anterior.
- La identidad evita aplicar estado de otro programa o generación.
- La escritura permanece contenida dentro de `0x1440..0x1A3F`.
- Superblocks, Slots A/B y la reserva no fueron modificados.
- La prueba no inicializó E/S ni conmutó salidas.
- La recuperación automática tras reinicio quedó protegida mediante NVS.

## Decisión

El store retentivo A/B v1 queda aprobado para integrarse con la fachada de alto nivel:

```text
prepareStoredProgram()
→ restoreStoredRetentiveState()
→ start() explícito
```

El guardado del estado seguirá siendo explícito y deberá ocurrir antes de `stop()`, porque `stop()` conserva su comportamiento histórico de limpiar los estados temporales en RAM.
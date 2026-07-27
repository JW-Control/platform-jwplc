# Resultado de validación — integración retentiva del runtime

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 59 PASS, 0 FAIL
INTEGRACION RETENTIVA DEL RUNTIME: PASS
```

## Compilación

```text
Flash:       445757 bytes / 3145728 bytes (14 %)
RAM global:   39500 bytes / 327680 bytes (12 %)
RAM restante: 288180 bytes
```

## Región temporalmente administrada

```text
0x0000..0x1A3F
6720 bytes
```

Incluye:

- superblocks;
- Slots A/B de programa;
- región retentiva A/B.

La reserva `0x1A40..0x1FFF` permaneció fuera de la prueba.

## Flujo validado

1. Se respaldaron 6720 bytes en NVS con CRC32.
2. Se registró una restauración pendiente para recuperación tras reinicio.
3. El program store se formateó explícitamente.
4. La región retentiva se limpió temporalmente y se verificó.
5. Programa A se guardó, preparó y cargó con identidad disponible.
6. A comenzó sin snapshot compatible y con el estado retentivo en falso.
7. El estado retentivo de A se activó en RAM y se guardó mediante la API de alto nivel.
8. `stop()` limpió el estado temporal.
9. A se preparó nuevamente y su snapshot se restauró antes de `start()`.
10. Programa B se guardó con nueva generación.
11. El snapshot de A no se aplicó a B por identidad distinta.
12. Se guardó y restauró un snapshot independiente para B.
13. Rollback a A recuperó su identidad y su snapshot.
14. Un segundo rollback a B recuperó su identidad.
15. Se corrompió de forma controlada el snapshot B.
16. B corrupto no recibió ningún snapshot y quedó en falso.
17. La copia retentiva A permaneció válida.
18. La guarda posterior a la región administrada permaneció intacta.
19. Los 6720 bytes originales fueron restaurados exactamente.
20. La fachada volvió al estado original `UNFORMATTED` y se eliminó el respaldo NVS.

## Propiedades confirmadas

- Programa y snapshot se vinculan por Program ID, generación, cantidad de bloques y tamaño del bitmap.
- Los snapshots de dos programas pueden coexistir en las dos copias retentivas.
- Rollback de programa permite recuperar el snapshot correspondiente a la identidad realmente cargada.
- Un snapshot corrupto no se aplica.
- La ausencia o incompatibilidad de snapshot deja el estado retentivo en falso sin impedir un arranque explícito posterior.
- Restaurar retentivos no inicia automáticamente el runtime.
- Guardar retentivos sigue siendo explícito y debe realizarse antes de `stop()`.
- No se escribe FRAM en cada scan ni en cada cambio del bloque SET/RESET.
- Los programas de prueba no contenían bloques `DigitalOutput`; Q0 permaneció apagado.

## Decisión

La integración retentiva v1 queda aprobada para el JWPLC Basic actual con FRAM de 8 KiB.

Flujo aprobado:

```text
prepareStoredProgram()
→ restoreStoredRetentiveState()
→ start() explícito
```

Guardado aprobado:

```text
saveStoredRetentiveState()
→ stop()
```

## Pendientes antes de congelar el backend para una interfaz funcional

1. Persistencia retentiva a través de un reinicio o corte real de alimentación, en una prueba de varias etapas.
2. Regresión completa del programa persistente no retentivo con `flags = 0` después de los cambios del codec y del motor.
3. Convivencia de runtime + TFT/botonera y medición de latencia de scan durante refresco gráfico.

El diseño visual de la interfaz puede comenzar en paralelo. La conexión de acciones reales de guardar, restaurar y RUN debe esperar estas tres pruebas de cierre.
# Resultado de validación — rollback persistente

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 34 PASS, 0 FAIL
ROLLBACK PERSISTENTE: PASS
```

Hardware utilizado:

```text
JWPLC Basic
FRAM física: 8192 bytes
Región temporal: 0x0000..0x143F
```

## Flujo validado

1. Se respaldaron los 5184 bytes del gestor A/B en NVS.
2. Se verificó el respaldo mediante CRC32.
3. Se registró una restauración pendiente.
4. Se formateó explícitamente el almacenamiento.
5. Se guardó Programa A en Slot A, generación 1.
6. Se guardó Programa B en Slot B, generación 2.
7. Se cargó Programa B como activo.
8. `storage().rollback()` verificó y reactivó Programa A.
9. El rollback avanzó la secuencia del superblock.
10. La fachada se reinicializó y volvió a cargar Programa A.
11. Se restauró exactamente la región original.
12. Se recuperó el estado previo sin formato.
13. Se eliminó el respaldo temporal de NVS.

## Estado después del rollback

```text
Slot activo:      A
Programa cargado: A
Program ID:       0xA001
Generación:       1
Secuencia:        4
```

La secuencia aumenta porque el rollback representa una nueva decisión transaccional. El ID y la generación pertenecen a la imagen reactivada y conservan sus valores originales.

## Propiedades confirmadas

- El slot alterno se carga y verifica antes de activarlo.
- Se verifican descriptor, CRC, codec y programa lógico.
- La imagen del programa anterior no se reescribe.
- La activación solo modifica el superblock redundante.
- El programa reactivado queda disponible en RAM.
- El estado sobrevive a una reinicialización de la fachada.
- La prueba no inicializa E/S ni conmuta salidas.
- El contenido original de FRAM se recuperó exactamente.

## Resultado serial relevante

```text
[PASS] Programa A guardado
[PASS] Programa B guardado
[PASS] Programa B carga como activo
[PASS] storage().rollback() reactiva candidato verificado
[PASS] rollback reactiva Slot A
[PASS] rollback recupera ID y generacion de A
[PASS] rollback deja Programa A cargado en RAM
[PASS] rollback avanza la secuencia del superblock
[PASS] Slot A permanece activo tras reinicializar
[PASS] Programa A vuelve a cargar tras reinicializar
[PASS] contenido original restaurado exactamente

Resultado: 34 PASS, 0 FAIL
ROLLBACK PERSISTENTE: PASS
```

## Conclusión

La API pública de rollback queda validada físicamente para el mapa persistente v1. La siguiente etapa es fijar la política de arranque ante almacenamiento sin formato, almacenamiento vacío, imagen activa corrupta con fallback disponible y ausencia total de imágenes válidas.

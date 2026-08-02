# Resultado de validación — integración persistente hacia runtime

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 48 PASS, 0 FAIL
INTEGRACION RUNTIME PERSISTENTE: PASS
```

La integración se validó dos veces con el mismo ejemplo:

1. build inicial compilado para 400 bloques;
2. build optimizado compilado para 100 bloques.

Ambas ejecuciones terminaron con `48 PASS, 0 FAIL`.

## Hardware

```text
JWPLC Basic
FRAM física: 8192 bytes
Región reversible: 0x0000..0x143F
```

## Flujo validado

1. `storage().begin(JWPLC_FRAM)` inicializó la fachada persistente.
2. `runtime.begin()` inicializó E/S y dejó el motor en `READY`.
3. `runtime.prepareStoredProgram()` clasificó correctamente una FRAM sin formato.
4. `start()` rechazó la ausencia de programa con `PROGRAM_NOT_LOADED`.
5. Se respaldaron 5184 bytes del gestor A/B en NVS con CRC32.
6. Se formateó el almacenamiento de forma explícita.
7. El almacenamiento vacío descargó cualquier programa del motor.
8. Se guardó Programa A y se transfirió al motor mediante la API de alto nivel.
9. Programa A inició, ejecutó un scan y se detuvo con salidas apagadas.
10. Se guardó Programa B mientras A seguía cargado en el motor.
11. La copia profunda permitió reiniciar y ejecutar A después de reutilizar los buffers de almacenamiento.
12. Programa B reemplazó correctamente la copia interna del motor.
13. Al corromper B, el runtime cargó A como fallback sin iniciar automáticamente.
14. Al corromper también A, el motor descargó completamente el programa anterior.
15. `start()` volvió a rechazar la ejecución tras el fallo persistente total.
16. La región original de FRAM fue restaurada exactamente.
17. El estado original `UNFORMATTED` volvió a detectarse.
18. El respaldo temporal de NVS fue eliminado.

## Propiedades confirmadas

- La API pública no expone los buffers internos del almacenamiento.
- `prepareStoredProgram()` no llama a `start()`.
- Un fallback válido se carga en el motor, pero exige inicio explícito.
- Un fallo total de almacenamiento descarga el programa anterior.
- El motor conserva una copia profunda del nombre y los bloques.
- `storage().save()` puede reutilizar sus buffers sin invalidar el programa cargado.
- Los programas de prueba no incluyeron bloques `DigitalOutput`; Q0 permaneció apagado.
- La restauración reversible recuperó byte por byte el contenido original.
- La reducción del build a 100 bloques no introdujo regresiones funcionales.

## Comparación directa de memoria

### Build de 400 bloques

```text
Flash:       440717 bytes / 3145728 bytes (14 %)
RAM global:   51108 bytes / 327680 bytes (15 %)
RAM restante: 276572 bytes
```

### Build de 100 bloques

```text
Flash:       440689 bytes / 3145728 bytes (14 %)
RAM global:   37908 bytes / 327680 bytes (11 %)
RAM restante: 289772 bytes
```

### Diferencia con el mismo sketch

```text
Flash:          -28 bytes
RAM global:  -13200 bytes
RAM libre:   +13200 bytes
```

El ahorro de 13200 bytes coincide exactamente con reducir de 400 a 100 bloques cuatro reservas internas:

```text
Copia de definiciones del motor: 300 × 12 B = 3600 B
Estados temporales del motor:    300 ×  8 B = 2400 B
Buffer reconstruido de storage:  300 × 12 B = 3600 B
Scratch del codec:               300 × 12 B = 3600 B
                                             --------
Total:                                       13200 B
```

El cambio reduce aproximadamente 25.8 % la RAM global usada por este sketch de integración, sin alterar su comportamiento.

## Conclusión

El flujo de producción queda validado desde FRAM hasta el motor en RAM, incluyendo clasificación de arranque, copia profunda, fallback, descarga segura y restauración exacta.

El perfil compilado predeterminado de 100 bloques queda confirmado como la decisión correcta para el JWPLC Basic actual con FRAM de 8 KiB. La capacidad futura de 400 bloques permanece disponible mediante una configuración de compilación explícita para hardware de 32 KiB.

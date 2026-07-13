# PoC 6 — resultados de persistencia entre reinicios

**Hardware:** JWPLC Basic  
**FRAM:** 8192 bytes  
**Ventana temporal:** `0x1BC0..0x1FFF`  
**Resultado:** PASS

## Etapa 1

```text
[PASS] Respaldo persistente guardado en NVS.
Etapa 1/3: formatear ventana y guardar Programa A.
[PASS] Programa A persistido y validado.
ETAPA 1 COMPLETA.
```

## Etapa 2 después de reinicio real

```text
Etapa 2/3: cargar Programa A tras reinicio y guardar B.
[PASS] Programa A cargado despues del reinicio.
[PASS] Programa B persistido en el slot inactivo.
ETAPA 2 COMPLETA.
```

## Etapa 3 después del segundo reinicio real

```text
Etapa 3/3: cargar Programa B y restaurar la ventana.
[PASS] Programa B cargado despues del reinicio.
[PASS] Contenido original de FRAM restaurado exactamente.
[PASS] Estado temporal NVS eliminado.

PERSISTENCIA ENTRE REINICIOS: PASS
PoC 6 completada.
```

## Conclusiones

- El Programa A sobrevivió un reinicio completo del ESP32.
- El Programa B se escribió en el slot inactivo y sobrevivió otro reinicio.
- Los superblocks y descriptores permitieron reconstruir el programa activo.
- La ventana original se restauró exactamente.
- El respaldo temporal de NVS se eliminó después de la restauración.
- La FRAM física actual de 8 KiB es suficiente para iniciar el desarrollo del runtime persistente.

La prueba no establece todavía el mapa final de producción ni ejecuta el programa cargado sobre las E/S. Ese flujo corresponde a PoC 7.

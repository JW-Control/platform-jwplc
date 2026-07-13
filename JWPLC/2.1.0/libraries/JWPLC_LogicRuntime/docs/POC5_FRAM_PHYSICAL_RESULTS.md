# PoC 5 — resultados del backend FRAM físico

## Estado

```text
VALIDADO EN HARDWARE
```

Hardware:

```text
JWPLC Basic
FRAM física detectada: 8192 bytes
Ventana de prueba: 0x1BC0..0x1FFF
Tamaño de ventana: 1088 bytes
```

## Resultado

```text
22 PASS, 0 FAIL
BACKEND FRAM FISICA: PASS
La ventana de prueba fue restaurada.
```

## Validaciones completadas

- Inicialización de `LogicFRAMStorage`.
- Detección de la FRAM de 8 KiB.
- Validación del layout A/B reducido.
- Respaldo temporal de la ventana.
- Formateo del almacenamiento.
- Guardado de Programa A en Slot A.
- Reinicio lógico del gestor.
- Carga y validación de Programa A.
- Guardado de Programa B en Slot B.
- Segundo reinicio lógico del gestor.
- Carga y validación de Programa B.
- Conservación de ID, generación y nombre.
- Restauración byte por byte de la ventana original.
- Verificación final de la restauración.

## Conclusión

El gestor A/B probado previamente sobre RAM funciona también sobre la FRAM física actual del JWPLC Basic usando la librería `JW_FRAM` y el bloqueo del bus SPI compartido del package.

La PoC 5 no demuestra todavía persistencia a través de un reinicio real del ESP32, porque los reinicios usados dentro del ejemplo fueron reinicios lógicos del objeto `LogicProgramStore`. Esa validación corresponde a la PoC 6.

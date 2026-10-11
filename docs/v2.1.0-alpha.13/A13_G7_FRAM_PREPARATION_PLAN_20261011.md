# Alpha13-G7 — Plan de calificación FRAM (A13-009 + A13-013)

**Estado:** PREPARATION_NOT_VALIDATED. Fecha: 2026-10-11.
Rama: `v2.1.0-alpha.13/feature/cleanup-robustness`.

## Línea base confirmada

- G1–G6: `CLOSED_PASS`. G6 produjo los commits de producto
  `575144f49f1a392615ee3e0c1b9e137202ff532d` y documentación
  `17c37355e851bcc4306b8dd737eedfa45eef05b2`.
- `JW_FRAM 1.0.3` se distribuye con `precompiled=full` y
  `src/esp32/libJW_FRAM.a`. **Cambiar un archivo .cpp sin regenerar
  el .a no garantiza que el comportamiento cambie.**
- `JWPLC_LogicRuntime` define un mapa de FRAM 8 KiB:
  superblocks `0x0000..0x003F`; A `0x0040..0x0A3F`;
  B `0x0A40..0x143F`; retentivos `0x1440..0x1A3F`;
  reserva `0x1A40..0x1FFF`. Este mapa solo aplica si el runtime
  persistente se habilita explícitamente. **Ni siquiera la reserva
  se considera libre para un test sin comprobar propiedad.**
- La API básica tiene escrituras manuales `writeEnable()`/WREN
  y operaciones de alto nivel `put()`, `writeBlock()`,
  `writeString()` con WREN automático. No romper estas firmas.

## Riesgos observables en fuente — hipótesis a contrastar

1. `JW_FRAM::read(buf, count)` y `write(buf, count)` no comprueban
   punteros nulos cuando `count > 0`, antes de pasarlos a
   `Adafruit_SPIDevice`. Se debe revisar el manejo de fallos.
2. `readBlock<T>()` modifica `value` **antes** de comprobar checksum.
   Si el bloque es inválido, el método devuelve `false` pero deja en
   el resultado bytes potencialmente corruptos. Cualquier cambio debe
   preservar tipos trivially copyable sin imponer requisitos de
   constructibilidad adicionales ni introducir grandes buffers de stack.
3. `readCString` comprueba el rango para `maxLen+1` completo;
   en la frontera de la FRAM una cadena corta terminada en `\0`
   puede rechazarse aunque quepa. Verificar intención de API,
   compatibilidad y tratamiento de buffers destino.
4. Las secuencias de WREN, WRITE y WRDI son operaciones separadas.
   La atomicidad de secuencias de alto nivel ante varias tareas requiere
   revisión del contrato de bloqueo del bus SPI; no modificar
   unilateralmente la gestión compartida con TFT/SD/Ethernet.

Estos cuatro puntos son observaciones sobre el código, **no una
atribución verificada individual a A13-009 o A13-013**. La auditoría
canónica original no está disponible íntegramente en la rama actual.
No cerrar esos IDs hasta comprobar correspondencia o dejar un
pendiente explícito.

## Estrategia obligatoria de ejecución G7

**P0 / preflight no destructivo (preparado):**
`tools/alpha13/gates/a13_g7_fram_preflight.py`.

- Exigir CLI oficial por ruta absoluta, registrar SHA y versión.
  Rechazar `Arduino PLC IDE Tools`. Comprobar `jwplc_local:esp32`.
- Identificar branch, HEAD, worktree y archivado FRAM original;
  inventariar fuentes, `library.properties`, dependencia BusIO,
  existencia y tamaño/hash del `libJW_FRAM.a`.
- Compilar ejemplos FRAM existentes usando el archivo precompilado
  normal; comprobar origen exacto de librería y presencia del archive
  en el enlace. No escribir FRAM ni subir firmware.
- Guardar evidencia legible en `tools/alpha13/results/` (ignorado).
  No hacer commit ni mutar el producto.

**Calificación de candidato y .a (pendiente de diseñar/ejecutar):**

- Implementar modificaciones mínimas y reversibles en
  `tools/alpha13/candidates/g7/`, fuera del producto.
- Construir desde fuente usando selección de Adafruit BusIO **bundled**,
  comprobar objeto único `JW_FRAM.cpp.o` y resultado del linker.
- Construir y extraer `libJW_FRAM.a` en TEMP: igualdad byte-a-byte
  objeto fuente ↔ miembro del archive y enlace normal comprobado.
- Compilar/regresar API básica, Strings, structs, LogicFRAMStorage
  y configuración de periferia global.

**Físico FRAM:** El autoload y el hardware en banco no implican que
sus datos sean desechables. No usar `format()`, no escribir en
`0x0000` ni en retentivos/slots, ni asumir que reserva es libre.
Para las pruebas que necesiten escribir:
identificar con el operador una ventana físicamente libre;
respaldar bytes originales y guardas con CRC, restaurarlos tras
la prueba y verificar igualdad exacta. Preferir tests de rechazo
de API que no escriban nada hasta asegurar ventana. El respaldo
debe ser recuperable ante reinicio inesperado.

**Cierre:** Solo tras PASS completo, adoptar archivos fuente+archive
con backups, regresión normal y hashes, commits y push automáticos
(no `AUTORIZO_COMMIT_G7`). Seguir exigiendo confirmación de banco
antes del upload físico; no PR/release/PreRelease de Alpha13 aún.

## Contratos / límites

```text
OPENPLC=OUT_OF_SCOPE
HMI_DESIGNER=OUT_OF_SCOPE
TFT_NEW_FEATURES=OUT_OF_SCOPE
G7_PRODUCT_CHANGED=NO
G7_ARCHIVE_REBUILT=NO
G7_PHYSICAL=NOT_EXECUTED
G7_CLOSURE=NOT_EXECUTED
```

No ejecutar el script heredado
`tools/alpha12/gates/alpha12_jw_fram_precompiled_refresh.ps1`
sin revisar antes su CLI predeterminado y sus rutas de rama.

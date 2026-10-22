# Alpha13 G9 — Plan smoke físico acotado SD/RTC/botonera

**Estado:** `PREPARED_HARDWARE_AVAILABILITY_PENDING`.
**Implementación productiva base:** commit
`63e0291a7b1480af846e955559ab175aa78e7300`.
**Alpha13:** sigue `IN_PROGRESS`.

## Motivo

G9 modifica el source y archivo precompilado `JW_SD` y
simplifica estados internos RTC/botonera en
`JWPLC_GlobalPeripherals`. Los checks de compilación,
fuente→objeto→archive y cuatro builds normales pasaron,
pero no representan prueba eléctrica con el nuevo paquete.

Para un cierre funcional defendible, ejecutar una única
prueba de banco de **observación y lectura**, sin
operaciones que modifiquen archivos microSD ni actúen
relés. No se necesita repetir Modbus RTU del gate G8.

## Equipo y condiciones previas

- Un `JWPLC Basic v2` disponible en banco sin cargas ni
  conexiones a maquinaria. El operador tiene COM4 y COM14
  de G8; confirmar cuál usará para esta prueba y que sigue
  accesible. El programa G8 previo en esa placa se
  **sobrescribirá**; no se restaura automáticamente.
- **Confirmar microSD instalada** en la placa seleccionada.
  Sin tarjeta, no dar por aprobada la parte física SD.
- Cable USB/CH340 y alimentación de banco, motor/DO
  desconectados, monitor serial cerrado.
- Usar **Arduino CLI 1.5.1** y
  `jwplc_local:esp32:jwplcbasic`, variante productiva
  normal y autoload completo de periféricos.
- Verificar antes del upload que el archive
  `JWPLC/2.1.0/libraries/JW_SD/src/esp32/libJW_SD.a`
  es el commit productivo con SHA-256
  `58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e`
  y que GlobalPeripherals se compila desde source.

## Prueba propuesta (runner aún NO creado)

1. Preflight no invasivo: Git, FQBN, archive, puerto USB,
   autorización expresa de upload y microSD insertada.
2. Compilar un sketch de test **sin suprimir ningún
   periférico** y verificar la selección normal.
   No usar overlays ni `precompiled` alternativos.
3. Upload controlado solo a la placa elegida.
   Validar marcador serial propio del sketch, evitando
   una lectura accidental de firmware viejo.
4. SD: confirmar `JWPLC_SD.isCardPresent()`,
   `JWPLC_SD.isReady()` y realizar una operación
   **solo de lectura** sobre una ruta segura existente,
   o una consulta read-only de directorio sin crear/borrar
   ficheros. Distinguir `NO_CARD`, `INIT_FAILED`,
   `READ_FAILED` y `PASS`.
5. RTC: confirmar callback, presencia/lectura válida cuando
   esté correctamente alimentado/configurado; guardar
   telemetría `present`, `valid`, `lostPower`,
   sin cambiar reloj ni borrar estado del chip.
   Si estado RTC previo es inválido, diagnosticar y
   no atribuir automáticamente al refactor.
6. Botonera: operador pulsa/libera al menos un botón;
   detectar evento real mediante `pressed/released`
   y `isDown`; conservar refresco/throttle de 25 ms,
   no bloquear core. No automatizar pulsación inexistente.
7. Capturar logs y marcar PASS solo con evidencias
   físicas. Guardar el manifiesto local bajo
   `tools/alpha13/results/` y, si corresponde,
   documentar decisión de hardware pendiente.

## Evidencia y cierre

```text
G9_PRODUCT_COMMIT=63e0291a7b1480af846e955559ab175aa78e7300
G9_ARCHIVE_SHA256=58e86fdf31c9bf94204fcbcf7462656fd9f4d9a7d73bff35bd660dbff538b89e
G9_GIT=CLOSED_PASS
G9_PHYSICAL_SD=NOT_EXECUTED
G9_PHYSICAL_RTC=NOT_EXECUTED
G9_PHYSICAL_BUTTONS=NOT_EXECUTED
G9_PHYSICAL_SCRIPT=NOT_YET_IMPLEMENTED
G9_HARDWARE_PORT=AWAITING_OPERATOR_CONFIRMATION
G9_MICROSD_CARD=AWAITING_OPERATOR_CONFIRMATION
G9_FUNCTIONAL=IN_PROGRESS
ALPHA13=IN_PROGRESS
```

No saltar a G10 de release final ni declarar PASS físico
hasta resolver la disponibilidad y registrar la
evidencia, o una excepción documentada y explícitamente
aceptada con pruebas equivalentes suficientes.

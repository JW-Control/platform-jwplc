# Prueba — sesión estructural RAM v0.5.5

## Objetivo

Validar la base transaccional necesaria antes de dibujar el bloque virtual `+` en el mapa FBD.

Esta revisión todavía no agrega el asistente gráfico `NUEVO BLOQUE`. Primero cierra las operaciones estructurales sobre el borrador RAM:

- agregar un bloque al final del orden topológico;
- contar consumidores;
- impedir eliminar un bloque utilizado;
- eliminar un bloque sin consumidores;
- compactar bloques y enlaces;
- corregir índices y `firstInput`;
- validar y aplicar el programa completo al motor;
- mantener operaciones inválidas sin efectos parciales.

## API incorporada

```cpp
bool appendBlock(LogicV2BlockType type,
                 const LogicV2InputLink *inputs,
                 uint8_t inputCount,
                 uint16_t resource = 0,
                 uint32_t parameter = 0,
                 uint16_t *newBlockIndex = nullptr);

uint16_t consumerCount(uint16_t blockIndex) const;
bool hasConsumers(uint16_t blockIndex) const;
bool removeBlock(uint16_t blockIndex);
```

## Reglas de esta primera etapa

### Creación

- Solo se agrega al final del arreglo de bloques.
- Las fuentes deben ser bloques anteriores o constantes válidas.
- La cantidad de entradas, recurso y parámetro deben validar para el tipo.
- Si la operación falla, los conteos y el contenido del borrador no cambian.
- Todavía no existe inserción en medio del orden topológico.

### Eliminación

- No hay eliminación en cascada.
- No se desconectan consumidores automáticamente.
- Un bloque con consumidores no se puede eliminar.
- El último bloque del programa no se puede eliminar porque el motor v2 todavía rechaza programas vacíos.
- Al eliminar se compactan los arreglos y se corrigen referencias posteriores.
- La operación usa un respaldo temporal para restaurar exactamente el borrador si la validación final falla.

## Ejemplo de prueba

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_EditSession_Structural_RAM
```

No requiere entrar a USER ni utilizar la TFT. Toda la salida aparece por Serial a `115200`.

## Programa base

```text
B00 I0.0
B01 I0.1
B02 AND(B00, B01)       sin consumidores
B03 NOT(B00)
B04 Q0.0 <- B03
```

El bloque `B02` permite probar la eliminación y compactación sin afectar la rama activa `B03 → B04`.

## Secuencia validada por el sketch

1. Cargar e iniciar el programa base.
2. Abrir una sesión transaccional.
3. Confirmar `5 bloques / 4 enlaces`.
4. Confirmar que `B02` no tiene consumidores.
5. Confirmar que `B03` sí tiene un consumidor.
6. Rechazar la eliminación de `B03`.
7. Eliminar `B02`.
8. Confirmar la compactación:
   - antiguo `B03 NOT` pasa a `B02`;
   - antiguo `B04 Q0.0` pasa a `B03`;
   - sus `firstInput` se reducen;
   - la fuente de Q cambia de índice `3` a `2`.
9. Agregar un TON al final.
10. Agregar un NOT que consuma el TON.
11. Rechazar la eliminación del TON mientras tenga consumidor.
12. Eliminar primero el NOT y después el TON.
13. Intentar agregar una segunda `Q0.0` y verificar rollback atómico.
14. Intentar agregar un NOT con fuente inexistente y verificar rollback atómico.
15. Agregar un OR válido.
16. Aplicar el borrador al motor.
17. Ejecutar un scan y verificar los valores resultantes.

## Salida esperada

Cada validación debe aparecer como:

```text
[OK]   descripción
```

Al final:

```text
RESULTADO: 29 OK, 0 FAIL
PRUEBA APROBADA
```

El número exacto de verificaciones puede aumentar si se amplía el ejemplo; el criterio principal es `0 FAIL`.

## Criterio de aprobación

```text
Compila desde Arduino IDE.
El programa base valida y arranca.
appendBlock agrega únicamente programas válidos.
Un append inválido no modifica conteos ni contenido.
consumerCount identifica usos reales.
removeBlock rechaza bloques utilizados.
removeBlock compacta bloques y enlaces.
Las fuentes mayores al índice eliminado se decrementan.
Los firstInput posteriores se corrigen.
El borrador final valida.
apply recarga y reinicia el motor.
El scan posterior entrega los valores esperados.
No se escribe FRAM.
No se utiliza la TFT.
No se conmutan salidas físicas.
```

## Siguiente incremento

Después de aprobar esta prueba física/compilación, el siguiente paso es exponer estas operaciones en el mapa FBD mediante:

1. bloque virtual `+` al final del programa;
2. pantalla `NUEVO BLOQUE`;
3. creación inicial de un subconjunto pequeño y representativo (`DI`, `NOT`, `AND`, `TON`, `DO`);
4. foco `ACCIONES` en DETALLE;
5. eliminación con mensaje de consumidores cuando corresponda.

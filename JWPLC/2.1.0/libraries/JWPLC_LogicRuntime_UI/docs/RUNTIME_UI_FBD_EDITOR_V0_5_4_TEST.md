# Prueba física — editor FBD v0.5.4

## Objetivo

Validar la revisión de refresco gráfico y la nueva aceleración de `EDITAR T`:

1. eliminar redibujados globales durante cambios normales del runtime;
2. actualizar únicamente bloques, enlaces, pines o textos cuyo estado cambió;
3. acelerar `UP/DOWN` mantenido usando `JW_MatrixButtons::applyAxis()` y sus multiplicadores;
4. conservar pulsaciones cortas de una unidad;
5. mantener `Ta LECTURA` en vivo sin parpadeo.

## Ejemplo

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

## Serial esperado

```text
Motor v2: RUNNING
UI FBD v0.5.4: REFRESCO PARCIAL + REPEAT RAPIDO
```

## Política de redibujado

El redibujado completo queda reservado para:

- entrada a una pantalla;
- cambio de página o ventana visible;
- cambio de programa o layout;
- aplicación de una edición que recarga el motor;
- `forceRedraw()` explícito.

Durante la ejecución normal no se debe limpiar todo el panel.

## Prueba 1 — MAPA FBD sin parpadeo

1. Permanecer en `MAPA FBD` durante dos ciclos completos del ejemplo.
2. Observar los cambios de `B04`, `B05`, `B06`, `B07`, `B08`, `B09` y `B10`.

Resultado esperado:

- solo se repinta el bloque que cambia;
- solo se recolorean sus enlaces de salida;
- se repinta el bloque consumidor cuando cambia el color de uno de sus pines;
- los demás bloques permanecen visualmente inmóviles;
- no se limpia el área completa del mapa;
- no se aprecia destello negro o gris en todo el panel.

## Prueba 2 — selección en MAPA FBD

1. Moverse con `LEFT/RIGHT/UP/DOWN` entre bloques visibles.
2. Repetir sin provocar desplazamiento de ventana.
3. Después navegar hasta provocar un cambio real de ventana o viewport.

Resultado esperado:

- sin desplazamiento: solo se actualizan el bloque anterior, el nuevo y el texto del encabezado;
- con desplazamiento: se permite un redibujado completo porque cambia la geometría visible;
- no quedan bordes amarillos residuales.

## Prueba 3 — DETALLE con señales dinámicas

1. Entrar al detalle de `AND`, `OR`, `SR` y `TON`.
2. Permanecer sin pulsar botones mientras cambian las señales.

Resultado esperado:

- solo cambia la fila de entrada afectada;
- solo cambia su enlace y pin de entrada;
- el bloque central solo cambia cuando cambia su salida;
- en TON, `Ta` cambia dentro de su línea, sin reconstruir el bloque;
- no parpadean las demás filas ni el fondo completo.

## Prueba 4 — navegación dentro de DETALLE

1. Cambiar la entrada seleccionada con `UP/DOWN`.
2. En TON, alternar `ENTRADAS/PARAMETROS` con `LEFT/RIGHT`.

Resultado esperado:

- se repintan únicamente el elemento anterior y el nuevo;
- el panel `T/Ta` solo se reconstruye al cambiar su condición de selección;
- no se limpia todo el detalle.

## Prueba 5 — pulsación corta en EDITAR T

1. Abrir `B07 TON → PARAM T → EDITAR T`.
2. Mantener el foco en `VALOR`.
3. Pulsar brevemente `UP` y `DOWN`.

Resultado esperado:

```text
UP corto   → +1 unidad
DOWN corto → -1 unidad
```

No debe existir salto circular en los límites.

## Prueba 6 — repeat rápido por unidad

La UI aplica temporalmente este perfil mientras el foco está en `VALOR`:

```text
Retardo inicial: 170 ms
Umbrales:        3 / 8 / 15 repeats
Intervalos:      85 / 65 / 48 / 35 ms
```

Multiplicadores:

| Unidad | Etapa 1 | Etapa 2 | Etapa 3 | Etapa 4 |
|---|---:|---:|---:|---:|
| ms | 1 | 10 | 100 | 1000 |
| s | 1 | 2 | 10 | 30 |
| min | 1 | 1 | 2 | 5 |
| h | 1 | 1 | 1 | 2 |

Procedimiento:

1. Probar `ms`, `s`, `min` y `h`.
2. Mantener `UP` durante dos segundos.
3. Mantener `DOWN` durante dos segundos.

Resultado esperado:

- el primer cambio es inmediato;
- el avance se vuelve perceptiblemente más rápido antes del primer segundo;
- en `ms` se alcanzan saltos de 10, 100 y 1000;
- en `s` se alcanzan saltos de 2, 10 y 30;
- al soltar, el valor se detiene;
- el botón completo no parpadea: solo cambia la línea `VALOR <...>`;
- al salir de `EDITAR T`, se restaura el perfil normal de navegación.

## Prueba 7 — Ta vivo en EDITAR T

1. Permanecer en `EDITAR T` sin tocar botones.
2. Esperar el inicio del TON.

Resultado esperado:

- `Ta LECTURA` se consulta cada 100 ms;
- solo se escribe cuando cambia el texto formateado o su color;
- no se reconstruyen `VALOR`, `UNIDAD`, encabezado ni panel completo;
- no se aprecia parpadeo.

## Prueba 8 — edición transaccional

1. Cambiar `VALOR` y `UNIDAD`.
2. Pulsar `ESC` y confirmar que el motor conserva el valor previo.
3. Repetir y pulsar `OK`.

Resultado esperado:

- `ESC` cancela el borrador;
- `OK` muestra `APLICANDO CAMBIOS...`;
- la aplicación se procesa fuera del callback TFT;
- el motor se recarga en RAM;
- no se escribe FRAM;
- no se conmutan salidas físicas.

## Criterio de aprobación

```text
Compila y sube desde Arduino IDE.
Aparece UI FBD v0.5.4 en Serial.
No hay parpadeo general en MAPA FBD.
No hay parpadeo general en DETALLE.
Solo cambian regiones cuyo estado o selección cambió.
UP/DOWN corto cambia una unidad.
UP/DOWN mantenido acelera según la unidad.
Durante repeat solo cambia el texto de VALOR.
Ta se actualiza sin interacción y sin reconstruir el panel.
ESC cancela y OK aplica en RAM.
No se escribe FRAM.
No se conmutan salidas físicas.
```

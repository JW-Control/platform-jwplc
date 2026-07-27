# Validación física — mapa FBD v0.4.1

## Objetivo

Confirmar que el mapa FBD conserva la navegación y distribución lógica de v0.4.0 sin dejar contenido residual ni invadir otras regiones de la pantalla.

## Cambios bajo prueba

```text
PANEL exterior separado
franja INFO exclusiva
viewport MAP exclusivo
limpieza independiente de INFO y MAP
nodos solo si entran completos
cables solo entre nodos completamente visibles
redibujado total del viewport al cambiar selección o señal
reconstrucción del layout si cambia cantidad de bloques o enlaces
```

También se corrigió el formato de roles en detalle para evitar textos como `S0` o `R0`.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

El monitor Serial debe indicar:

```text
UI FBD v0.4.1: LISTA
```

## Verificaciones

### Vista inicial

- La cabecera `MAPA FBD` permanece limpia.
- La insignia `RUN` no es cubierta por nodos ni cables.
- La línea `Bxx n/11 X:n Y:n` ocupa únicamente la franja INFO.
- El mapa comienza debajo del separador horizontal.
- `B03` no debe aparecer cortado sobre el footer; puede quedar oculto hasta desplazar el viewport.

### Navegación vertical

Recorrer `B00`, `B01`, `B02` y `B03` con `UP/DOWN`:

- no deben quedar cajas fantasma;
- no deben quedar textos de la posición anterior;
- ningún nodo debe tocar la cabecera ni el footer;
- el viewport debe desplazarse solo al necesitar mostrar completo el seleccionado.

### Navegación horizontal

Avanzar por conexiones con `RIGHT` hasta `B10` y regresar con `LEFT`:

- las posiciones lógicas se mantienen;
- no quedan trazos de columnas anteriores;
- nodos fuera de pantalla se omiten completos;
- los cables se muestran únicamente cuando ambos extremos están visibles;
- el borde exterior del panel permanece intacto.

### Señales automáticas

Durante el ciclo de nueve segundos:

- señales `TRUE` cambian a verde;
- señales `FALSE` vuelven a gris;
- el refresco de estados no deja colores ni cables residuales;
- el borde amarillo de selección no se confunde con el verde lógico.

### Detalle

Abrir `B06 SET/RESET` y `B07 TON`:

- `S` y `R` no deben mostrar un cero añadido;
- TON debe mostrar `IDLE`, `CONTANDO` o `LISTO`;
- tiempo transcurrido y restante deben actualizarse sin invadir otros campos;
- `OK` regresa al mapa completamente limpio.

## Aislamiento

```text
FRAM no utilizada
almacenamiento A/B no utilizado
salidas Q0 físicas no conmutadas
codec v1 sin cambios
edición gráfica deshabilitada
autoload normal conservado
```

## Datos requeridos

```text
Flash usada
RAM global
RAM restante
log Serial de arranque
foto de vista inicial
foto tras scroll vertical
foto tras scroll horizontal
foto de detalle B06
foto de detalle B07 contando o listo
```

## Criterio de aprobación

```text
SIN RESIDUOS: OK
SIN SUPERPOSICIÓN: OK
CABECERA Y FOOTER PROTEGIDOS: OK
FRANJA INFO AISLADA: OK
MAPA ESTABLE: OK
NAVEGACIÓN POR CONEXIONES: OK
TON EN VIVO: OK
```

El clipping parcial y los indicadores de conexiones fuera de pantalla no forman parte de esta corrección.

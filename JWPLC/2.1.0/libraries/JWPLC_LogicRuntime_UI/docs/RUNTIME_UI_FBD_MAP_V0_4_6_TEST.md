# Prueba física — mapa FBD v0.4.6

## Objetivo

Validar la nueva ventana horizontal discreta de cinco slots:

```text
slot 0 | slot 1 | slot 2 | slot 3 | slot 4
lateral| central| central| central| lateral
```

Los tres slots centrales deben mantener exactamente las mismas coordenadas en todas las vistas.

## Preparación

1. Actualizar el branch:

```bash
git pull
```

2. Abrir:

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime_UI
→ JWPLC_LogicRuntime_UI_FBD_Map_V2_RAM
```

3. Compilar y subir al JWPLC Basic.
4. Abrir el monitor Serial a 115200 baud.

## Salida esperada

```text
Motor v2: RUNNING
UI FBD v0.4.6: LISTA
```

## Casos visuales

### 1. Extremo izquierdo

Seleccionar B00, B04, B05, B06 o B07 sin cruzar al nivel siguiente.

Esperado:

- niveles 0, 1, 2 y 3 a tamaño completo;
- nivel 4 reducido en el slot derecho;
- ningún preview izquierdo;
- columnas completas en coordenadas fijas;
- B08/B09 no cubren el contenido principal.

### 2. Posición intermedia

Navegar hasta B08 o B09.

Esperado:

- nivel 1 reducido en el slot izquierdo;
- niveles 2, 3 y 4 completos en los tres slots centrales;
- nivel 5 reducido en el slot derecho;
- B06, B07 y B08/B09 alineados como en la composición aprobada por el usuario;
- ninguna columna central cambia su coordenada respecto a las otras vistas intermedias.

### 3. Extremo derecho

Navegar hasta B10.

Esperado:

- nivel 1 reducido en el slot izquierdo;
- niveles 2, 3 y 4 completos en los tres slots centrales;
- nivel 5 completo en el slot derecho;
- ningún preview derecho;
- B10 completamente visible y seleccionable.

## Cableado

Confirmar:

- continuidad entre previews y bloques completos;
- cables recortados correctamente en los bordes;
- cuatro conexiones de B04 visibles;
- entrada negada de B04 con burbuja gris;
- conexiones S/R y TON sin saltos al cambiar de ventana;
- ausencia de residuos tras pasar entre extremo izquierdo, intermedio y extremo derecho.

## Refresco

Durante el ciclo automático de nueve segundos verificar:

- cambios de verde/gris sin parpadeo fuerte;
- geometría inmóvil mientras solo cambia el estado lógico;
- redibujado completo únicamente al cambiar de ventana horizontal o vertical.

## Detalle

El detalle gráfico debe conservar el comportamiento de v0.4.5:

- UP/DOWN recorre entradas;
- LEFT salta a la fuente;
- OK regresa al mapa;
- sin edición ni escritura FRAM en esta etapa.

## Criterio de aprobación

```text
COMPILA: sí
EXTREMO IZQUIERDO: correcto
INTERMEDIO: correcto
EXTREMO DERECHO: correcto
CABLEADO: correcto
PARPADEO: aceptable
DETALLE: sin regresiones
```

Con esta aprobación se habilita la siguiente etapa: editor RAM en STOP para fuentes, negación, cantidad de entradas, recursos I/Q y parámetro TON.

# Resultado físico — mapa FBD v0.4.6

## Estado

```text
COMPILACIÓN: APROBADA
EJECUCIÓN: APROBADA
GEOMETRÍA DE CINCO SLOTS: APROBADA
DETALLE GRÁFICO: APROBADO COMO BASE
RESULTADO DEL USUARIO: CONTENTO / CIERRE VISUAL
```

## Resultado confirmado

Las fotografías físicas confirmaron los tres casos horizontales:

### Extremo izquierdo

```text
FULL | FULL | FULL | FULL | MINI >
```

### Posición intermedia

```text
< MINI | FULL | FULL | FULL | MINI >
```

### Extremo derecho

```text
< MINI | FULL | FULL | FULL | FULL
```

Los tres slots centrales conservan coordenadas fijas y los previews laterales representan únicamente la columna adyacente fuera del área principal.

## Elementos aprobados

- bloques compactos de dos líneas;
- tres columnas centrales inmóviles;
- previews laterales simétricos;
- cableado recortado dentro del panel;
- selección amarilla;
- señales activas en verde;
- burbuja de negación gris;
- detalle gráfico de fuentes y bloque destino;
- navegación completa del programa;
- ausencia de footer inferior redundante.

## Microfix incorporado al iniciar v0.5.0

Se detectó que las repeticiones generadas mientras un botón seguía presionado podían llegar a la pantalla siguiente aunque la cola se limpiara una vez.

La solución adoptada es una compuerta de liberación:

```text
Cambiar de pantalla.
Limpiar eventos pendientes.
Ignorar y seguir limpiando mientras exista un botón físicamente presionado.
Habilitar la pantalla nueva solo después de liberar todos los botones.
```

Esta regla se aplica a:

- mapa → detalle;
- detalle → mapa;
- detalle → bloque fuente;
- detalle → editor;
- editor → detalle por aplicar;
- editor → detalle por cancelar.

## Decisión

La geometría del mapa v0.4.6 queda congelada como base de `JWPLC_LogicRuntime_UI v0.5.0`.

Los cambios siguientes deben concentrarse en edición funcional y no deben volver a modificar la distribución general sin una observación física nueva.

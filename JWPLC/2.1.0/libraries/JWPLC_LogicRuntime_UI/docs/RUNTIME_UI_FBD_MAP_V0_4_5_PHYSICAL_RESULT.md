# Resultado físico — mapa FBD v0.4.5

## Estado

```text
COMPILACIÓN: aprobada por el usuario
EJECUCIÓN: aprobada
NAVEGACIÓN: operativa de B00 a B10
DETALLE GRÁFICO: aprobado visualmente
GEOMETRÍA HORIZONTAL: requiere corrección discreta
RESULTADO GENERAL: APROBADO PARCIAL
```

## Avances confirmados

Las fotografías físicas confirmaron:

- eliminación correcta del footer inferior;
- bloques compactos de dos líneas;
- cuatro filas visibles en el mapa;
- detalle gráfico legible para I, AND, OR, SR, TON, NOT y Q;
- señales activas en verde y selección en amarillo;
- menor parpadeo durante cambios lógicos;
- navegación completa hasta las salidas B09 y B10.

## Irregularidad encontrada

La v0.4.5 conserva un viewport horizontal continuo y reserva espacio lateral de manera dinámica. Como consecuencia:

- las columnas centrales cambian de posición entre vistas;
- una vista intermedia puede quedar desplazada hacia un lateral;
- los previews laterales no son perfectamente simétricos;
- ciertos cables terminan o comienzan en posiciones diferentes al cambiar el viewport;
- el resultado depende de la cantidad de contenido detectado fuera de pantalla.

## Geometría acordada para v0.4.6

La pantalla se divide en cinco slots físicos:

```text
LATERAL | CENTRAL 1 | CENTRAL 2 | CENTRAL 3 | LATERAL
```

Reglas:

1. Los tres slots centrales nunca cambian de coordenadas.
2. En el extremo izquierdo, los niveles 0 a 3 se muestran completos y el siguiente nivel aparece reducido a la derecha.
3. En posiciones intermedias, los tres niveles centrales se muestran completos y los niveles adyacentes aparecen reducidos a ambos lados.
4. En el extremo derecho, el nivel final se muestra completo en el slot derecho y el nivel anterior externo aparece reducido a la izquierda.
5. El desplazamiento se realiza por niveles completos, no por píxeles.
6. Solo se representa la columna inmediatamente anterior o siguiente.

## Edición

El detalle de v0.4.5 sigue siendo de solo lectura. La edición de conexiones, inversión de pines y parámetros se desarrollará después de aprobar físicamente la geometría v0.4.6.

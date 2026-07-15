# Resultado físico parcial — mapa FBD v0.4.2

## Estado

```text
COMPILACIÓN: aprobada por el usuario
MOTOR V2: RUNNING
MAPA ESTABLE: operativo
RESULTADO VISUAL: PARCIAL / REQUIERE AJUSTES
```

## Mejoras confirmadas

Las fotografías físicas validan:

```text
metadata movida al encabezado
área gráfica extendida hasta el footer
bloques más anchos
separación entre gutter de pines y contenido
etiquetas 1, 2, !3, 4, S, R y T visibles
navegación horizontal hasta NOT y DigitalOutput
señales TRUE en verde
```

La representación general ya se reconoce como un mapa FBD conectado y la v0.4.2 eliminó las superposiciones principales observadas en v0.4.1.

## Observaciones pendientes

El usuario identificó dos problemas:

1. Al desplazarse, normalmente solo quedan tres bloques completos visibles y el bloque inmediatamente anterior o siguiente desaparece por completo.
2. Los cables se omiten cuando la fuente o el consumidor no está totalmente dentro del viewport, por lo que algunas entradas de AND4 no muestran toda su continuidad.

## Decisión para v0.4.3

```text
Mantener bloques completos con su información actual.
Dibujar bloques parcialmente visibles en modo resumido.
Mostrar una etiqueta de borde Bxx TIPO para el siguiente bloque cercano.
Recortar cada segmento horizontal/vertical del cable contra el mapa.
No exigir que ambos nodos estén totalmente visibles.
Separar las rutas de múltiples entradas usando carriles distintos.
Dibujar un stub para entradas especiales X, HI y LO.
```

La v0.4.2 queda aprobada como avance visual, pero no como cierre definitivo del mapa FBD.

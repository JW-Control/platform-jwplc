# Resultado físico parcial — mapa FBD v0.4.1

## Estado

```text
COMPILACIÓN: aprobada por el usuario
MOTOR V2: RUNNING
MAPA CONECTADO: visible
RESULTADO VISUAL: PARCIAL / REQUIERE AJUSTES
```

## Avances confirmados

La v0.4.1 eliminó la mayor parte de los residuos que aparecían al desplazar el viewport. El usuario confirmó que la presentación mejoró y que el conjunto ya se percibe como un mapa FBD conectado, no como páginas independientes por bloque.

También se observaron correctamente:

```text
selección amarilla
señales TRUE verdes
navegación horizontal y vertical
bloques AND, OR, SR y TON
scroll hasta NOT y DigitalOutput
```

## Observaciones pendientes

Las fotografías físicas mostraron tres ajustes necesarios:

1. La franja `Bxx x/xx X:a Y:b` consume altura útil dentro del panel.
2. El panel gráfico todavía deja espacio libre antes del footer.
3. Las etiquetas de entrada, la burbuja de negación y el título del bloque comparten la misma zona y se superponen visualmente.

## Decisión para v0.4.2

```text
Mover Bxx/posición al encabezado, entre MAPA FBD y RUN.
Extender el panel hasta y=154, justo antes del footer de y=156.
Ampliar los nodos horizontalmente.
Reservar un gutter exclusivo para pines y etiquetas.
Representar la negación con burbuja y prefijo ! explícito.
Mantener clipping completo y redibujado limpio.
```

La v0.4.1 no se considera cierre visual definitivo; sirve como validación positiva de la arquitectura del mapa y como base para v0.4.2.

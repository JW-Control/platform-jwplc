# Resultado físico — Runtime UI Diagrama v0.3.1

## Compilación

```text
Flash:       449229 bytes / 3145728 bytes (14 %)
RAM global:   32916 bytes / 327680 bytes (10 %)
RAM restante: 294764 bytes
```

## Resultado técnico

```text
Compilación: PASS
Carga del runtime: PASS
Programa RAM: PASS
RUN: PASS
Interfaz USER: PASS
Transición HOME -> DIAGRAMA: PASS
Vista de B00..B06: PASS
Valores en vivo: PASS
Detalle y lista: PASS
Q0 sin conmutación: PASS
FRAM sin escritura: PASS
```

## Resultado de experiencia de uso

```text
DIAGRAMA v0.3.1 COMO MAPA PRINCIPAL: NO APROBADO
```

La vista de contexto centrada en un bloque resultó visualmente atractiva y técnicamente correcta, pero no entregó la sensación de mapa continuo esperada para programar.

Problemas observados físicamente:

- `UP/DOWN` cambia por índice `B00..B06`, no por relación espacial;
- cada cambio reconstruye un contexto distinto alrededor del bloque central;
- las posiciones de los bloques cambian entre vistas;
- el usuario pierde memoria espacial del programa;
- las conexiones solo explican vecinos inmediatos;
- no se ve una red completa ni una porción estable de ella;
- la navegación se siente como páginas relacionadas por índices;
- selección y señal activa comparten demasiado verde y pueden confundirse;
- los textos internos ocupan más espacio que el propio mapa;
- los tres botones inferiores reducen el área útil del diagrama.

## Decisión

La implementación v0.3.1 se conserva como prototipo de renderer de nodos y señales, pero no se adopta como superficie de programación.

El siguiente diseño debe cumplir:

```text
mapa estable
+ posiciones fijas
+ viewport desplazable
+ navegación por conexiones y geometría
+ bloques compactos
+ detalle contextual separado
```

La tabla técnica y el detalle actual permanecen disponibles como herramientas secundarias.
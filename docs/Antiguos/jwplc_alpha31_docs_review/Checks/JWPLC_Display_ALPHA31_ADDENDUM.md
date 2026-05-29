# JWPLC_Display - Addendum alpha31

Archivo destino sugerido:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_Display/README.md
```

## Cambio sugerido

Agregar cerca del final, antes de `## Estado`, o reemplazar el bloque `## Estado` actual.

```md
## Validación alpha31

Para alpha31, `JWPLC_Display` se valida como parte del package completo instalado desde cero.

Pruebas recomendadas:

- autoarranque de pantalla `IDLE`;
- entrada a pantalla `USER`;
- retorno `USER -> IDLE` por timeout;
- retorno `USER -> IDLE` por `ESC`;
- modo `IDLE_RETURN_DISABLED`;
- indicadores `RUN`, `ERR`, `BUS` y `ETH`;
- ausencia de superposición de texto;
- ausencia de eventos pendientes de botonera al cambiar de pantalla;
- coexistencia SPI con Ethernet, SD y FRAM.

No se agregan features nuevas de display en alpha31.
```

## Bloque de estado sugerido

```md
## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_Display 1.0.0
```

Alpha31 valida el comportamiento actual del display dentro del package completo. No introduce cambios funcionales grandes en esta librería.
```

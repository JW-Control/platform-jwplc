# JWPLC_Ethernet - Addendum alpha31

Archivo destino sugerido:

```txt
JWPLC/JWPLC-2.0.0/libraries/JWPLC_Ethernet/README.md
```

## Cambio sugerido

Agregar cerca del final, antes de `## Estado`, o reemplazar el bloque `## Estado` actual.

```md
## Validación alpha31

Para alpha31, `JWPLC_Ethernet` se valida como parte del package completo instalado desde cero.

Pruebas recomendadas:

- arranque con RJ45 conectado;
- arranque sin RJ45 conectado;
- conexión posterior de RJ45;
- desconexión y reconexión sin falso error permanente;
- DHCP automático;
- IP estática configurada desde `setup()`;
- indicador `ETH` en pantalla IDLE;
- coexistencia SPI con TFT, microSD y FRAM;
- comportamiento `Ethernet disabled` esperado en `JWPLC Basic Core`.

No se agregan cambios funcionales de Ethernet en alpha31.
```

## Bloque de estado sugerido

```md
## Estado

Documentación revisada para:

```text
JWPLC ESP32 2.0.0-alpha.31
JWPLC_Ethernet 1.0.0
```

Alpha31 valida la integración Ethernet actual dentro del package completo. No cambia la arquitectura de inicialización automática.
```

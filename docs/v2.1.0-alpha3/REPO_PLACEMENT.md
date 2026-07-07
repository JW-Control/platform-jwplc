# Ubicación recomendada en `platform-jwplc` - v2.1.0-alpha.3 Remote I/O RTU

## Decisión de versión

La etapa correcta para esta importación es:

```txt
v2.1.0-alpha.3
```

No usar `alpha13`. Esa numeración venía del nombre/importación del ZIP original y no corresponde a la línea real del package actual.

## Rama recomendada

```txt
develop/v2.1.0-alpha.3-remote-io-rtu
```

Base recomendada:

```txt
main o la rama que contenga v2.1.0-alpha.2 ya mergeada
```

## Archivos que sí deberían versionarse

Colocar estos archivos en el repo:

```txt
docs/openplc/JWPLC_REMOTE_IO_RTU_PROTOCOL.md
docs/openplc/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md
docs/openplc/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md
docs/openplc/JWPLC_REMOTE_IO_RTU_HANDOFF_NEXT_CHAT.md
docs/releases/v2.1.0-alpha.3/JWPLC_V2_1_0_ALPHA3_REMOTE_IO_RTU_NOTES.md
```

## Archivos que no conviene versionar

No colocar en el repo:

```txt
original_import/
README de entrega temporal
MANIFEST temporal
PR_*.md
COMMIT_MESSAGE_*.md
ZIP de revisión
```

Estos son auxiliares de trabajo local o texto para pegar en GitHub.

## Archivos del package que todavía no deben tocarse solo por esta documentación

No modificar todavía:

```txt
JWPLC/package_jwplc_index.json
JWPLC/package_jwplc_index_dev.json
JWPLC/JWPLC-2.0.0/platform.txt
JWPLC/JWPLC-2.0.0/boards.txt
```

`package_jwplc_index_dev.json` se actualiza recién cuando exista el ZIP real `jwplc-esp32-2.1.0-alpha.3.zip`, con SHA-256 y tamaño final.

`package_jwplc_index.json` público debe seguir apuntando a `v2.0.0` hasta una estable nueva.

## README principal

Actualizar `README.md` solo si el PR documental quiere dejar visible el estado de desarrollo de `v2.1.0-alpha.3`.

Bloque sugerido:

```md
## Estado de desarrollo v2.1.0-alpha.3

`v2.1.0-alpha.3` corresponde a una pre-release técnica enfocada en la documentación y preparación de la PoC de JWPLC Remote I/O por Modbus RTU / RS-485.

Esta alpha no reemplaza a `v2.0.0` como canal estable público.

Alcance principal:

- definición del protocolo JWPLC Remote I/O RTU v1;
- mapa operativo Modbus RTU para `I0_0..I0_7` y `Q0_0..Q0_7`;
- plan de implementación por PoC;
- checklist de validación física de PoC 1;
- reglas para mantener OpenPLC/Remote I/O como integración opcional.

Queda fuera:

- integración obligatoria de OpenPLC en el runtime normal;
- cambios a `platform.txt`;
- OTA;
- cambio de FlashFreq;
- publicación de `bootloader.bin` definitivo;
- actualización del package dev hasta tener ZIP, SHA-256 y tamaño final.
```

## Orden recomendado de PR

1. PR documental con los archivos de `docs/openplc` y `docs/releases/v2.1.0-alpha.3`.
2. Después, PR de firmware PoC 1.
3. Recién con firmware probado, preparar pre-release `v2.1.0-alpha.3` y actualizar `package_jwplc_index_dev.json`.


# Handoff - JWPLC Basic v2.0.0 estable

## Estado

```txt
Alpha31 validada.
Beta1 publicada.
Siguiente objetivo: publicar v2.0.0 estable.
```

## Decisión de flujo

Aunque el flujo original contemplaba:

```txt
alpha31 -> beta1 -> rc1 -> v2.0.0
```

se acuerda simplificar este ciclo a:

```txt
alpha31 -> beta1 -> v2.0.0
```

RC no será obligatorio salvo que aparezca un bloqueante durante la instalación limpia final de `2.0.0`.

Para futuros ciclos, beta también será opcional si la última alpha de verificación ya fue validada como beta interna.

## Pendientes inmediatos

1. Generar `jwplc-esp32-2.0.0.zip`.
2. Publicar GitHub Release `v2.0.0`.
3. Calcular SHA-256 y size.
4. Actualizar `package_jwplc_index.json` público.
5. Crear/actualizar `package_jwplc_index_dev.json`.
6. Instalar desde cero con Boards Manager.
7. Validar Arduino IDE.
8. Validar Arduino CLI.
9. Confirmar release estable.

## No incluir

- OpenPLC real.
- OTA.
- FlashFreq 80 MHz.
- `bootloader.bin` definitivo.
- Precompilación de librerías.
- Rediseño multicore.
- Coredump formal.

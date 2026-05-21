# Changelog

## 1.0.2

- README actualizado

## 1.0.1

- Agrega `setEnabled(bool)` para permitir habilitar/deshabilitar la SD desde perfiles de hardware.
- Agrega `isEnabled()` para consultar el estado de habilitación desde el objeto `JW_SD`.
- Agrega estado de error `JW_SD_ERR_DISABLED`.
- Mejora la integración con ecosistemas donde la SD puede existir en una variante de hardware y estar deshabilitada en otra.

## 1.0.0

- Primera version publica de `JW_SD`.
- Agrega clase principal `JW_SD`.
- Agrega wrapper protegido `JWPLCFile`.
- Soporta callbacks `lock/unlock` para buses SPI compartidos.
- Soporta deteccion de tarjeta mediante pin opcional.
- Incluye helpers basicos: `exists`, `mkdir`, `remove`, `rmdir`, `rename`.
- Incluye `openNative()` para acceso avanzado a `File` nativo.
- Incluye ejemplos `BasicReadWrite` y `CardInfo`.

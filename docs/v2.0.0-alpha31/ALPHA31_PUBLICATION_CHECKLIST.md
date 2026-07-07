# Alpha31 - Checklist de publicación

Este checklist acompaña la publicación de:

```txt
JWPLC Basic v2.0.0-alpha.31
```

---

## 1. Antes de generar ZIP

- [ ] Confirmar rama activa:

```txt
develop/alpha31-release-readiness
```

- [ ] Confirmar que el snapshot de librerías JW está integrado:

| Librería | Versión |
|---|---:|
| `JW_MatrixButtons` | `1.0.4` |
| `JW_FRAM` | `1.0.2` |
| `JW_RTC` | `1.0.2` |
| `JW_SD` | `1.0.2` |

- [ ] Confirmar que no hubo cambios no planificados en:

```txt
boards.txt
platform.txt
cores/
variants/
```

- [ ] Confirmar compilación local preliminar OK.
- [ ] Confirmar que el máximo de programa sigue siendo:

```txt
3145728 bytes
```

---

## 2. Generar ZIP alpha31

Nombre esperado:

```txt
jwplc-esp32-2.0.0-alpha.31.zip
```

Validar que el ZIP contiene:

```txt
boards.txt
platform.txt
cores/
variants/
libraries/
tools/
```

y dentro de `libraries/`:

```txt
JW_MatrixButtons/
JW_FRAM/
JW_RTC/
JW_SD/
```

---

## 3. Publicar GitHub Pre-Release

Release:

```txt
v2.0.0-alpha.31
```

Título:

```txt
JWPLC Basic v2.0.0-alpha.31 - Release readiness y snapshot de librerías JW
```

Asset:

```txt
jwplc-esp32-2.0.0-alpha.31.zip
```

Marcar como:

```txt
Pre-release
```

---

## 4. Calcular checksum y size

PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-alpha.31.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0-alpha.31.zip).Length
```

Registrar:

```txt
SHA-256:
Size:
```

---

## 5. Actualizar package_jwplc_index.json

Actualizar entrada:

```json
{
  "version": "2.0.0-alpha.31",
  "archiveFileName": "jwplc-esp32-2.0.0-alpha.31.zip",
  "checksum": "SHA-256:<PENDIENTE>",
  "size": "<PENDIENTE>"
}
```

Validar que la URL descargue correctamente el ZIP.

---

## 6. Instalación limpia desde Boards Manager

Después de publicar y actualizar index:

1. Cerrar Arduino IDE.
2. Borrar o renombrar:

```txt
C:\Users\<usuario>\AppData\Local\Arduino15\packages\jwplc
```

3. Abrir Arduino IDE.
4. Buscar:

```txt
JW Control ESP32 Boards
```

5. Instalar:

```txt
2.0.0-alpha.31
```

6. Confirmar que existe:

```txt
C:\Users\<usuario>\AppData\Local\Arduino15\packages\jwplc\hardware\esp32\2.0.0-alpha.31
```

7. Confirmar librerías:

```txt
JW_MatrixButtons 1.0.4
JW_FRAM 1.0.2
JW_RTC 1.0.2
JW_SD 1.0.2
```

---

## 7. Validación mínima posterior

- [X] Compilar sketch vacío.
- [X] Compilar sketch Serial mínimo.
- [X] Subir sketch Serial mínimo.
- [X] Confirmar monitor serial.
- [X] Compilar `DigitalIO_BlockRead`.
- [X] Compilar `DigitalIO_BlockMirror`.
- [X] Compilar `RS485_USB_Bridge`.
- [X] Compilar `ModbusRTU_CRC_Test`.
- [X] Validar periféricos principales si hay hardware disponible.

---

## 8. Decisión posterior

Si instalación limpia y compilación pasan:

```txt
Alpha31 queda apta para cerrar y pasar a beta1-package-validation.
```

Si aparece bloqueante:

```txt
Corregir en alpha31 antes de beta1.
```

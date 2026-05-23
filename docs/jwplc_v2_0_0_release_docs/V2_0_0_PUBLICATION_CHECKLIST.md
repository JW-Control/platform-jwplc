# v2.0.0 - Checklist de publicación estable

Versión:

```txt
JWPLC Basic v2.0.0
```

Branch sugerido:

```txt
develop/beta1-package-validation
```

Tag:

```txt
v2.0.0
```

---

## 1. Decisión de flujo

- [x] `alpha31` validó instalación limpia y periféricos principales.
- [x] `beta1` fue publicada como package validation.
- [x] Se acuerda pasar a `v2.0.0` estable.
- [x] RC no será obligatorio en este ciclo.
- [x] Para futuros ciclos, beta/RC quedan como etapas opcionales si la última alpha de verificación fue suficientemente validada.

---

## 2. Antes del ZIP

- [ ] Confirmar branch activo.
- [ ] Hacer `Fetch origin` / `Pull origin`.
- [ ] Confirmar README final.
- [ ] Confirmar docs de release.
- [ ] Confirmar que no hay cambios funcionales grandes pendientes.
- [ ] Confirmar `boards.txt`.
- [ ] Confirmar `platform.txt`.
- [ ] Confirmar librerías bundled.
- [ ] Confirmar que no se arrastra ZIP beta dentro del repo si no corresponde.

---

## 3. ZIP estable

Archivo esperado:

```txt
jwplc-esp32-2.0.0.zip
```

Estructura esperada:

```txt
jwplc/
└─ hardware/
   └─ esp32/
      └─ 2.0.0/
         ├─ boards.txt
         ├─ platform.txt
         ├─ cores/
         ├─ variants/
         ├─ libraries/
         └─ tools/
```

Validar que incluya:

```txt
JW_MatrixButtons
JW_FRAM
JW_RTC
JW_SD
JWPLC_Display
JWPLC_GlobalPeripherals
JWPLC_Ethernet
JWPLC_RS485
JWPLC_ModbusRTU
```

---

## 4. SHA-256 y size

PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0.zip).Length
```

Registrar:

```txt
SHA-256:
Size:
```

---

## 5. GitHub Release

Crear release:

```txt
v2.0.0
```

Título:

```txt
JWPLC Basic v2.0.0 - Release estable inicial
```

Asset:

```txt
jwplc-esp32-2.0.0.zip
```

No marcar como Pre-release.

---

## 6. Package index público

Archivo:

```txt
JWPLC/package_jwplc_index.json
```

Debe apuntar a:

```txt
2.0.0
```

Recomendación:

```txt
No incluir alphas en el canal público.
```

Puede incluir beta1 si se quiere dejar disponible, pero para usuario final se recomienda mostrar principalmente estable.

---

## 7. Package index dev

Archivo:

```txt
JWPLC/package_jwplc_index_dev.json
```

Debe incluir:

```txt
alphas + beta1 + 2.0.0
```

Uso:

```txt
Validación interna.
```

---

## 8. Instalación limpia final

Después de actualizar index:

- [ ] Cerrar Arduino IDE.
- [ ] Borrar o renombrar `Arduino15/packages/jwplc`.
- [ ] Abrir Arduino IDE.
- [ ] Instalar `JW Control ESP32 Boards 2.0.0`.
- [ ] Confirmar carpeta `2.0.0`.
- [ ] Confirmar librerías bundled.
- [ ] Compilar sketch vacío.
- [ ] Compilar/subir sketch Serial mínimo.
- [ ] Confirmar monitor serial.
- [ ] Ejecutar Arduino CLI.

---

## 9. Validación mínima final

- [ ] `ESP32 Board` compila.
- [ ] `JWPLC Basic` compila.
- [ ] `JWPLC Basic Core` compila.
- [ ] `JWPLC Basic` sube por USB.
- [ ] `Maximum is 3145728 bytes`.
- [ ] `DigitalIO_BlockRead` compila.
- [ ] `DigitalIO_BlockMirror` compila.
- [ ] `RS485_USB_Bridge` compila.
- [ ] `ModbusRTU_CRC_Test` compila.
- [ ] `Ethernet_SPI_Coexistence` compila.

---

## 10. Decisión

```txt
[ ] v2.0.0 aprobada como release estable.
[ ] v2.0.0 requiere corrección antes de publicación estable.
```

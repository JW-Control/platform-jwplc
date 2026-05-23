# Beta1 - Checklist de publicación

Versión:

```txt
JWPLC Basic v2.0.0-beta.1
```

Branch:

```txt
develop/beta1-package-validation
```

---

## 1. Antes del ZIP

- [ ] Confirmar branch activo.
- [ ] Hacer `Fetch origin` / `Pull origin`.
- [ ] Confirmar README actualizado a beta1.
- [ ] Confirmar docs beta1 agregados.
- [ ] Confirmar Arduino CLI post-publicación OK.
- [ ] Confirmar que no hay cambios funcionales grandes.
- [ ] Confirmar que `boards.txt` mantiene configuración fija.
- [ ] Confirmar que `platform.txt` no se modificó salvo bloqueante real.

---

## 2. ZIP beta1

Archivo esperado:

```txt
jwplc-esp32-2.0.0-beta.1.zip
```

Debe contener estructura:

```txt
jwplc/
└─ hardware/
   └─ esp32/
      └─ 2.0.0-beta.1/
         ├─ boards.txt
         ├─ platform.txt
         ├─ cores/
         ├─ variants/
         ├─ libraries/
         └─ tools/
```

Validar que `libraries/` incluya:

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

## 3. GitHub Pre-Release

Tag:

```txt
v2.0.0-beta.1
```

Título:

```txt
JWPLC Basic v2.0.0-beta.1 - Package validation
```

Asset:

```txt
jwplc-esp32-2.0.0-beta.1.zip
```

Marcar como:

```txt
Pre-release
```

---

## 4. SHA-256 y size

PowerShell:

```powershell
certutil -hashfile jwplc-esp32-2.0.0-beta.1.zip SHA256
(Get-Item .\jwplc-esp32-2.0.0-beta.1.zip).Length
```

Registrar:

```txt
SHA-256:
Size:
```

---

## 5. Package indexes

### Canal público

Archivo:

```txt
JWPLC/package_jwplc_index.json
```

Debe incluir:

```txt
2.0.0-beta.1
```

y no arrastrar todas las alphas.

### Canal dev

Archivo:

```txt
JWPLC/package_jwplc_index_dev.json
```

Debe incluir:

```txt
alphas + beta1
```

---

## 6. Instalación limpia

Después de actualizar index:

- [ ] Cerrar Arduino IDE.
- [ ] Borrar o renombrar `Arduino15/packages/jwplc`.
- [ ] Abrir Arduino IDE.
- [ ] Instalar `JW Control ESP32 Boards 2.0.0-beta.1`.
- [ ] Confirmar carpeta `2.0.0-beta.1`.
- [ ] Confirmar librerías bundled.
- [ ] Compilar sketch vacío.
- [ ] Compilar/subir sketch Serial mínimo.
- [ ] Confirmar monitor serial.
- [ ] Ejecutar validación CLI.

---

## 7. Decisión

```txt
[ ] Beta1 aprobada para avanzar a RC1.
[ ] Beta1 requiere beta2.
```

# Alpha31 - Archivos generados para revisión

Este paquete contiene propuestas documentales para revisar y copiar manualmente al repositorio correspondiente.

No se modifica código fuente ni configuración técnica del package.

---

## Archivos generados

| Archivo generado | Tipo | Destino sugerido |
|---|---|---|
| `README_PLATFORM_JWPLC_ALPHA31_PROPUESTO.md` | README completo propuesto | `JW-Control/platform-jwplc` → `README.md` |
| `JWPLC_Display_ALPHA31_ADDENDUM.md` | Bloque para insertar/reemplazar | `JWPLC/JWPLC-2.0.0/libraries/JWPLC_Display/README.md` |
| `JWPLC_Ethernet_ALPHA31_ADDENDUM.md` | Bloque para insertar/reemplazar | `JWPLC/JWPLC-2.0.0/libraries/JWPLC_Ethernet/README.md` |
| `JWPLC_RS485_ALPHA31_ADDENDUM.md` | Bloque para insertar/reemplazar | `JWPLC/JWPLC-2.0.0/libraries/JWPLC_RS485/README.md` |
| `JWPLC_ModbusRTU_ALPHA31_ADDENDUM.md` | Bloque para insertar/reemplazar | `JWPLC/JWPLC-2.0.0/libraries/JWPLC_ModbusRTU/README.md` |
| `JW_FRAM_LICENSE_FIX.md` | Corrección puntual | `JW-Control/JW_FRAM` → `README.md` |
| `JW_SD_README_PROPUESTO.md` | README completo propuesto | `JW-Control/JW_SD` → `README.md` |
| `JW_RTC_README_ES_PROPUESTO.md` | README español propuesto | `JW-Control/JW_RTC` → `README_ES.md` o reemplazo de `README.md` si se desea español completo |
| `JW_MatrixButtons_ALPHA31_ADDENDUM.md` | Bloque opcional | `JW-Control/JW_MatrixButtons` → `README.md` |
| `JW_DWIN_RS485_ALPHA31_ADDENDUM.md` | Bloque opcional | `JW-Control/JW_DWIN_RS485` → `README.md` |

---

## Recomendación de aplicación

### 1. Primero aplicar en `platform-jwplc`

Orden recomendado:

1. Revisar `README_PLATFORM_JWPLC_ALPHA31_PROPUESTO.md`.
2. Si está conforme, reemplazar el `README.md` principal.
3. Revisar addendums internos:
   - `JWPLC_Display_ALPHA31_ADDENDUM.md`
   - `JWPLC_Ethernet_ALPHA31_ADDENDUM.md`
   - `JWPLC_RS485_ALPHA31_ADDENDUM.md`
   - `JWPLC_ModbusRTU_ALPHA31_ADDENDUM.md`
4. Insertar los bloques sugeridos en los READMEs internos.

Commit sugerido:

```txt
docs: update alpha31 package documentation
```

---

### 2. Luego aplicar en repositorios externos

Orden recomendado:

1. `JW_FRAM_LICENSE_FIX.md`: corrección puntual de cita residual.
2. `JW_SD_README_PROPUESTO.md`: reemplazo completo del README actual.
3. `JW_RTC_README_ES_PROPUESTO.md`: agregar como `README_ES.md` o usar como base para traducir `README.md`.
4. `JW_MatrixButtons_ALPHA31_ADDENDUM.md`: opcional.
5. `JW_DWIN_RS485_ALPHA31_ADDENDUM.md`: opcional.

Commit sugerido para cada repo:

```txt
docs: improve README for JWPLC alpha31 readiness
```

---

## Criterio alpha31

Estos documentos no agregan features nuevas.

Solo buscan:

- aclarar estado actual;
- reforzar que OpenPLC no está integrado todavía;
- reforzar que OTA no está definido;
- mantener la decisión de no publicar `bootloader.bin`;
- documentar validaciones de instalación limpia;
- preparar documentación para beta1.

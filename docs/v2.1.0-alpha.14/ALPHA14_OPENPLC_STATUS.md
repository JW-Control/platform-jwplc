# Estado · alpha OpenPLC + HMI (trabajo ALPHA14 · publica 2.1.0-alpha.15)

Estado vivo de la línea OpenPLC ↔ HMI Designer. No confundir con `ALPHA14_STATUS.md` (alpha.14 de robustez
Ethernet, cerrada en 2026-09). Traspaso vigente: `JWPLC_NEW_CHAT_START_ALPHA14_OPENPLC_20261024.md`.
Hoja de ruta (hecho, pruebas y lo que falta): `A14_HOJA_DE_RUTA_ALPHA15_OPENPLC_HMI_20261024.md`.

```text
RAMA_PRINCIPAL=v2.1.0-alpha.15/OpenPLC-HMI/main (platform-jwplc y openplc-editor)
BASE_PLATFORM=release/v2.1.x @ f3f47ae (2.1.0-alpha.14 publicada)
BASE_EDITOR=develop/alpha14-openplc-hmi-v1 @ c0bf9ac07 + 4.2.8-jwplc.3
VPP_VIGENTE=2.1.0-alpha.32 en …/main; 2.1.0-alpha.15 en feature/vpp-version-alpha15 (falta firmar)
F0_BOOL=PASS_PHYSICAL
F0b_VALORES=PASS_PHYSICAL
F0c_ESCRITURA=PASS_PHYSICAL
F0d_COSTO_HMI=PASS_PHYSICAL (alpha.32)
F0d_JITTER_TFT=PENDING (package: redibujo de página en jwplcSystemTask)
MERGE_HMI_DESIGNER_2_1_0_ALPHA14=PASS [ejecutado] (24/10, tests del Designer + modo OpenPLC en Chromium)
F3_PUENTE_AMPLIADO_CONTRATO_1_1=PASS [ejecutado] (feature/designer-alpha14-en-openplc, sin fusionar)
F3_VARIABLES_SIN_DIRECCION_1_2=PASS [ejecutado] (tests + compilación ESP32; sin fusionar)
VPP_2_1_0_ALPHA_15=COMPILA PASS [ejecutado]; FIRMA PENDING
F3_CADENA_COMPLETA_EN_APP_Y_PLACA=PENDING [no verificado]
CIERRE=PENDING
```

## Registro de merges en `…/main`

| Fecha | Repo | Qué entró | SHA en `…/main` | Documento |
|---|---|---|---|---|
| 24/10 | platform-jwplc | historia de `v2.1.0-alpha.14/HmiDesigner/main` (árbol = release) | `83abf3c` | `A14_APERTURA_RAMA_ALPHA15_OPENPLC_HMI_20261024.md` |
| 24/10 | platform-jwplc | `v2.1.0-alpha.14-OpenPLC-HMI` @ `20ef597` (2 conflictos resueltos) | `7476cd6` | ídem |
| 24/10 | openplc-editor | `claude/awesome-goldberg-7sj2vx` @ `df6b74d6c` (4.2.8-jwplc.3) sobre `develop/alpha14-openplc-hmi-v1` | `adde17d74` | ídem |

## Pendiente (orden del traspaso §3)

1. ~~Traer el HMI Designer de 2.1.0-alpha.14 a la línea~~ (hecho, 24/10).
2. ~~Puente ampliado del anfitrión OpenPLC (contrato 1.1)~~ y variables sin dirección (1.2): hechos en
   `v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc` (los dos repos), sin fusionar.
3. Firma del VPP 2.1.0-alpha.15 y cadena completa F3 en la app y en la placa (guía `A14_F3_1`).
4. Revisión del contrato y de `designer-openplc-host.js` con el jefe y André (A15-6).
5. Designer dentro del VPP firmado, pestaña en vez de ventana (F4), variables sin dirección, `EDIT_BOOL` RW, TEXT.
6. Package: retraso del scan por el redibujo de página de la TFT.

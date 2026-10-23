# Gate A15-1..A15-3 — Codegen, páginas y estática fuera del tick

Prueba local (PC con Windows + JWPLC Basic) del trabajo de alpha.15 hasta el
2026-10-17. Documento del gate: `docs/v2.1.0-alpha.15/A15_1_3_CODEGEN_PAGES_STATIC_20261017.md`.

```text
A15_GATE_LOCAL_DESIGNER=PENDING
A15_GATE_ARDUINO_CLI_COMPILE=PASS_CLOUD (2026-10-17: 444649 B flash / 34620 B RAM con JWPLC_TFT 0.2.0;
                                         443973 B con el package publicado 0.1.0)
A15_GATE_PHYSICAL_TFT=PENDING
```

Contenido:

```text
A15_1_3_Codegen_Static/
├─ A15_1_3_Codegen_Static.ino      sketch de prueba (anima los valores)
├─ JWPLC_HMI_Generated.h           header generado por el Designer de esta rama
└─ A15_1_3_Codegen_Static.jwhmi    proyecto, para abrirlo en el Designer
designer_p0.png .. designer_p2.png  cómo se ve cada página en el Designer
```

## 0. Traer la rama

```powershell
git fetch origin v2.1.0-alpha.15-HmiDesigner
git switch v2.1.0-alpha.15-HmiDesigner
```

## 1. Designer desde el repo (no la app instalada)

La app instalada (Electron) sigue siendo Alpha11. Para ver esta rama:

```powershell
powershell -ExecutionPolicy Bypass -File tools\jwplc-hmi-designer\Start-JWPLC-HMI-Designer.ps1
```

Abre Edge/Chrome en `http://127.0.0.1:8765/desktop.html`. Si ves la versión vieja,
recarga con **Ctrl+F5** (el service worker cambió de caché).

Comprobar:

| # | Acción | Esperado |
|---|---|---|
| D1 | Abrir `A15_1_3_Codegen_Static.jwhmi` | 3 páginas; cada página igual a `designer_pN.png` |
| D2 | Seleccionar un BAR → Forma → Tipo | Tarjeta **Circular** deshabilitada ("Requiere firmware"); "Degradado" deshabilitado |
| D3 | Seleccionar el texto RAW → fuente | Sólo GLCD seleccionable; el resto "(requiere firmware)" |
| D4 | Borrar la página "Motor", Guardar, Abrir de nuevo | Abre sin error; quedan "Principal" y "Nivel" |
| D5 | Generar C++ | Ninguna línea con `getTFT`; figuras dentro de `jwplcUIPageEnter()`; `jwplcUIUpdate()` sólo con `setText/setValue/setBool/setBar` |

## 2. Compilar con arduino-cli (o Arduino IDE)

Con el package publicado/instalado:

```powershell
arduino-cli compile --fqbn jwplc:esp32:jwplcbasic `
  tools\jwplc-hmi-designer\gates\A15_1_3_Codegen_Static\A15_1_3_Codegen_Static
```

Con el namespace local de desarrollo usar `--fqbn jwplc_local:esp32:jwplcbasic`.
En Arduino IDE: abrir el `.ino` y Verificar.

Esperado: compila sin errores. Anotar flash/RAM que reporta el compilador.

## 3. Subir y mirar la TFT

```powershell
arduino-cli upload --fqbn jwplc:esp32:jwplcbasic -p COMx `
  tools\jwplc-hmi-designer\gates\A15_1_3_Codegen_Static\A15_1_3_Codegen_Static
arduino-cli monitor -p COMx -c baudrate=115200
```

| # | Comprobación | Esperado |
|---|---|---|
| T1 | Página 1 | Cabecera azul con "JWPLC HMI A15", elipse verde, Temp oscilando, Estado READY/RUN/WAIT/ALARM |
| T2 | LEFT/RIGHT | Cambia de página; barra inferior PRNC/MTR/NVL resalta la activa |
| T3 | Página 2 | Hexágono cian, triángulo naranja, línea gris, Motor PARADO/MARCHA cada 2 s |
| T4 | Página 3, OK y luego UP/DOWN | Barra Nivel cambia de 5 en 5; ESC vuelve al selector |
| T5 | Serial | Una línea `[A15-GATE] pageEnter` **sólo** al entrar a una página, no cada tick |
| T6 | Mientras cambian los valores | Las figuras y la barra inferior **no parpadean** |
| T7 | Ir a IDLE y volver a USER | La barra de navegación y las figuras se redibujan completas |
| T8 | Comparar con `designer_pN.png` | Misma posición y forma (anotar diferencias en elipse/triángulo) |

Si algo falla, una foto de la TFT junto al PNG correspondiente y la salida Serial
alcanzan para diagnosticar.

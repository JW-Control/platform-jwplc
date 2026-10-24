# F3.1 · El HMI Designer de 2.1.0-alpha.14 dentro de OpenPLC (24/10/2026)

Rama (los dos repos): `v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc`.
Contrato: `A14_F3_CONTRATO_HMI_OPENPLC_20261017.md` §7 (`jwplc-hmi-host/1.1`).

```text
A14_F3_1_PUENTE_1_1=PASS [ejecutado] (simulado: openplc-host-smoke.cjs; editor: tests + tsc + eslint)
A14_F3_1_EN_LA_APP=PENDING [no verificado] (sección 3)
A14_F3_1_EN_PLACA=PENDING [no verificado] (sección 3, pasos 6-7)
```

## 1. Por qué OpenPLC mostraba el HMI antiguo

OpenPLC no trae su propio Designer: carga los archivos de `JWPLC_HMI_DESIGNER_DIR` o del checkout vecino
`..\platform-jwplc\tools\jwplc-hmi-designer\poc`. En la rama de Kenyi (`v2.1.0-alpha.14-OpenPLC-HMI`) ese
Designer es la versión anterior («Alpha11» en el título), anterior a lo publicado en 2.1.0-alpha.14. El Designer de
2.1.0-alpha.14 está en `v2.1.0-alpha.15/OpenPLC-HMI/main`, pero usa unas 20 funciones del puente y OpenPLC sólo
daba 4: por eso esta feature amplía el puente (contrato 1.1) y adapta el Designer al modo OpenPLC.

Desde 1.1, si el checkout vecino tiene un Designer viejo, OpenPLC no lo abre: avisa que hay que cambiar
platform-jwplc a `v2.1.0-alpha.15/OpenPLC-HMI/main`.

## 2. Pruebas que hizo Kenyi (revisadas)

| Gate | VPP | Resultado | Qué queda |
|---|---|---|---|
| F0 BOOL ↔ relé `Q0_0` | alpha.30 | PASS en placa (también con forzado) | — |
| F0b VALUE/BAR ← `%MW0` | alpha.30 | PASS en placa | — |
| F0c MENU → `%MW1` (escritura) | alpha.30 | PASS en placa: OK escribe, UP/DOWN ±1, LEFT/RIGHT ±10 | — |
| F0d costo de la HMI en el scan | alpha.31 → alpha.32 | alpha.31 ≈ 167 µs (FAIL); alpha.32 5–15 µs prom., ≤ 27 µs (PASS) | cambio de página 53–65 ms (package) |
| F3 Designer en OpenPLC | alpha.32 | primera versión OK en la app; las 2 fallas (selector y guardado) corregidas sin volver a probar | con el Designer viejo |

Detalle: `tools/jwplc-hmi-designer/gates/A14_F0b_*`, `A14_F0c_*`, `A14_F0d_*` e informe `A14_HMI_V1_OPENPLC_20261017.md`.
Los F0 prueban el firmware (HAL del VPP + header del Designer) y siguen valiendo; F3 se repite con el Designer
nuevo (sección 3). El VPP `2.1.0-alpha.15` es el mismo payload que alpha.32 (`A14_VPP_VERSION_ALPHA15_20261024.md`).

## 3. Prueba en la PC (Windows, Node 22)

1. **Ramas.** En `C:\Users\jeykc\Documents\GitHub`:

   ```bat
   cd platform-jwplc
   git fetch origin
   git checkout v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc
   cd ..\openplc-editor
   git fetch origin
   git checkout v2.1.0-alpha.15/OpenPLC-HMI/feature/designer-alpha14-en-openplc
   npm install
   npm run setup:strucpp
   npm run build:dll
   npm run dev
   ```

   Los dos repos tienen que estar uno al lado del otro (`GitHub\platform-jwplc` y `GitHub\openplc-editor`).
2. **VPP.** Instalar el `.vpp` firmado (2.1.0-alpha.15, o alpha.32 mientras tanto: mismo payload).
3. **Abrir.** Proyecto de prueba (p. ej. `HMIProbe` en `C:\Users\jeykc\Documents\openPLC`), guardarlo,
   configuración del dispositivo → **Abrir HMI Designer**. Debe verse:
   - el distintivo `2.1.0-alpha.14` junto al título;
   - el botón «OpenPLC: <proyecto> ▾» (menú: «Recargar HMI guardado», «Abrir carpeta hmi/»);
   - «Actualizar HMI»;
   - si el proyecto ya tenía `hmi/JWPLC_HMI.jwhmi`, esa pantalla cargada.
4. **Enlazar.** En la pestaña Datos de cada objeto, «Variable PLC»: Relé → `RELE`, Contador y Nivel → `CONTADOR`,
   nodo del MENU Límite → `LIMITE` (como F0c).
5. **Actualizar HMI.** Escribe `hmi/JWPLC_HMI_Generated.h` y `hmi/JWPLC_HMI.jwhmi`, sin `JWPLC_HMI_LEEME.md`.
   Si el proyecto tiene un `hmi/bindings.json` de F0, borrarlo (los nombres C++ cambian, p. ej. `rele_q0_0` →
   `rele`), igual que el `.jwhmi` viejo `A14_F0c_OpenPLC_Escritura.jwhmi`.
6. **Compilar y subir** desde OpenPLC: la placa debe comportarse como en F0c.
7. **Cerrar y reabrir.**
   - Cerrar el Designer con un cambio sin guardar: debe preguntar (Guardar / No guardar / Cancelar).
   - Reabrirlo: muestra la última pantalla guardada.
8. **(Opcional) Live.**
   - «Conectar JWPLC» abre el selector de puertos del Designer. Usar un COM que no tenga tomado el depurador de
     OpenPLC.
   - El flasheo del Live Bridge reemplaza el programa del PLC: después hay que volver a subir desde OpenPLC.

Anotar el resultado en los flags del principio (`PASS_PHYSICAL` / `FAIL` con lo observado).

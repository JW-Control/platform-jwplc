# A17 · Limpieza y numeración de ejemplos (02/11/2026)

Rama: `v2.1.0-alpha.17/JWPLC-Sim/feature/limpieza-ejemplos` (PR hacia `v2.1.0-alpha.17/JWPLC-Sim/main`).
Base de la propuesta: `/mnt/project-files/sim-alpha17/EJEMPLOS_QUE_QUEDAN.md` y
`A17_ANALISIS_API_EJEMPLOS_20261102.md`. **Cada grupo de borrados espera el OK del propietario.**

```text
A17_LIMPIEZA_EJEMPLOS=IN_PROGRESS   (OK del propietario 02/11; falta fusionar jwplc-sim #16 y subir el pin)
```

## 1. Renombres (se conserva el código; cambia carpeta, `.ino` y la línea 2 del encabezado)

| Librería | Antes | Después | Motivo |
|---|---|---|---|
| GlobalPeripherals | `06.Runtime_Cache_Status` | `06.IO_Estado_Escaneo` | Es la caché de E/S; «Runtime» se confunde con el Runtime de panel. |
| Display | `Display_Fuentes_Grandes` | `10.Display_Fuentes_Grandes` | Numerar los avanzados detrás de 01–09. |
| Display | `Display_Graficos_Sprite` | `11.Display_Graficos_Sprite` | Ídem. |
| Display | `Display_Efficient_Redraw` | `12.Display_Efficient_Redraw` | Ídem. |
| Display | `Display_Idle_Return_Modes` | `13.Display_Idle_Return_Modes` | Ídem. |
| Display | `Display_UserUI_Callbacks` | `14.Display_UserUI_Callbacks` | Ídem. |
| RS485 | `RS485_USB_Bridge` | `04.RS485_Puente_USB` | Único puente USB↔RS-485 que queda. |
| Display | `Display_FlappyBird` | `15.Display_FlappyBird` | Demo que se mantiene (propietario, 02/11). |
| Display | `Display_Tetris` | `16.Display_Tetris` | Ídem. |

## 2. Ejemplos borrados (28)

| Grupo | Ejemplos | Motivo |
|---|---|---|
| GlobalPeripherals (4) | `DigitalIO_BlockMirror`, `DigitalIO_BlockRead`, `DigitalIO_BlockWrite`, `JWPLC_IO_BlockMirror` | Variantes de lo mismo; `01.DigitalIO_Basic` ya usa `JWPLC_IO.inputs()` / `setOutputs()` (PR de API con «.»). |
| Display (1) | `Display_DotAPI_Minimal` | Lo cubren 01–04 con la API con «.». |
| Ethernet (6) | `Ethernet_Auto_DHCP_Status`, `Ethernet_Auto_StaticIP_Status`, `Ethernet_Display_Status` | Repiten 01–03 con pantalla. |
| | `Ethernet_Continuous_Stress_TFT`, `Ethernet_SPI_Coexistence`, `Ethernet_HTTP_TFT_Diagnostics` | Pruebas de validación de alphas viejas (A14), no ejemplos de usuario. |
| RS485 (5) | `RS485_Basic_Send`, `RS485_Basic_Echo`, `RS485_Status` | Duplican 01–03. |
| | `RS485_Bridge` | Lo cubre `04.RS485_Puente_USB`. |
| | `RS485_Native` | Prueba interna del backend nativo. |
| ModbusRTU (12) | `ModbusRTU_Master_ReadHoldingRegisters`, `ModbusRTU_Master_WriteSingleRegister`, `ModbusRTU_Slave_HoldingRegisters` | Duplican 01–03. |
| | `ModbusRTU_CRC_Test`, `ModbusRTU_Tool_Test`, `ModbusRTU_Master_Cooperative_Validation` | Pruebas. |
| | `ModbusRTU_Native`, `ModbusRTU_Master_Native`, `ModbusRTU_Master_Extern`, `ModbusRTU_Slave_Extern` | Variantes internas de backend. |
| | `JWPLC_RemoteIO_Slave_RTU`, `JWPLC_RemoteIO_Master_RTU_Test` | E/S remota entre dos JWPLC; la reemplaza `JWPLC_Expansion` (`Expansion_Esclavo_Basic`). |

Los borrados quedan en el historial de git (último SHA con ellos: `d007b44`).

## 3. Librerías borradas (propietario, 02/11: «el runtime anterior elimínalo»)

- `JWPLC_LogicRuntime` (27 ejemplos) y `JWPLC_LogicRuntime_UI` (6): motor v1/v2 anterior. El runtime vigente es
  `JWPLC_Runtime`. Ningún otro código del package las incluye `[visto]`; las referencias en `tools/alpha12` y
  `tools/alpha13` son gates históricos y no se tocan. `CLAUDE.md` §1 actualizado.
- En jwplc-sim (PR #16), `gate-g4-nvs`, `e2e-g4` y `diag-windows-crash` solo usan esos ejemplos si el package
  fijado los trae; `sim/package-parity.json` se regenera con `parity --update` al subir el pin.

## 4. Efectos

- **CI del package** (`ci-jwplc-package-smoke.yml`): la lista de compilación ya no nombra los borrados, usa
  los nombres nuevos y añade `01.ModbusRTU_Slave_Holding` y `04.RS485_Puente_USB`. `[visto]`
- **JWPLC Sim**: la matriz `SIM_REGRESSION` recorre los ejemplos del package y falla si uno no tiene
  `.jwtest.json`. Los renombrados necesitan su test en `jwplc-sim` (copias con la ruta nueva; las viejas se
  quedan para alpha.16) y `scripts/gate-g1-examples-io.mjs` deja de nombrar los ejemplos de bloques. Después
  se sube el pin de `jwplc-sim` en los dos workflows (`uses`, `sim_ref` y `JWSIM_REF`). Hasta entonces este
  PR **no puede pasar** `SIM_REGRESSION`. `[inferido]`
- READMEs actualizados: `examples/README.md` de GlobalPeripherals, Display, RS485, ModbusRTU y Ethernet;
  `JWPLC_Ethernet/README.md`; `JWPLC_TFT/README.md`. `[visto]`

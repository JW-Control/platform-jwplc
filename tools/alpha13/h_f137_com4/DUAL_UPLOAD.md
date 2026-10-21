# Alpha13 F137 — Subida dual COM14 Slave y COM4 Master

**Autorización del propietario (21/10/2026):** ambas placas JWPLC de prueba conectadas por RS-485, COM4 Master y COM14 Slave. Se autoriza subir los **dos firmwares específicos** juntos sin respaldo, conforme a la excepción de placas de desarrollo del 17/10/2026. No aplicar a equipos instalados/de clientes.

## Preparar conexiones antes de la prueba

- Router/switch de banco con **DHCP habilitado** (o servidor DHCP en esa LAN); conectar ambos RJ45 y el PC a la **misma red local**, sin salida a Internet necesaria.
- Mantener el RS-485 del banco entre placas, A↔A, B↔B. No confundir esta conexión con los cables de Ethernet; cada PLC necesita su RJ45 para probar Ethernet/Modbus TCP en ambos.
- COM4=Master con microSD; COM14=Slave **sin microSD**. Salidas Q sin cargas/tensión, alimentación adecuada, monitores serial cerrados antes de flashear.
- El inventario COM4 y COM14 debe reconocer **dos CH340 distintos** en Windows. La enumeración USB no prueba por sí sola el rol: confirmar físicamente antes de ejecutar.
- No se ejecutan `erase-all`, backup, restore, manipulación de GPIO4 ni operaciones sobre Q. Upload USB puede reiniciar ambos PLC.

## 1. Usar el build Master existente, compilar SOLO Slave

Desde una sesión PowerShell en `platform-jwplc`, conservar el directorio del build **COM4 sellado previamente**, **no limpiar ni eliminar**:

```powershell
git status --short
git switch codex/alpha13-precompile-benchmark-docs-20261020
git pull --ff-only
$head = (git rev-parse HEAD).Trim()
$out = 'C:\Users\jeykc\AppData\Local\Temp\jwplc_f137_com4_20261021_092404'

py -3 tools/alpha13/h_f137_com4.py --inventory --expected-head $head
py -3 tools/alpha13/h_f137_dual.py --selftest
py -3 tools/alpha13/h_f137_dual.py --prepare-slave --expected-head $head --output $out
py -3 tools/alpha13/h_f137_dual.py --check-dual --expected-head $head --output $out
```

`--prepare-slave` comprueba la cadena Git desde el HEAD que compiló COM4 y que **no cambiaron** ni el package, ni el sketch Master, ni el generador de ese build. Revalida el `MANIFEST.json` y los archivos Master, reutiliza el platform/config staged, y compila solo `G10P_COM14_Slave`. Sella por separado `MANIFEST_COM14.json`. Si falta el directorio original, detenerse y preparar ambos de nuevo con el runner en un directorio nuevo (no inventar o reutilizar archivos de CI). No abrir COM en la preparación.

## 2. Upload dual — permiso ya otorgado en conversación

Una vez **PASS** en ambas verificaciones y los monitores COM cerrados:

```powershell
py -3 tools/alpha13/h_f137_dual.py --upload-both --expected-head $head --output $out
```

El ejecutor exige una confirmación textual única: `AUTORIZO UPLOAD F137 COM14 Y COM4 SIN BACKUP`. A continuación, realiza los uploads **COM14 Slave y luego COM4 Master**, usando únicamente `arduino-cli upload --input-dir` sobre los binarios locales previamente sellados; no recompila y no requiere un respaldo full-flash.

**Fail-closed:** si COM14 falta, su VID/PID no coincide, la identidad es la misma, algún binario/sha cambió, algún upload falla, o ya existe un recibo de subida parcial, detiene la campaña. **No borrar ni repetir automáticamente**; conservar `UPLOAD_RECEIPT_COM14.json`, `UPLOAD_RECEIPT_COM4.json` y logs locales, según corresponda.

Resultado de carga (solo confirma el comando, **no** funcionalidad física):
- `H_F137_UPLOAD_COM14=COMMAND_SUCCESS_UART_PENDING`
- `H_F137_UPLOAD_COM4=COMMAND_SUCCESS_UART_PENDING`
- `H_F137_DUAL_UPLOAD=COMMAND_SUCCESS_2_OF_2_UART_PENDING`

## 3. Observación física después de la subida

Abrir monitores **115200 8N1** para ambas placas y confirmar identificación **F137**, BOOT estable y HB creciente:
- COM14: `G10P_COM14_BOOT` y `VER=G10_PROMOTED_PRODUCT_F137_V1`; `G10P_COM14_HEARTBEAT`, `RTU_REQUESTS` creciendo.
- COM4: `G10P_COM4_BOOT` y `VER=G10_PROMOTED_PRODUCT_F137_V1`; `G10P_COM4_HEARTBEAT`, `RTU_OK` creciendo, `RTU_FAIL`/CRC sin tendencia anómala.
- Con RJ45 en ambos y DHCP disponible: tras estabilización `PHY=1`, `ETH=1`, `TCP=1`, e IP diferente, válida, no `0.0.0.0`. Desde PC usar el probe **read-only** existente `g10p_modbus_tcp_readonly.py` con las dos IP mostradas por UART; además eco UDP a COM4 y control de RTC/SD/FRAM/IO/TFT. No inventar IPs, no adjudicar DHCP con la sola conexión física.
- Un reset watchdog `TG0/TG1`, BOOT que cambia inesperadamente, error RTC/SD/FRAM/IO, red sin DHCP o fallo RTU: **REVIEW_STOP**, guardar evidencia y diagnosticar. No confundir éxito de upload con PASS físico.
- No desconectar RJ45 durante este gate sin autorización de esa maniobra; no repetir la campaña larga G10 R1/R4 ya cerrada.

```text
H_F137_BACKUP=WAIVED_DEV_BOARDS
H_F137_DUAL_UPLOAD_AUTHORIZATION=OPERATOR_20261021
H_F137_LOCAL_SLAVE_SEAL=WAIT_OPERATOR
H_F137_PHYSICAL=WAIT_OPERATOR
H_F137_RELEASE=BLOCKED
PR125=DRAFT_NO_MERGE
NEXT_GATE=H_F137_DUAL_LOCAL_SEAL_UPLOAD_AND_UART_NETWORK_REPORT
```

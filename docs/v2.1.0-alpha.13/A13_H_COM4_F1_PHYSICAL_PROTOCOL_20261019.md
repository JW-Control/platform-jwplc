# Alpha13 — H0-F1: protocolo físico COM4 posterior al PR #116

**Fecha:** 19/10/2026. **Estado:** PREPARADO / NO EJECUTADO.
**Producto:** `JW-Control/platform-jwplc` base `v2.1.0-alpha.13/feature/cleanup-robustness`, PR #116 fusionado en `f2c6821566fa62df744c3af8d4d70a440994e64f`.
**Límites:** únicamente la placa de desarrollo **COM4 Master**; COM14 **NO_TOUCHED**; H0 física separada de la campaña G10-R R1–R4, todavía bloqueada. No ejecutar pruebas físicas en CI/Cloud.

## 1. Condición previa

1. El operador confirma que COM4 es su JWPLC Basic de desarrollo aislado: **CH340 VID 1A86/PID 7523**, ESP32, flash real **4 MB** y **MAC idéntica a la ficha LOCAL**, sin publicar identificadores en GitHub.
2. Relés Q sin cargas ni tensiones externas, ningún equipo industrial conectado, USB seguro, monitores seriales cerrados antes del flash, procedimiento de corte de energía disponible.
3. Se mantiene la excepción previamente aceptada de banco de desarrollo sin backup, **solo para este equipo**. La decisión no cambia la política B4 para equipos de cliente.
4. Git HEAD exacto y limpio que contiene el merge `f2c68215`; copia aislada del package `JWPLC/2.1.0`, no el overlay de H0/G10. Verificar en la compilación la librería elegida, el objeto `w5100.cpp.o`, dependencias y ELF; `JWPLC_ETH_CS=5`.
5. No utilizar `tools/alpha13/gates/a13_g10_r_build_candidate.py --upload-h0-diag-com4`: recompone el overlay de H0-F anterior y no es la vía productiva de F1.

## 2. Separar build, upload y observación

- **Build sin puertos:** compilar un sketch mínimo que, en `setup()`, imprime `H0F1_EARLY_LINK_BEGIN`, llama una sola vez a `JWPLC_Ethernet.linkStatus()`, imprime `H0F1_EARLY_LINK_RETURN=<valor>`, y luego reporta el PHY real por UART a 1 Hz. Mantener el autoload y periféricos del package. Cero escrituras SD/FRAM/relés y ningún reset GPIO4.
- **Upload COM4:** verificar MAC y flash de COM4 justo antes de cargar; exigir consentimiento local específico `H0F1 AUTORIZO SOLO COM4 PACKAGE CORREGIDO SIN BACKUP`; usar únicamente `arduino-cli upload ... --port COM4` con el build y fuente comprobados. No abrir ni flashear COM14.
- **Captura UART:** monitor COM4 a **115200**, guardar el log y `BOOT`/etapa anteriores, idealmente desde el arranque. Si no se capturó el primer `setup()`, el operador decidirá un único reset EN manual como operación separada. Nunca reset automático ni lectura de datos privados en CI.
- **Observación:** 30–60 segundos con RJ45 desconectado; con router de banco aislado comprobar la respuesta PHY ON/OFF real (0=Unknown, 1=LinkON, 2=LinkOFF). Si pasa, medir 15 minutos de progreso sin WDT; desconexión/reconexión RJ45 solamente cuando el operador la autorice de forma explícita. Los 10 ciclos power-cycle y 10 resets EN del plan extendido siguen como operaciones separadas.

**Criterio de H0 físico PASS:** primer `linkStatus()` real retorna en el mismo boot, `STAGE=2`, no se observa `rst:0x8 TG1WDT_SYS_RESET` ni panic, el contador UART progresa y se comprueba PHY real con LinkOFF/LinkON bajo las maniobras autorizadas. `Unknown` persistente no demuestra un PHY válido. Un `LINK=-9` de laboratorio **nunca** equivale a PASS.

**Aborto:** fallo de identidad, salida Q energizada, WDT, reset espontáneo, retorno ausente, pérdida de puerto, error SPI, sobrecalentamiento, condiciones de red inseguras. Tras reset watchdog previo, la sonda debe quedar desarmada y no repetirse automáticamente.

## 3. Registro y cierre

El resultado físico se incorporará en `ALPHA13_STATUS.md` y al registro F131 sin abrir un fallo duplicado; debe contener SHA del package, objeto/ELF, estado del upload COM4, duración, arranques, `EARLY_LINK_RETURN`, PHY real y eventos reset. Preservar logs/MAC/waiver fuera de repositorios públicos. El merge del PR #116 y la CI **no** cierran H0 sin esta evidencia.

```text
H0_F1_MERGED_TO_ALPHA_BASE=YES
H0_F1_H=NOT_RUN
COM4_UPLOAD=NOT_RUN
COM14=NOT_TOUCHED
G10_R_R1_R4=BLOCKED
G10_PACKAGE_PROMOTION=PENDING_OVERLAY_ONLY
NEXT_GATE=H0_F1_COM4_OPERATOR_BUILD_IDENTITY_UPLOAD_THEN_UART_PHY
```

# Alpha13 — H0 WDT COM4: aislamiento directo LINK (experimental)

**Incidencia observada 17/10/2026 en COM4 Master** con firmware PR #109
`701196fc5f1b998fcc1b4c0082e9c75e86672a5a`: upload COM4/COM14 PASS,
Master recae cada ~2 s en `rst:0x8 (TG1WDT_SYS_RESET)`. El monitor imprime
`G10R_BOOT:TCP=1 RTU=1` y `G10R_READY:... LINK=` sin valor siguiente.
La incidencia **persiste** con RJ45 conectado al router aislado y sin RJ45,
con NONCE nuevo en cada boot. TFT parpadea por reinicios completos.
`IP=0.0.0.0` durante el fallo no certifica ausencia de DHCP.

## Hipótesis, no conclusión

En `g10_r_integrated.ino`, tanto READY como HEARTBEAT llaman
`JWPLC_Ethernet.linkStatus()`. Esta función obtiene el mutex SPI hasta
200 ms, deselecciona CS, consulta `Ethernet.linkStatus()`, que a su vez
lee `W5100.getLinkStatus()` por SPI con una transacción y libera el mutex.
Por el log incompleto se infiere que esa ruta es candidata a bloqueo;
**no se ha demostrado** si la causa es adquirir SPI, transacción SPI,
autoloader concurrente o interacción con otro periférico.

## Experimento A/B mínimo (solo sketch Master, no corrección productiva)

- **A = ya ejecutado:** sketch original con dos lecturas PHY directas, WDT
  con y sin RJ45.
- **B = este PR diagnóstico:** reemplazar únicamente las **dos** lecturas
  PHY directas desde el sketch por `LINK=-9`. Enviar al iniciar
  `G10R_H0_WDT_DIAG=SKIP_2_DIRECT_LINK_READS_NO_R1_R4`. `-9` es
  sentinel artificial, **no** significa LINK_OFF/Unknown/LinkON.
- El autoload habitual sigue ejecutando Ethernet, Display, microSD, FRAM,
  RTC, botonera, RS-485, Modbus RTU/TCP y el servicio de IO. **No** se
  quitan periféricos, no se cambia código dentro de `JWPLC/2.1.0`,
  APIs, .a, GPIO4, bootloader ni Alpha15.
- Se usa nueva rama `codex/alpha13-h0-wdt-link-isolation-20261017`.
  La función `--upload` normal que podría cargar ambos está **bloqueada
  explícitamente** aquí; solo `--upload-h0-diag-com4 --dev-waiver <LOCAL_JSON>`
  podrá subir **COM4** con recompilación source-first, HEAD remoto igual,
  MAC y flash reales del Master y frase de autorización específica.
  COM14 no se reprograma. Conserva `R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=False`
  y `R3_PHYSICAL_INJECTION_READY=False`. No autoriza R1–R4.
- El comando esptool que verifica la MAC puede resetear temporalmente COM4.
  No hay respaldo por renuncia específica del propietario para equipos de desarrollo.

**Interpretación:**
1. B también genera `TG1WDT_SYS_RESET`: la lectura PHY directa del sketch
   **no es necesaria** para reproducir; investigar autoload W5500/SPI y
   stack/otros periféricos por separado. No reutilizar campaña H.
2. B se mantiene estable sin RJ45 durante ≥30 s: la lectura PHY directa
   **es un factor necesario bajo esta configuración**, pero todavía no
   certifica que el backend W5500 sea correcto. Repetir solo observación
   con router por 30 s previa autorización; luego instrumentar puntos
   del acquire/getLinkStatus dentro del overlay, no adoptar un bypass.
3. B se mantiene estable solo con RJ45: el comportamiento depende del
   estado del enlace; falta instrumentar PHY/servicio antes de cualquier
   campaña R2.

**Criterios de aborto:** WDT recurrente, reinicios inesperados, pérdida
de COM, salidas Q activas, cambios de carga/hardware o identidad USB
imprevista. En caso de WDT, dejar COM4 sin router y no iniciar R1–R4.

**Evidencia exigida:** compilaciones source-first Windows+Ubuntu, guards
con escenarios negativos sin hardware, test B3 N0/N1/N3 sin fuentes
productivas modificadas, SHA/HEAD exactos, resultado de upload
`PASS_COM4_DIAG_ONLY_EXPLICIT` y traza UART de al menos 30 s.
Nunca reinterpretar PASS de compilación como PASS físico.

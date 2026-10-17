## Evidencia física B — recibida 17/10/2026 13:03 (Perú)

El operador confirmó la ejecución del firmware aislado solo COM4, con:
- `G10R_H0_WDT_DIAG=SKIP_2_DIRECT_LINK_READS_NO_R1_R4` en arranque.
- `G10R_READY` completo a 1 Hz, `LINK=-9` fijo (centinela deliberado, **no** PHY real).
- `BOOT=94B40EC4` constante desde 13:03:16.857 hasta 13:03:46.856, **~30 s sin `TG1WDT_SYS_RESET`**.
- `IP=192.168.0.31 TCP=1 UDP=1 RTU=1` en períodos observados. Hubo pérdida temporal de readiness (`IP=0.0.0.0 TCP=0` 13:03:26–37) y recuperación 13:03:38 sin cambio de BOOT. El operador informa que reinició y desconectó el puerto durante la ejecución; **no existe cronología de sus maniobras**, por tanto no atribuir esa transición a un defecto DHCP.
- 13:03:15.678 registra `rst:0x1 (POWERON_RESET)`, diferente del repetitivo `rst:0x8` en A; el operador había realizado reinicio, no se prueba que ese POWERON haya sido exclusivamente manual.

**Dictamen comparativo:** A reprobara continuamente con `TG1WDT_SYS_RESET` tras `LINK=`; B estabilizó el sketch con el autoload y todos los periféricos integrados sin esas dos lecturas directas. Es evidencia fuerte de que la llamada síncrona a `JWPLC_Ethernet.linkStatus()` desde READY es un disparador específico bajo la condición ensayada, pero aún no identifica la instrucción causal ni justifica retirar esta API del package. Su ruta obtiene mutex SPI 200 ms, deselecciona CS, consulta W5500 PHY y libera; hay que instrumentar los límites *acquire → PHY read → release* por etapa, y recuperar un LINK real sin generar otro bloqueo.

**Limitaciones:** estabilidad solo ≥30 s en reposo del sketch, no R1–R4, no carga RTU/TCP concurrente sostenida, no validación de LINK real, no R2. No dar por aprobada la campaña ni `A13-006`.

```text
H0_WDT_AB=PASS_ISOLATED_CAUSAL_TRIGGER
H0_COM4_STABILITY=OBSERVED_30S_NO_TG1WDT
H0_LINK=-9_SENTINEL_NOT_REAL
H0_DHCP_TRANSIENT=OBSERVED_NEEDS_OPERATOR_TIMELINE
H0_ROOT_CAUSE=OPEN
G10-R-H=REVIEW_STOP_NOT_AUTHORIZED
```

---

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

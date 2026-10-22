# JWPLC Alpha13 — H0-D: lectura PHY en ventana original READY (máximo 8)

**Motivo de apertura:** El experimento C real (PR #111) con `JWPLC_Ethernet.linkStatus()` se ejecutó una vez a ~1,5 segundos desde `setup()`, devolvió `PHY=1 STAGE=50` y no causó watchdog. La versión A anterior ejecutó esa API desde `G10R_READY` y sufrió `TG1WDT_SYS_RESET` cerca de cada 2 s. El autoload también consulta PHY por su cuenta dentro de `JWPLC_Ethernet.service()`; **no hay caché PHY pública equivalente identificada**, aunque `isReady()` y `runtimeState()` consultan estado interno sin tocar SPI.

## Pregunta de investigación

¿La lectura `linkStatus()` del sketch en la **ventana concreta del primer READY** o su repetición a ~1 Hz provoca el WDT bajo autoload completo? No asumir que el mutex está roto porque una lectura aislada funcionó; tampoco asumir que la API es segura bajo concurrencia por ese único éxito.

## Ensayo H0-D (solo COM4)

- Nueva rama `codex/alpha13-h0-wdt-ready-window-20261017` basada en H0-C.
- El sketch imprime `G10R_H0_READY_BOUNDED=8` en `setup()`. Dentro del bloque `if (!active)`, en **exactamente la posición original** tras `Serial.print(" LINK=")`, lee realmente el PHY `JWPLC_Ethernet.linkStatus()`, **como máximo ocho veces** (una por mensaje READY, ~1 Hz). Se conservan el autoload y todos los periféricos, sin modificar código `JWPLC/2.1.0`. En HEARTBEAT se mantiene `LINK=-9` ficticio y en READY vuelve a `-9` después de la octava lectura.
- Cada intento escribe `ATTEMPT=1..8`, `RUN=1` y `STAGE=1` en `RTC_NOINIT_ATTR` antes de la llamada, con gancho staged en `JWPLC_EthernetClass::linkStatus()`: etapas 10 antes de mutex, 11 mutex no conseguido, 20 adquirido, 30 antes PHY, 40 después PHY, 50 liberación. No hay Serial dentro de la sección SPI.
- Si `linkStatus()` retorna, se imprime `LINK=<valor_real> H0_ATTEMPT=<n> H0_STAGE=50`. Después de 8 retornos imprime `H0_CAP_REACHED=1` y deja de leer PHY desde el sketch; el resto de READY reporta `LINK=-9`. El autoload sigue consultando PHY normalmente.
- Si hay watchdog, el siguiente arranque (sin corte de alimentación, y **solo si RTC_NOINIT conserva estado**) muestra `G10R_H0_PREVIOUS:RUN=1 ATTEMPT=<n> LAST_STAGE=<etapa> VALID=1` y **desarma todos los intentos**. No repetir la prueba automáticamente. `POWERON_RESET` o pérdida de RTC pueden reiniciar el contador; se usarán solo reinicios controlados y no es gate de seguridad productivo.

## Interpretación

| Resultado | Inferencia válida |
|---|---|
| WDT en ATTEMPT=1, etapa 10 | La ventana temporal inicial importa; estudiar contención y task |
| WDT en etapa 20/30/40 | Investigar instrucciones/estado dentro de ownership/PHY/release, no asumir timeout normal |
| WDT tras n>1 consultas | Un factor de frecuencia/solapamiento temporal puede intervenir |
| 8 retornos y sin WDT | No se reprodujo con máximo 8 llamadas en la nueva configuración; no asegura inocuidad ni estabilidad prolongada |
| `LAST_STAGE` con `VALID=0` | Registro incompleto; no inferir etapa física |

**Limitaciones:** la instrumentación y los prints alteran ligeramente el timing; ocho consultas no reproducen carga extendida. El gancho staged es débil y solo aplica a `JWPLC_EthernetClass::linkStatus`, no intercepta `Ethernet.linkStatus()` invocado internamente por autoload. Un `LINK=-9` indica únicamente marcador ficticio.

## Reglas

Mantener COM4 Master y COM14 Slave **aislados de cargas externas Q y RS-485**; COM14 sin reflash. Usar router de laboratorio solo si se requiere para constatar LinkON, sin vincular a producción; no ejecutar R1-R4. La rama D contiene solo sketches, scripts, gates y docs; **no cambia `JWPLC/2.1.0`, archives, bootloader, GPIO4, APIs o Alpha15**. Requiere CI completa HEAD exacto, comparación B3 de rutas, MAC/4MB reales del COM4, ficha local H0 previa y permiso interactivo específico `G10R AUTORIZO SOLO COM4 READY LINK 8 SIN BACKUP`. La ruta dual `--upload` debe seguir bloqueada.

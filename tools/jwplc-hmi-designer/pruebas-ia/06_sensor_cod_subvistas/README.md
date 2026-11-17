# Ejemplo 06 — Monitor Sensor COD con Subvistas por Página y Ventanas Emergentes Modales (I5–I6)

Evolución del **Ejemplo 05 (`05_sensor_cod`)** que incorpora dos capacidades clave del **JWPLC HMI Designer**:

1. **Subvistas internas por página (`views`)** en la pestaña `CFG` (`PAGE_MEDICION`):
   - **Subvista 0 (`V1 · Ajustes`)**: muestra el `MENU_MEDICION` con la píldora verde **`INICIAR MEDICION >`**, ajuste de `ETIQUETA: P01`, `TIEMPO: 5 min`, `DESCARGA WIFI` y botones de prueba para disparar alertas.
   - **Subvista 1 (`V2 · Monitoreo`, vista por defecto al medir)**: cambia el contenido de la pestaña `CFG` **en sitio** (sin añadir páginas nuevas a la `NAV_BAR` ni alterar el contador `NN/TT`), mostrando:
     - Cabecera compartida `CONFIGURACION DE MEDICION` + reloj RTC (`HH:MM:SS`).
     - Columna izquierda: `ETIQUETA:` (escala `2×`) y código de muestra `P01` en cian (escala `3×`).
     - Columna derecha: cronómetro `00:02` en amarillo (escala `3×`) y botón **rojo** redondeado con borde blanco **`DETENER`**.
     - Barra de progreso horizontal verde con marco blanco (`BAR`) y texto inferior dinámico `Muestra actual: P01    00:02 / 05:00`.
   - **Subvista 2 (`V3 · Espera_Detener`)**: al pulsar `OK` sobre `DETENER`, el botón pasa a **naranja** con cuenta regresiva **`ESPERA 5` → `ESPERA 1`** (5 s) antes de abrir el modal de confirmación de parada.
   - **Subvista 3 (`V4 · WiFi_QR`)**: al seleccionar `DESCARGA WIFI` con `OK`, conmuta `wifiDownload`, pasa en sitio a la subvista de portal WiFi y renderiza credenciales de prueba (`RED: JWPLC_COD_AP`, `PASS: 12345678`, `IP: 192.168.4.1`), estado booleano `[INACTIVO]` / `[ACTIVO]` y un widget `QR` estático de prueba (`WIFI:T:WPA;S:JWPLC_COD_AP;P:12345678;;`). Pulsar `OK` en `V4` regresa a `V1 · Ajustes`.

2. **Ventanas emergentes modales (`popups[]`, I5–I6)**:
   - **`POPUP_CONFIRM_MEDICION` (`CONFIRM`, ID `0`)**: vinculada con `confirmPopupId: 0` al nodo `INICIAR MEDICION` del menú (`messageTextSize: 2`). Al aceptar con `OK`, activa `measRun = true` (`confirmOutVar`) y conmuta automáticamente a `targetPage: 0`, `targetView: 1` (`V2 · Monitoreo`).
   - **`POPUP_CONFIRM_DETENER` (`CONFIRM`, ID `1`)**: se dispara automáticamente (`triggerVar: "popupConfirmDetener"`) tras los 5 s de espera en `V3 · Espera_Detener`. Permite elegir entre **`SEGUIR`** (vuelve a `V2 · Monitoreo` rojo) o **`DETENER`** (`confirmOutVar: "stopConfirmed"`).
   - **`POPUP_FIN_MEDICION` (`INFO`, ID `2`)**: se dispara (`triggerVar: "popupFinMedicion"`) cuando el temporizador completa el tiempo programado (`CICLO COMPLETADO`), se cierra tras `3500 ms` y regresa a `targetView: 0` (`V1 · Ajustes`).
   - **`POPUP_MEDICION_DETENIDA` (`WARN` amarillo `0xFFE0`, ID `3`)**: se dispara (`triggerVar: "popupMedicionDetenida"`) si el operador confirma detener manualmente (`CICLO DETENIDO`) y al pulsar `OK` regresa a `targetView: 0` (`V1 · Ajustes`).
   - **`POPUP_AVISO_REACTIVO` (`WARN` ámbar, ID `4`)**: alerta preventiva amarilla/ámbar (`triggerVar: "popupAvisoReactivo"`).
   - **`POPUP_ERROR_SENSOR` (`CRIT`, ID `5`)**: falla crítica enclavada con código `errCode: "E104"` reflejado en el indicador `ERR` de la pantalla `IDLE` (`triggerVar: "popupFallaSensor"`).

## Cómo validar, generar previsualizaciones y compilar

```powershell
node tools\jwplc-hmi-designer\cli\jwhmi.cjs validate tools\jwplc-hmi-designer\pruebas-ia\06_sensor_cod_subvistas\sensor_cod_subvistas.jwhmi
node tools\jwplc-hmi-designer\cli\jwhmi.cjs build tools\jwplc-hmi-designer\pruebas-ia\06_sensor_cod_subvistas\sensor_cod_subvistas.jwhmi --sketch tools\jwplc-hmi-designer\pruebas-ia\06_sensor_cod_subvistas\Sensor_COD_06 --preview
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --clean --warnings default --fqbn jwplc_local:esp32:jwplcbasic "tools\jwplc-hmi-designer\pruebas-ia\06_sensor_cod_subvistas\Sensor_COD_06"
```

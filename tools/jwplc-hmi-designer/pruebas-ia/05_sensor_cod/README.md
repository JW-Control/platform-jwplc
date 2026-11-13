# Ejemplo 05 — Monitor Sensor COD (Simulación Visual Multi-Pestaña)

Ejemplo visual completo inspirado en la interfaz TFT de `SENSOR_COD_WIFI_REVISION_25_08.ino`, implementado 100% con el flujo declarativo del **JWPLC HMI Designer** (`sensor_cod.json` -> `JWPLC_HMI_Generated.h`) y **simulación local continua** (sin WiFi ni Modbus).

## Páginas incluidas (7 pestañas `NAV_BAR` estilo `dark-pill`)

| Pestaña | Nombre | Contenido |
|---|---|---|
| `CFG` | Medicion | Reloj RTC (`HH:MM:SS`), `MENU` de control (`Medicion`, `Etiqueta P`, `Duracion`, `Simulacion`), barra de progreso, estado `[REP]`/`[MID]` y temporizador de ciclo (`MM:SS`) |
| `GEN` | Vista_General | Tarjetas codificadas por color para `T_COD` (°C), `COD` (mg/L), `BOD` (mg/L), `TOC` (mg/L), `TURB` (NTU) y estado general de medición |
| `T_CD` | Temp_COD | Tendencia en vivo color magenta (umbral en `35.0`) + indicador `[REP]`/`[MID]` |
| `COD` | Graf_COD | Tendencia en vivo color amarillo (umbral en `80.0`) + indicador `[REP]`/`[MID]` |
| `BOD` | Graf_BOD | Tendencia en vivo color cian (umbral en `45.0`) + indicador `[REP]`/`[MID]` |
| `TOC` | Graf_TOC | Tendencia en vivo color verde (umbral en `30.0`) + indicador `[REP]`/`[MID]` |
| `TUR` | Graf_TURB | Tendencia en vivo color naranja (umbral en `25.0`) + indicador `[REP]`/`[MID]` |

## Muestreo continuo en segundo plano

Las 5 variables (`T_COD`, `COD`, `BOD`, `TOC`, `TURB`) continúan simulándose y acumulando puntos en sus buffers de tendencia aunque estés visualizando `CFG`, `GEN`, otra gráfica o incluso en pantalla `IDLE`. Al regresar a cualquier pestaña de tendencia, la curva muestra el histórico acumulado sin pausas ni saltos artificiales.

## Cómo validar y compilar

```powershell
node tools\jwplc-hmi-designer\cli\jwhmi.cjs validate tools\jwplc-hmi-designer\pruebas-ia\05_sensor_cod\sensor_cod.json
node tools\jwplc-hmi-designer\cli\jwhmi.cjs build tools\jwplc-hmi-designer\pruebas-ia\05_sensor_cod\sensor_cod.json --sketch tools\jwplc-hmi-designer\pruebas-ia\05_sensor_cod\Sensor_COD_05 --preview
& "C:\Program Files\Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe" compile --clean --warnings default --fqbn jwplc_local:esp32:jwplcbasic "tools\jwplc-hmi-designer\pruebas-ia\05_sensor_cod\Sensor_COD_05"
```

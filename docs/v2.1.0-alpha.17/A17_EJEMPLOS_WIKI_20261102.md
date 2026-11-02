# A17 · Ejemplos listos para la Wiki del JWPLC (02/11/2026)

Revisión de los ejemplos que quedan tras la limpieza (PR #225), con la API con «.» (PR #224) y los JW_* de
JW-Libraries PR #7. Fuera de alcance: `JWPLC_LogicRuntime*` (decisión pendiente).

```text
A17_EJEMPLOS_WIKI=IN_PROGRESS   (falta «Qué mirar» en 64 sketches JWPLC_*; se hace tras fusionar #224/#225)
```

## 1. Criterio «utilizable en la Wiki»

Un ejemplo sirve para una página de la Wiki si cumple las seis:

1. **Nombre numerado** y en el orden de aprendizaje (`01.…`, `02.…`); los JW_* conservan su nombre de librería.
2. **API con «.»** (regla 9 de `CLAUDE.md`): sin `::`, `*` ni `&` en el sketch.
3. **Encabezado** con qué hace, qué conectar y qué mirar (Monitor serie, TFT, LEDs de salida).
4. **README** de la librería que lo nombre con una línea de descripción.
5. **Prueba de JWPLC Sim** (`jwplc-sim/tests/examples/<lib>/<ejemplo>.jwtest.json`): la Wiki puede mostrar la
   salida esperada y el usuario puede probarlo sin placa.
6. **Compila para ESP32** con `jwplc_local:esp32:jwplcbasic` sin warnings.

## 2. Estado por librería (77 ejemplos)

| Librería | Ejemplos | 1 | 2 | 3 | 4 | 5 | 6 |
|---|---|---|---|---|---|---|---|
| GlobalPeripherals (plantilla + 7 + curso 11) | 19 | ✅ | ✅ #224 | 🔧 qué mirar | ✅ | ✅ | ✅ |
| Display | 14 + 2 juegos | ✅ #225 | ✅ #224 | 🔧 qué mirar; 05 solo README | ✅ | ✅ | ✅ |
| Ethernet | 3 | ✅ | ✅ | 🔧 qué mirar | ✅ | ✅ | ✅ |
| RS485 | 4 | ✅ #225 | ✅ | 🔧 qué mirar | ✅ | ✅ | ✅ |
| ModbusRTU | 3 | ✅ | ✅ #224 | 🔧 qué mirar | ✅ | ✅ | ✅ |
| ModbusTCP | 2 | ✅ | ✅ | ✅ | ✅ #225 | ✅ | ✅ |
| Expansion, Runtime, UI_Widgets, WebScreen | 9 | 🔧 sin número | ✅ | ✅ | ✅ | ✅ | ✅ |
| JW_FRAM, JW_SD, JW_RTC | 9 | ✅ | ✅ #7 | ✅ #7 | ✅ #7 | ✅ | ✅ |
| JW_RS485, JW_ModbusRTU, JW_DWIN, JW_MatrixButtons | 12 | ✅ | ✅ #7 | ✅ | ✅ #7 | ✅ | ✅ |

Columnas 5 y 6: `[ejecutado]` en la nube (ESP32: 16/16 JW-Libraries, 18/18 #224; Sim: 15/15 y 16/16; los 7
renombrados 7/7 con jwplc-sim PR #16). La columna 3 es `[visto]`: 64 sketches `JWPLC_*` tienen encabezado con
qué hace y qué conectar, pero no dicen qué mirar.

## 3. Lo que falta y dónde se hace

| Pendiente | Dónde |
|---|---|
| Línea «Qué mirar» en los 64 sketches `JWPLC_*` y encabezado corto en `05.HMI_Plantilla_Caldera.ino` | feature `ejemplos-wiki` tras fusionar #224 y #225 (tocan los mismos archivos) |
| Numerar `Expansion_Esclavo_Basic`, `Runtime_*`, `Widgets_*` (`01.`…) | misma feature; requiere copiar sus pruebas en jwplc-sim, como #16 |
| Ejemplos nuevos `08.RTC_Ajustar_Hora` y `09.Botones_Eventos` | tras el sync de JW-Libraries #7 al package |
| `Display_FlappyBird` / `Display_Tetris` | decisión del propietario |

## 4. Plantilla de encabezado

```cpp
/*
  01.Nombre_Del_Ejemplo

  Qué hace: una o dos frases.

  Conexión: qué cablear (o «ninguna: solo la placa»).

  Qué mirar: Monitor serie (115200) «texto esperado», la TFT o los LEDs Q0_x.
*/
```

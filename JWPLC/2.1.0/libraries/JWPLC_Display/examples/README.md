# Ejemplos JWPLC_Display

Serie corta recomendada para taller:

```text
01.Display_IDLE_Status
02.Display_HMI_Fields
03.Display_HMI_Pages
04.Display_TFT_Direct
```

## Ejemplos avanzados y gates

```text
Display_Alpha8_HMI_Gate
Display_DotAPI_Minimal
Display_Efficient_Redraw
Display_FlappyBird
Display_Idle_Return_Modes
Display_Tetris
Display_UserUI_Callbacks
```

Desde Alpha8 el wake USER es seguro por defecto (`IDLE_WAKE_DISABLED`). Los ejemplos que esperan entrada automática a USER configuran explícitamente su botón/modo de wake.

Para dibujo manual nuevo se prefieren los callbacks cortos `jwplcUIEnter()`, `jwplcUIUpdate()` y `jwplcUIExit()`. `04.Display_TFT_Direct` conserva deliberadamente el callback legacy `jwplcUserDisplayRefreshCallback(io, rtc)` porque demuestra el uso directo de los snapshots `JWPLC_IOState`/`JWPLC_RTCState` entregados por el runtime.

Los callbacks manuales conservan la cadencia definida por `setUserRefreshPeriodMs()` cuando no hay fields ni PixelMaps declarativos registrados. Si una aplicación mezcla callbacks manuales con HMI declarativa y necesita ejecución periódica del callback, debe seleccionar `USER_REFRESH_PERIODIC` de forma explícita.

El periodo IDLE no debe endurecerse en un ejemplo salvo que sea parte de lo que se está demostrando. El package mantiene su propio valor por defecto; así los ejemplos heredan futuras mejoras del scheduler.

`Display_Tetris` y `Display_FlappyBird` son además gates físicos útiles para validar callback USER, botonera, dibujo directo y audio no bloqueante mientras el `loop()` principal permanece operativo.

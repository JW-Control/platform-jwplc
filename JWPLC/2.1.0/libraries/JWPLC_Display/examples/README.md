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

Desde Alpha8 el wake USER es seguro por defecto (`IDLE_WAKE_DISABLED`). Los ejemplos que esperan entrada automática a USER configuran explícitamente su botón/modo de wake o realizan `enterUserUI()` desde el sketch.

Los ejemplos con dibujo manual mediante `jwplcUserDisplayRefreshCallback()` conservan su cadencia definida por `setUserRefreshPeriodMs()` cuando no hay fields ni PixelMaps declarativos registrados. Si una aplicación mezcla callbacks manuales con HMI declarativa y necesita ejecución periódica del callback, debe seleccionar `USER_REFRESH_PERIODIC` de forma explícita.

`Display_Tetris` y `Display_FlappyBird` son además gates físicos útiles para validar que el callback USER periódico, la botonera y el dibujo directo continúan avanzando mientras el `loop()` principal permanece operativo.

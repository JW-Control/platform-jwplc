# Ejemplos JWPLC_Display

Serie corta recomendada para taller:

```text
01.Display_IDLE_Status
02.Display_HMI_Fields
03.Display_HMI_Pages
04.Display_TFT_Direct
```

## Plantillas hechas con el HMI Designer

Proyectos completos del HMI Designer listos para abrir, subir y modificar. Cada carpeta trae el `.ino`, el
`JWPLC_HMI_Generated.h`, el proyecto `.jwhmi` (para abrirlo en el Designer), la guía `JWPLC_HMI_LEEME.md`, un
`README.md` y vistas previas en `hmi_preview/`.

```text
05.HMI_Plantilla_Caldera       5 páginas: todos los tipos de objeto, figuras, PIXEL y MENU (plantilla)
06.HMI_Botonera_Interactiva    teclas del sketch por página: horno con recetas, juego de reflejos, motor con rampa
07.HMI_Iconos_Planta           15 iconos con estado (ICON) en una planta de agua simulada
08.HMI_Medidores_Animados      medidor circular (270° y 180°) e iconos que se animan en marcha
09.HMI_Registro_Eventos        historial de eventos con detalle, FRAM y copia a microSD
```

Antes de modificar uno, **Archivo → Guardar como**: los ejemplos del package son de solo lectura.

## Dibujo directo y pantalla USER

Todos dibujan con el objeto `JWPLC_TFT` (sin `::`, `*` ni `&` en el sketch).

```text
10.Display_Fuentes_Grandes     fuentes 16/26/7 segmentos, sin escalar la GLCD
11.Display_Graficos_Sprite     API gráfica: figuras, arcos, sprites
12.Display_Efficient_Redraw    redibujar solo lo que cambia
13.Display_Idle_Return_Modes   tres formas de volver de USER a IDLE
14.Display_UserUI_Callbacks    jwplcUIEnter/Update/Exit con retorno automático
```

## Demos

```text
Display_FlappyBird
Display_Tetris
```

En 2.1.0-alpha.17 `Display_DotAPI_Minimal` se retiró (los LED RUN/ERR/BUS/ETH están en `01`) y los avanzados se
numeraron del 10 al 14.

Por defecto el wake USER es seguro (`IDLE_WAKE_DISABLED`). Los ejemplos que esperan entrada automática a USER configuran explícitamente su botón/modo de wake.

Para dibujo manual nuevo se usan los callbacks cortos `jwplcUIEnter()`, `jwplcUIUpdate()` y `jwplcUIExit()`. Las rutas históricas se conservan únicamente dentro de la librería por compatibilidad con sketches existentes; no forman parte de la API recomendada ni aparecen en los ejemplos públicos.

Los callbacks manuales conservan la cadencia definida por `setUserRefreshPeriodMs()` cuando no hay fields ni PixelMaps declarativos registrados. Si una aplicación mezcla callbacks manuales con HMI declarativa y necesita ejecución periódica del callback, debe seleccionar `USER_REFRESH_PERIODIC` de forma explícita.

El periodo IDLE no debe endurecerse en un ejemplo salvo que sea parte de lo que se está demostrando. El package mantiene su propio valor por defecto; así los ejemplos heredan futuras mejoras del scheduler.

El gate de banco HMI + botonera (antes `Display_Alpha8_HMI_Gate`) está en `tools/display-gates/`, fuera de los
ejemplos.

`Display_Tetris` y `Display_FlappyBird` son además gates físicos útiles para validar `jwplcUIUpdate()`, botonera, dibujo directo y audio no bloqueante mientras el `loop()` principal permanece operativo.

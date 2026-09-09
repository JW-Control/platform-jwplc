# Alpha11 — A11-4 Runtime de páginas: interacción periódica

Fecha: 2026-09-06

## Contexto

Durante el gate integral de `JWPLC_HMI_Generated.h` se detectaron tres observaciones al usar `USER_REFRESH_PERIODIC`:

1. El indicador global `NN/TT` se redibujaba en cada ciclo periódico y podía percibirse como parpadeo.
2. La salida de `PAGE_CONTENT` con ESC dependía del muestreo físico del Display y debía reforzarse con el latch `pressed()` del botón que pertenece al sistema HMI.
3. Se confirmó que la lógica de aplicación debe vivir en `loop()` y `jwplcUIUpdate()` debe limitarse a sincronización gráfica.

## Setter sobre página no visible

`setText()`, `setValue()`, `setBool()` y `setBar()` buscan el field y comparan/actualizan su cache aunque pertenezca a otra página.

Si el valor cambia, el field queda `dirty`. Sin embargo, `markFieldDirty()` sólo solicita refresh TFT inmediato cuando `field.page == currentPage`.

`drawDirty()` también filtra por `currentPage`.

Consecuencia:

```text
SETTER_OFFSCREEN=UPDATES_CACHE
SETTER_OFFSCREEN=NO_TFT_DRAW
SETTER_OFFSCREEN=NO_SPI_REFRESH_BY_ITSELF
```

Existe un pequeño coste CPU de búsqueda/formato/comparación. Para proyectos grandes puede optimizarse `jwplcUIUpdate()` ejecutando sólo los setters de la página actual.

## Indicador NN/TT

Se introduce `g_indicatorDirty`.

El overlay sólo se redibuja cuando cambia alguno de estos estados:

```text
- página activa
- PAGE_SELECT <-> PAGE_CONTENT
- cantidad de páginas
- entrada/reentrada a USER
```

En refresh periódico estable:

```text
INDICATOR_REDRAW_EVERY_PERIOD=NO
```

## Enrutamiento de botones

### PAGE_SELECT

LEFT/RIGHT/UP/DOWN/OK pertenecen al sistema HMI. Para no perder pulsaciones entre ticks de Display, se usan los latches `JWPLC_Buttons.pressed()`.

### PAGE_CONTENT

LEFT/RIGHT/UP/DOWN/OK permanecen intactos para el usuario.

ESC pertenece al sistema HMI y ahora se detecta mediante `JWPLC_Buttons.pressed(BTN_ESC)`, por lo que el retorno a PAGE_SELECT no depende de acertar el intervalo de sondeo físico.

## Nota sobre pressed()

`pressed()` representa un flanco y se consume una vez. Mantener el botón presionado no incrementa continuamente por sí mismo.

Para un BAR 0..100, una prueba `nivel += 1.0f` puede producir un cambio visual de apenas ~1 px según el ancho. Para gate visible se recomienda temporalmente usar pasos de 10 y Serial.

## Smoke recomendado

```cpp
void loop()
{
    if (!JWPLC_Display.isUserPageSelection() &&
        JWPLC_Display.userPage() == PAGE_PAGINA_3)
    {
        if (JWPLC_Buttons.pressed(BTN_UP))
        {
            nivel11 += 10.0f;
            Serial.printf("UP page=%u nivel11=%.1f\n",
                          (unsigned)JWPLC_Display.userPage(),
                          nivel11);
        }

        if (JWPLC_Buttons.pressed(BTN_DOWN))
        {
            nivel11 -= 10.0f;
            Serial.printf("DOWN page=%u nivel11=%.1f\n",
                          (unsigned)JWPLC_Display.userPage(),
                          nivel11);
        }
    }
}
```

Criterio:

```text
INDICATOR_STABLE=YES
ESC_CONTENT_TO_SELECT=PASS
USER_UP_DOWN_PRESSED=PASS
BAR_VALUE_UPDATE=PASS
```

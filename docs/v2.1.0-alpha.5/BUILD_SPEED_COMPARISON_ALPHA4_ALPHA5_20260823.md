# Alpha5 - Comparación de tiempos frente a Alpha4

Fecha: 2026-08-23

Rama: `v2.1.0-alpha.5/feature/esp32-precompiled-compatibility`

Benchmark Alpha5 analizado:

```txt
Run: 20260823_112239
Label: alpha5-post-compatibility-final
Host: PC-MASTER-RACE
CPU: Intel Core i5-13400F
Logical cores: 16
RAM: ~23.8 GiB
OS: Windows 10 Pro 10.0.19045
Arduino CLI: 1.0.2
Jobs: 0
Commit: ae6eedc3f55dfbfacd3357c670176c0af625a7e3
```

## Auditoría previa

La auditoría global posterior a los gates físicos quedó limpia:

```txt
Archives encontrados: 7
PASS: 7
FAIL: 0
```

Archives compartidos que permanecen precompilados:

- `Adafruit_ST7735_and_ST7789_Library`;
- `FS`;
- `JW_FRAM`;
- `JW_RTC`;
- `JWPLC_ModbusRTU`;
- `SPI`;
- `Wire`.

## Resultado Alpha5 actual

| Target | Fase | Tiempo | TUs |
|---|---|---:|---:|
| Basic | managed cold | 89.222 s | 28 |
| Basic | managed warm no-change | 22.480 s | 1 |
| Basic | managed warm touch | 22.245 s | 1 |
| Basic | explicit cold | 88.885 s | 28 |
| Basic | explicit warm no-change | 22.554 s | 1 |
| Basic | explicit warm touch | 23.430 s | 1 |
| Core | managed cold | 102.164 s | 91 |
| Core | managed warm no-change | 23.317 s | 1 |
| Core | managed warm touch | 22.901 s | 1 |
| Core | explicit cold | 95.423 s | 91 |
| Core | explicit warm no-change | 23.513 s | 1 |
| Core | explicit warm touch | 22.819 s | 1 |

## Comparación Basic con hitos Alpha4

Los hitos Alpha4 de PC principal y el Alpha5 actual usan el mismo Intel Core i5-13400F y Arduino CLI 1.0.2. Los P7/P8 formales fueron medidos principalmente en una segunda laptop y se mantienen como referencia, no como comparación causal directa entre hosts.

| Estado | Host / método principal | Cold | TUs | Lectura frente a Alpha5 actual |
|---|---|---:|---:|---|
| Alpha3/base local previo a D1 | PC principal / managed cold | 148.649 s | 102 | Alpha5 managed cold es 59.427 s más rápido (-39.98%). |
| Alpha4 D1 discovery | PC principal / managed cold | 121.732 s | 102 | Alpha5 managed cold es 32.510 s más rápido (-26.71%). |
| Alpha4 P1 | PC principal / cold | 105.940 s | 97 | Alpha5 explicit cold es 17.055 s más rápido (-16.10%). |
| Alpha4 P2 | PC principal / cold | 104.223 s | 34 | Alpha5 explicit cold es 15.338 s más rápido (-14.72%). |
| Alpha4 P3 determinista | PC principal / cold controlado | 101.677 s | 32 | Alpha5 explicit cold es 12.792 s más rápido (-12.58%). |
| Alpha4 P5A Ethernet | PC principal / cold controlado | 90.587 s | 24 | Alpha5 explicit cold queda prácticamente equivalente: 1.702 s más rápido (-1.88%). |
| Alpha4 P6 full Adafruit | PC principal / cold controlado | 67.322 s | 12 | Alpha5 explicit cold es 21.563 s más lento (+32.03%). |
| Alpha4 P8 final | laptop conectada / promedio A-B-B-A | 59.901 s | 5 | Referencia de máximo rendimiento Alpha4; no comparar porcentaje causal con Alpha5 por ser otro host. |

## Comparación del ciclo warm

D1 Alpha4 en el mismo PC principal:

```txt
managed_warm_nochange: 14.157 s
managed_warm_touch:    14.074 s
```

Alpha5 actual:

```txt
managed_warm_nochange: 22.480 s
managed_warm_touch:    22.245 s
```

Diferencia:

- warm no-change: +8.323 s / +58.79 % respecto a D1;
- warm touch: +8.171 s / +58.06 % respecto a D1.

Sin embargo, Alpha5 sigue siendo claramente mejor que el baseline local previo a D1:

```txt
pre-D1 warm no-change: 36.523 s
pre-D1 warm touch:     40.524 s
```

Reducción Alpha5 frente a ese baseline:

- warm no-change: -14.043 s / -38.45 %;
- warm touch: -18.279 s / -45.11 %.

## Explicación estructural

Alpha4 P8 había reducido el cold de `JWPLC Basic` a 5 TUs desde fuente.

Tras la auditoría de compatibilidad Alpha5 se devolvieron a compilación desde fuente las librerías compartidas que contenían dependencias externas `jwplc_*` o estaban asociadas al mismo problema:

```txt
JW_MatrixButtons                 1 TU
JW_SD                           1 TU
SD                              3 TUs
Adafruit_GFX_Library            4 TUs
Adafruit_BusIO                  4 TUs
JWPLC_Display                   2 TUs
JWPLC_Ethernet_W5x00_Backend    8 TUs
-------------------------------------
Total recuperado               23 TUs
```

Por tanto:

```txt
P8 Alpha4:     5 TUs
Fallback:     23 TUs
Alpha5:       28 TUs
```

La cuenta coincide exactamente con el benchmark Alpha5 actual.

En warm, el log Alpha5 conserva sólo 1 compilación real, pero registra 9 pasadas `g++ -E`. D1 Alpha4 registraba 3 pasadas `g++ -E` en warm. Esto indica que la regresión incremental actual no proviene de recompilar las 28 TUs, sino principalmente de discovery/preprocesamiento y enlace de un grafo con más librerías source-only.

## Conclusión

Alpha5 sacrifica parte del máximo rendimiento cold de P6-P8 para recuperar compatibilidad entre `JWPLC Basic`, `JWPLC Basic Core` y `ESP32 Board` sin cambiar APIs ni semántica GPIO.

El estado actual no vuelve al rendimiento lento previo a Alpha4: el cold Basic de ~89 s sigue siendo ~40 % más rápido que el baseline local pre-D1 de ~149 s y queda prácticamente al nivel de P5A (~90.6 s).

El siguiente frente de optimización recomendado es el costo de discovery/preprocesamiento warm, porque todavía se recompila sólo 1 TU pero existen 9 pasadas `g++ -E`. Cualquier nueva precompilación compartida debe pasar obligatoriamente la auditoría de símbolos antes de adoptarse.

No se recomienda reintroducir directamente los archives incompatibles de Alpha4 únicamente para recuperar el cold de 60-67 s.

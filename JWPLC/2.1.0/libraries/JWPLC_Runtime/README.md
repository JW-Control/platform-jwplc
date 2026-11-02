# JWPLC_Runtime

Programación del JWPLC Basic **desde el propio panel**, al estilo del LOGO! 8: el programa se arma con bloques
FBD en la pantalla y la botonera, sin escribir lógica en el sketch. Versión **0.2.0** (package 2.1.0-alpha.16).

```cpp
#include <JWPLC_Runtime.h>

void setup()
{
    JWPLC_Runtime.loadDemoIfEmpty(true); // programa de ejemplo si la FRAM está vacía
    JWPLC_Runtime.begin();
}

void loop()
{
}
```

## Qué trae

- **Pantalla RUN**: la de reposo de siempre (PWR/RUN/ERR, entradas, salidas). Cualquier tecla abre el **MENU**.
- **Programa**: editor FBD (◄ ► ▲ ▼ recorren el mapa, OK edita), Monitor en vivo con los cables en verde,
  lista de bloques, nombre, recursos, *Cargar ejemplo* y *Borrar programa*.
- **Parámetros**: tiempos y presets marcados como parámetro, editables en RUN.
- **Configuración** (fecha y hora, arranque, PIN de Admin, expansión), **Red** (IP, pantalla web) y **Diagnóstico** (resumen,
  eventos, avisos activos, prueba de salidas Q en STOP y **Simular entradas I** en RUN).
- Programa en FRAM; Tarjeta SD guarda y carga programas y exporta los eventos a CSV; los bloques marcados como remanentes conservan su estado al apagar.
- **Expansión RS-485**: otro JWPLC Basic con [`JWPLC_Expansion`](../JWPLC_Expansion/README.md) aporta 8 entradas
  y 8 salidas (`1.I1`, `1.Q1`…), dado de alta desde Configuración > Expansión.
- **Página HMI propia**: el proyecto «Runtime» del HMI Designer genera `JWPLC_HMI_Generated.h` con objetos
  enlazados a I, Q, M y bloques; desde RUN, ► / ◄ abren las páginas (ejemplo `Runtime_HMI`).

## Simular entradas sin cables

*Diagnóstico › Simular entradas I*: OK fuerza la entrada a 1 y cada OK siguiente la alterna 1 ↔ 0 (un pulso es
OK, OK); ◄ o ► la liberan y *Liberar todas* vuelve a las entradas reales. El programa usa el valor forzado
(Monitor, salidas, página HMI) y la pantalla RUN marca esas entradas con «F». Vive solo en RAM: al apagar
desaparece.

## «Demo feria» (programa de ejemplo)

| Bloque | Qué hace | Salida |
|---|---|---|
| B01 SR | I1 marcha, I2 parada del motor | Q1 |
| B02 TON 3 s | la bomba arranca 3 s después del motor (tiempo en Parámetros) | Q2 |
| B03 contador | cuenta piezas en I3 hasta 5 (reset I4), conserva la cuenta al apagar | Q3 |
| B04 aviso | «Lote completo» cuando el contador llega | ERR A04 |

## API del sketch (opcional)

| Llamada | Devuelve |
|---|---|
| `JWPLC_Runtime.begin()` | arranca el Runtime (la pantalla USER y las salidas Q pasan a ser suyas) |
| `run()` / `stop()` / `isRunning()` | RUN/STOP desde el sketch |
| `input(n)` / `output(n)` / `marker(n)` | I1..I8, Q1..Q8, M1..M8 del programa en marcha |
| `block(n)` / `blockSeconds(n)` / `counter(n)` | salida, tiempo transcurrido y cuenta del bloque Bnn |

En modo Runtime el sketch no define `jwplcUIEnter` ni `jwplcUIUpdate`.

## Ejemplos

- `Runtime_Panel`: el Runtime solo.
- `Runtime_HMI`: el Runtime con dos páginas HMI hechas en el Designer.
- Con la pantalla en el navegador: `JWPLC_WebScreen › 02.Runtime_En_Vivo`.

Se distribuye en código fuente (sin `.a`). Diseño y pruebas: `docs/v2.1.0-alpha.16/A16_RUNTIME_MVP_20261101.md`,
`A16_RT2_EXPANSION_DISENO_20261101.md` y `A16_RUNTIME_HMI_20261101.md`.

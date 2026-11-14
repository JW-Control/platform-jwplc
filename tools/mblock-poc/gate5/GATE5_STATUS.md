# JWPLC Basic - mBlock - Gate 5 (fase 2: tiempo, memoria y microSD)

Fecha: 2026-10-24
Rama: `experiment/mblock-jwplc`
Package: 2.1.0-alpha.14 (`origin/v2.1.0-alpha.14/HmiDesigner/main` @ `eddee62e`) vía `jwplc_a14:esp32:jwplcbasic`.
Extensión: `jwplc_basic` 0.4.0. Bridge: 0.3.0.

## Estado

~~~text
GATE5A_LIBRARY_PHASE2=PASS              [ejecutado] JWPLC_mBlock.h: Timer, Clock, Memory, Files, Join
GATE5B_PALETTE_PHASE2=PASS              [ejecutado] Tiempo | Memoria | microSD visibles
GATE5C_CODEGEN_PHASE2=PASS              [ejecutado]
GATE5D_COMPILE_A14_FROM_MBLOCK=PASS     [ejecutado] fase 2 y regresión de la fase 1
GATE5E_UPLOAD_AND_PHYSICAL=PASS         [verificado por el usuario] reloj, temperatura, FRAM, microSD y cronómetro
~~~

## Bloques

Todos los objetos nuevos viven en `extension/src/arduino/JWPLC_mBlock.h`
(sólo header, viaja con la extensión): la vista previa sigue sin funciones
propias en el sketch.

| Categoría | Bloque | Código | Base en el package |
|---|---|---|---|
| Tiempo | cronómetro (s) / reiniciar cronómetro | `JWPLC_Timer.seconds()` / `reset()` | `millis()` |
| Tiempo | [hora/minuto/segundo/día/mes/año/día de la semana] actual | `JWPLC_Time.hour()`… | vista cacheada del RTC, sin I2C extra |
| Tiempo | hora como texto / fecha como texto | `JWPLC_Clock.timeText()` / `dateText()` | `JWPLC_Time` |
| Tiempo | temperatura (°C) | `JWPLC_Clock.temperature()` | `JWPLC_RTC.readTemperatureC()`, máx. 1 lectura/s |
| Tiempo | ¿el reloj tiene hora? | `JWPLC_Time.valid()` | |
| Tiempo | poner en hora el reloj con la computadora | `JWPLC_Clock.setFromComputer()` | `__DATE__`/`__TIME__` + `JWPLC_RTC.write()` |
| Memoria | guardar [valor] en la memoria [1-100] / valor de la memoria [n] | `JWPLC_Memory.save(n, v)` / `load(n)` | `JWPLC_FRAM.writeBlock/readBlock` |
| microSD | ¿hay tarjeta microSD? | `JWPLC_Files.ready()` | `JWPLC_SD.isReady()` |
| microSD | escribir [x] en el archivo [datos.txt] | `JWPLC_Files.appendLine(f, x)` | `JWPLC_SD.open(FILE_APPEND)` |
| microSD | ¿existe el archivo [f]? / borrar el archivo [f] | `JWPLC_Files.exists(f)` / `remove(f)` | `JWPLC_SD.exists/remove` |

Decisiones:

- **Reloj en hora:** se usa la hora de la computadora al compilar. Sólo se
  escribe si el RTC no tiene hora o va atrasado respecto a la compilación, así
  un reinicio no lo devuelve a la hora vieja. Error esperado: el tiempo de
  subida (menos de un minuto). Después se limpia el flag de oscilador detenido.
- **Memoria:** 100 números (float) en los últimos 2 KB de la FRAM, cada uno
  como bloque con checksum (8 B de cabecera + 4 B, cada 16 B). Una memoria
  nunca guardada vale 0. Si el valor no cambió no se escribe (caché en RAM),
  para no cargar el SPI compartido si se guarda dentro de «por siempre». No
  encontré zonas de FRAM reservadas en el package (fuera de `JWPLC_LogicRuntime`,
  que está fuera de alcance).
- **microSD:** `JWPLC_SD` protege cada operación con el mutex del bus SPI; los
  nombres van en la raíz (`datos.txt` -> `/datos.txt`). Recomendación para los
  ejemplos: escribir en la SD desde «cada N segundos», no en cada vuelta.
- **«unir»:** el bloque estándar emitía `String(a) + String(b)` y con variables
  float mostraba «Arranques: 3.00». Se sobrescribe con `JWPLC_Join(a, b)`, que
  usa el mismo formato que la pantalla (3 -> «3», 24,25 -> «24.25», SI/NO).

## Código generado

Programa de prueba `test/cdp-program-phase2.js`; código completo en
`results/20261024_gate5/GATE5_PHASE2_GENERATED.ino`. Extracto:

~~~cpp
void setup() {
  JWPLC_Screen.begin();
  JWPLC_Buttons.clearPendingInput();
  JWPLC_Clock.setFromComputer();
  JWPLC_Memory.save(1, (JWPLC_Memory.load(1) + 1));
  JWPLC_Screen.print(1, JWPLC_Join("Arranques: ", JWPLC_Memory.load(1)));
}

void loop() {
  ...
      JWPLC_Screen.print(2, JWPLC_Clock.timeText());
      JWPLC_Screen.print(3, JWPLC_Clock.dateText());
      JWPLC_Screen.print(4, JWPLC_Join("Temp C: ", JWPLC_Clock.temperature()));
      if(JWPLC_Files.ready()){
        JWPLC_Files.appendLine("registro.csv", JWPLC_Join(JWPLC_Clock.timeText(), JWPLC_Join(";", JWPLC_Clock.temperature())));
      }
  ...
}
~~~

## Compilación

[ejecutado] Desde el renderer de mBlock, con `JWPLC_mBlock.h` enviado por la
misma ruta que el driver:

~~~text
fase 2             425 608 B flash (10 %), 38 284 B RAM (11 %), 41 s
fase 1 (regresión) compila; "unir" -> JWPLC_Join, división en coma flotante
~~~

Log: `results/20261024_gate5/GATE5_COMPILE_A14.log`.

## Gate 5E - Subida y prueba física

[ejecutado] Subida desde «Subir código» de mBlock a COM14: compilación 40 s,
upload 55 s en total, hash verificado, reset por RTS. Log:
`results/20261024_gate5/GATE5E_UPLOAD_PHASE2.log`.

[verificado por el usuario] Los 5 puntos siguientes PASS:

1. Al arrancar, la línea 1 muestra «Arranques: N» y N sube en cada reinicio
   (también tras quitar la alimentación): FRAM.
2. Líneas 2 y 3: hora y fecha de la computadora (diferencia menor a un minuto);
   la línea 4 muestra la temperatura del RTC.
3. Con microSD: cada segundo se agrega «HH:MM:SS;temperatura» a `/registro.csv`;
   ▶ muestra SI; ESC borra el archivo (pitido) y ▶ muestra NO hasta el siguiente segundo.
4. OK reinicia el cronómetro; ▲ muestra el cronómetro; ▼ el minuto actual.
5. Reiniciar la placa sin volver a subir: la hora no debe retroceder.

## Siguiente

- Fase 3: WiFi entre placas (mensajes por la red local, como el «LAN broadcast» de CyberPi).
- Integración con el HMI Designer (pantalla diseñada, campos por nombre).

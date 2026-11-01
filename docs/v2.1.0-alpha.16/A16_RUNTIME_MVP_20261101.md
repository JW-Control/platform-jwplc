# A16 RT1 — `JWPLC_Runtime` 0.1.0 (MVP del Runtime estilo LOGO! 8)

Rama: `v2.1.0-alpha.16/JWPLC-Sim/feature/runtime-mvp`, desde `v2.1.0-alpha.16/JWPLC-Sim/feature/ui-f0-resto`
(PR #202). Depende de #201 (`JWPLC_TFT`/`JWPLC_UI_Widgets`) y #202: se fusiona después de ellas.
Diseño de referencia: `DISENO_RUNTIME_ESTILO_LOGO8.md` (carpeta del proyecto).

```text
A16_RT1_MOTOR_HOST=PASS          [ejecutado] motor (orden topológico, 16 tipos, retentivos) con asserts en g++ de PC
A16_RT1_SIM_EDITOR=PASS_SIM      [ejecutado] editor FBD: programa vacío, nuevo bloque desde la salida, paleta, pines, negación, borrar, guardar
A16_RT1_SIM_RUN=PASS_SIM         [ejecutado] INICIAR/DETENER, monitor en vivo (cables verdes, Ta), Q1 tras T=3 s, contador 5/5
A16_RT1_SIM_AVISOS=PASS_SIM      [ejecutado] bloque AVISO -> ERR "A08" en la pantalla RUN, avisos activos y eventos
A16_RT1_SIM_PERSISTENCIA=PASS_SIM [ejecutado] --state: programa, parámetro, eventos, retentivo (3/5) y arranque "último estado" tras reinicio
A16_RT1_SIM_SD=PASS_SIM          [ejecutado] guardar /jwplc/<nombre>.jwl, listar, cargar con confirmación, exportar /jwplc/eventos.csv
A16_RT1_SIM_MENUS=PASS_SIM       [ejecutado] Parámetros (RUN y STOP), Configuración (fecha, hora, arranque, PIN, retorno), Red, Diagnóstico, prueba de Q
A16_RT1_SIM_TEST=PASS            [ejecutado] jwplc-sim tests/examples/JWPLC_Runtime/Runtime_Panel.jwtest.json
A16_RT1_EJEMPLO_COMPILA=PENDING  [no verificado] Arduino IDE / arduino-cli ESP32 (en la nube downloads.arduino.cc da 403)
A16_RT1_FISICO=PENDING           ver checklist físico
```

## Qué es

Una librería nueva, `JWPLC_Runtime`, que convierte al JWPLC Basic en un equipo programable desde el panel, como un
LOGO! 8. El sketch queda en tres líneas (`examples/Runtime_Panel`):

```cpp
#include <JWPLC_Runtime.h>
void setup() { JWPLC_Runtime.loadDemoIfEmpty(true); JWPLC_Runtime.begin(); }
void loop() {}
```

No reemplaza ni toca `JWPLC_LogicRuntime` / `JWPLC_LogicRuntime_UI` (cadena V2…V14, unas 35 000 líneas). Se empezó de
cero porque la revisión del editor viejo en el simulador mostró cables encima de etiquetas («Trg» sobre «TON»), números
B00, etiquetas cortadas, ningún parámetro visible, sin menús ni modos y nada guardado. Retirar las librerías viejas se
propone recién cuando el propietario valide este MVP en la placa.

## Contraste con el LOGO! 8

| LOGO! 8 | JWPLC Runtime 0.1.0 |
|---|---|
| Menú: Program, Card, Setup, Network, Diagnostics, Start | Menú de 6 baldosas (Programa, Parámetros, Tarjeta SD, Config., Red, Diagnóstico) y botón INICIAR/DETENER |
| Edición desde la salida hacia atrás (Q1 → bloque → entradas) | Igual: OK sobre una Q o un pin abre el selector; «+ Nuevo bloque…» crea el bloque y lo conecta |
| GF / SF / Co como listas | Paleta con categorías (Lógica, Tiempo, Contador, Reloj, Sistema, Analog.) y ayuda con cronograma |
| Bloques B001… con parámetros visibles en el bloque | B01… con su parámetro dentro (T=03:00s, Ta en amarillo en RUN, 3/5, horas, texto del aviso) |
| Negación de entradas en el pin | ◄ ► en el pin la niega; círculo en el pin. NAND/NOR/NOT dibujan el círculo en la salida |
| Protección «+» de parámetros, menú Set Param en RUN | Marca «+» y menú Parámetros: se cambian en RUN sin parar el programa |
| Remanencia por bloque | Opción «Retentivo (FRAM)», marca «Rem» encima del bloque |
| Message texts (aviso en pantalla) | Bloque AVISO: texto, severidad, ERR «Axx» en la pantalla RUN, evento y lista de avisos activos |
| Contraseña | PIN de Admin para editar, SD, Configuración e INICIAR/DETENER; intento fallido queda en eventos |
| Arranque en RUN/STOP | STOP, RUN o último estado |
| Programa en la tarjeta | microSD `/jwplc/<nombre>.jwl`, lista, carga con confirmación; eventos a CSV |
| Diálogos Sí/No | Siempre con el cursor en **No**: un OK de más no borra ni detiene nada |

Lo que el LOGO! tiene y aquí no entra (todavía): bloques analógicos (el JWPLC Basic no tiene ADC), PI, rampas, registro
de datos, red entre equipos y el servidor web (F9: la pantalla Red > Conectar celular ya muestra el QR y el código).

## Bloques (16)

`AND`, `OR`, `NAND`, `NOR` (4 entradas), `XOR`, `NOT`, flanco ascendente y descendente, `SR`, telerruptor (`IMP`),
`TON`, `TOF`, `TP`, `TONR` (retentivo), generador de pulsos `GEN`, contador `CTUD`, temporizador semanal `WEEK` y `AVISO`.
Tiempo 0…9999 en base s (centésimas), m o h, mostrado como el LOGO! («12:50s»). Hasta 64 bloques y 8 marcas M.

## Arquitectura

- Tarea `jwplc_runtime` (núcleo 1, prioridad 2, 10 ms): lee entradas, ejecuta el programa, escribe Q solo si cambian,
  guarda retentivos (máx. cada 500 ms) y atiende los comandos de la pantalla por una cola.
- La pantalla (`jwplcUIEnter/Update/Exit`) corre con el bus SPI tomado, así que **no** toca FRAM, SD ni Ethernet: manda
  un comando y muestra «Procesando…» hasta la respuesta (regla 1 del CLAUDE.md).
- FRAM: configuración en dos copias, programa en dos ranuras A/B con CRC y verificación (la anterior queda de respaldo),
  retentivos con CRC y anillo de 32 eventos.
- Las escrituras de FRAM activan WREN antes de cada `JWPLC_FRAM.write()` (la API de bajo nivel no lo hace; sin eso la
  FRAM real no guarda nada). Lo detectó la prueba de persistencia en el simulador.

## Cambios en `JWPLC_UI_Widgets` (0.4.0)

- `JWPLC_UIList.refreshChanged()`: redibuja solo las filas cuyo texto cambió (valores en vivo sin parpadeo).
- `JWPLC_UIDialog.selectNo()`: deja el cursor en «No».
- El botón único «Aceptar» del diálogo es 30 px más ancho (no entraba en GLCD ×2).

## Checklist físico (taller)

1. Compilar `Runtime_Panel` en Arduino IDE con la placa local; subir con la FRAM borrada: aparece «Demo tablero».
2. Pantalla RUN: PWR, RUN apagado, ERR vacío. Cualquier tecla abre el MENU.
3. INICIAR (◄ para «Sí»): I0.0 + I0.1 → Q0.0 a los 3 s. Monitor muestra Ta en amarillo y cables verdes.
4. Cinco pulsos en I0.6 → Q0.2 y ERR «A08»; Diagnóstico > Avisos activos lo lista; I0.5 lo reinicia.
5. Parámetros en RUN: cambiar T de B02 a 5 s y comprobar el nuevo tiempo sin detener.
6. Cortar la alimentación en RUN con el contador en 3: al volver arranca en RUN y el contador sigue en 3.
7. Editor en STOP: borrar el programa, crear Q1 ← OR(I1, I2) desde la salida, guardar, iniciar y probar.
8. SD: guardar, borrar el programa, cargar desde la lista; exportar eventos y abrir el CSV en la PC.
9. PIN 0001: Editar, SD y Configuración lo piden; un PIN errado queda en eventos.
10. Medir el scan (Diagnóstico > Resumen) con el demo y con 64 bloques; la TFT no debe parpadear con valores en vivo.

## Pendiente (siguientes hitos)

- Servidor web y vinculación real del celular (F9).
- Mover el registro de eventos a `JWPLC_UI_EventLog` cuando se fusione la línea del HMI Designer.
- Retirar `JWPLC_LogicRuntime` y `JWPLC_LogicRuntime_UI` (tras validar este MVP).
- Al cierre de la alpha: decidir si `JWPLC_Runtime` se publica precompilada y regenerar su `.a`.

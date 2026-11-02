# A16 — Pantalla del JWPLC en el navegador (`JWPLC_WebScreen`) — 01/11/2026

```text
A16_WEBSCREEN_SIM=PASS_SIM        [ejecutado] JWSIM_WEBSCREEN: página, teclas desde la web, cuadro idéntico píxel a píxel
A16_WEBSCREEN_EJEMPLOS=PASS_SIM   [ejecutado] jwplc-sim tests/examples/JWPLC_WebScreen 2/2
A16_WEBSCREEN_ESP32=PENDING       [no verificado] compilar en Arduino IDE (ruta ESP32 de TFT_eSPI) y probar en placa
```

## Qué es

Con `JWPLC_WebScreen.begin()` el equipo sirve una página (puerto 80) que muestra la TFT en vivo y tiene la
botonera de seis teclas. Se ve en el navegador de la laptop por cable Ethernet, sin programa en la PC. Las teclas
físicas siguen funcionando; la página solo agrega teclas virtuales. Pensado para la demostración del 03/11
(el público ve en el proyector cómo se navega el Runtime con el equipo en la mano) y para soporte remoto.

## Cómo funciona

- **Copia de la pantalla** (`JWPLC_TFT/src/JWPLC_TFT_Mirror.*`, interno): las macros de escritura de TFT_eSPI
  (`TFT_WRITE_BITS`, `tft_Write_16N`, `DC_C/DC_D` en el procesador ESP32; `tft_Write_8/16` en el genérico que usa
  JWPLC Sim) y `pushBlock/pushPixels` avisan al espejo, que decodifica CASET/RASET/RAMWR como el ST7789 y escribe
  un búfer de 8 bits por píxel con paleta (hasta 256 colores RGB565 exactos): 54 400 B, reservados al arrancar la
  TFT solo si el sketch enlaza `JWPLC_WebScreen` (función débil `jwplcTFTMirrorWanted`). Sin la librería, el costo
  es una comparación por escritura.
- **Versión por fila**: cada fila guarda en qué versión cambió; `/f?v=<versión>&p=<colores>` devuelve solo las filas
  nuevas (RLE) y los colores nuevos. Con la pantalla quieta, ~18 B por pedido; un cambio de menú, ~5–9 KB.
- **Servidor** (`JWPLC_WebScreen`): tarea `jwplc_web` (núcleo 0, prioridad 1), una conexión a la vez, `Connection:
  close`. Cada operación del W5500 toma el bus con el acceso seguro de `JWPLC_Ethernet`; armar la respuesta lee
  RAM y no toca el bus (regla 1).
- **Teclas virtuales**: `JW_MatrixButtons.setVirtualDown(id, bool)` (1.0.6, pasa a **modo fuente**) suma la tecla
  después del antirrebote, con eventos y repetición normales. La página repite «pulsada» cada 1 s mientras se
  mantiene y el equipo la suelta solo a los 4 s si se corta la red.
- **Página**: canvas 320×170 escalado sin suavizado, botonera como la física (◄ ▲ ► / ESC OK ▼), flechas,
  Enter y Esc del teclado; `F` o doble clic = pantalla completa; `T` o `?solo` oculta las teclas.

## Ejemplos

- `JWPLC_WebScreen/examples/01.Pantalla_Web`: cualquier sketch + la pantalla web (DHCP).
- `JWPLC_WebScreen/examples/02.Runtime_En_Vivo`: Runtime de panel + pantalla web con IP fija `192.168.10.10`.

## Evidencia (JWPLC Sim, W5500 simulado + puente al host)

- Pantalla de la web vs. TFT simulada en el mismo instante: 0 píxeles distintos (reposo y MENU del Runtime).
- OK, ▼ y ► desde la página (y desde el teclado en Chromium) mueven el MENU igual que la botonera.
- ~12 cuadros/s en el navegador; 11 colores en el Runtime.
- El servidor queda listo ~6–8 s después del arranque (lo que tarda Ethernet en levantar).

## Para la demo del martes (guion)

1. **Lunes, compilar y subir** `02.Runtime_En_Vivo` (Herramientas > placa JWPLC Basic). Es la primera compilación
   ESP32 de este cambio y de `JW_MatrixButtons` en modo fuente: si falla, mandar el error.
2. **Laptop, una vez**: Configuración > Red e Internet > Ethernet > Asignación de IP > Editar > Manual > IPv4:
   IP `192.168.10.1`, máscara `255.255.255.0`, sin puerta de enlace. (Con router en medio no hace falta: cambiar la IP
   del sketch por una libre de esa red.)
3. Cable directo laptop ↔ JWPLC. Encender; a los ~8 s abrir `http://192.168.10.10/` en Chrome o Edge. El punto verde
   dice «en vivo · N cuadros/s». Pulsar `T` para ocultar las teclas y `F` para pantalla completa en el proyector.
4. Guion sugerido (5 min): pantalla RUN → MENU → Programa > Editar: armar «Q1 ← AND(I1, I2) con TON 3 s» →
   guardar → INICIAR → mover entradas y ver Q y los cables verdes en Monitor → aviso (contador) y ERR «A08» →
   Diagnóstico > Eventos → cortar la luz y volver: sigue en RUN con el contador.
5. **Plan B**: si la red falla, cámara del celular apuntando a la TFT.

## Latencia (02/11, tras la primera prueba en placa)

En la placa la página marcaba 12 cuadros/s y se notaba retraso: la página preguntaba cada 60 ms y cada tecla era
otra conexión que esperaba detrás del cuadro en curso. Rama `v2.1.0-alpha.16/JWPLC-Sim/feature/pantalla-web-latencia`:

- `/f?w=1`: espera larga. Si nada cambió, el equipo responde en cuanto cambia una fila (máx. 400 ms).
- `/f?e=<tecla><1|0>`: la tecla viaja en la petición del cuadro; la página cancela la espera para mandarla.
- `/k` queda por compatibilidad.

`[ejecutado]` JWPLC Sim, `02.Runtime_En_Vivo`, 12 pulsaciones en la página (tecla → cambio en el canvas): mediana
112 → 69 ms, máximo 151 → 100 ms; con la pantalla quieta, 12 → ~2 peticiones/s. `JWSIM_WEBSCREEN=PASS`.
`[no verificado]` en placa: el W5500 real suma el tiempo de envío de los cuadros grandes (entrar al MENU o al editor).

## Pendiente

- Compilación ESP32 y prueba en placa (propietario).
- Al cierre: regenerar `.a` de `JW_MatrixButtons` (ahora en fuente) junto con los demás.
- Más adelante: WiFi (punto de acceso del ESP32) con la vinculación del celular de F9 y contraseña.

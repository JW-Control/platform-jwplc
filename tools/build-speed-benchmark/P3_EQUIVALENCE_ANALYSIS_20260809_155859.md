# P3 - analisis de equivalencia estructural del run 20260809_155859

## Resultado del run

Referencia P2 validada:

- cold: 104.223 s
- compilaciones: 34
- preprocesados `g++ -E`: 51
- `JWPLC_Display.cpp` preprocesado: 17
- app: 406016 bytes

P3 run `20260809_155859`:

- cold: 95.172 s
- compilaciones: 32
- `JWPLC_Display` desde fuente: 0
- core stub P2: 1
- preprocesados `g++ -E`: 49
- `JWPLC_Display.cpp` preprocesado: 0
- app: 406016 bytes

Reduccion frente a P2 validado:

- 9.051 s menos
- 8.7 % menos tiempo cold
- 2 compilaciones menos
- 17 -> 0 pasadas de discovery sobre `JWPLC_Display.cpp`

## Por que fallo el comparador de payload

El comparador P3 original esperaba igualdad byte a byte de la imagen fuera de los campos hash del ESP32.

Ese criterio es valido para algunos cambios que no alteran el orden de link, pero no es valido en general al sustituir objetos sueltos por un archive estatico. Aunque el contenido de los objetos sea el mismo, el linker puede cambiar el orden y las direcciones finales de secciones. Las referencias absolutas/relativas cambian y esto produce diferencias binarias extensas aunque el conjunto de codigo fuente compilado sea equivalente.

En este run:

- P2 y P3 generan exactamente 406016 bytes.
- La comparacion raw detecta muchas diferencias por layout/relocacion y por tanto `payload igual=False`.
- Esto no debe usarse por si solo como criterio de equivalencia para P3.

## Evidencia de objetos compilados

Se compararon los `.o` de las librerias presentes en ambos builds P2 y P3.

- Objetos comunes comparados: 30
- Objetos comunes con SHA-256 identico: 30
- Diferencias entre objetos comunes: 0

Los dos objetos Display usados para fabricar `libJWPLC_Display.a` son exactamente los producidos por el P2 de referencia:

- `JWPLC_Display.cpp.o`
  - 220708 bytes
  - SHA-256 `513d9ff4f65653d02fc5266ce1ab0f49b9b66e535e51c887ca92e955bcea95f9`
- `JWPLC_IdleScreen.cpp.o`
  - 109680 bytes
  - SHA-256 `9f807ce465d912d352d35ea5e79ae67b0dc3bbfd3807bd4337fd9e67f23ba216`

Por tanto, P3 no recompilo Display con flags diferentes: reutilizo exactamente los objetos del P2 validado.

## Evidencia del map P3

A diferencia del intento anterior, el archive P3 si fue extraido por el linker.

El map contiene referencias a:

- `libJWPLC_Display.a(JWPLC_Display.cpp.o)`
- `libJWPLC_Display.a(JWPLC_IdleScreen.cpp.o)`

Tambien quedan resueltos desde el archive los simbolos funcionales del runtime Display, incluyendo:

- `jwplcDisplayDesiredPeriod_ms`
- `jwplcDisplayBeginCallback`
- `jwplcDisplayRefreshCallback`
- funciones de `JWPLCIdleScreen`

## Comparacion de simbolos ELF

Ignorando direcciones finales, el conjunto de simbolos P2/P3 es practicamente igual.

Se observo en el run con el ancla `JWPLC_Display`:

- un simbolo global adicional retenido: `JWPLC_Display`;
- una pequena diferencia de tamano en un destructor weak de `Adafruit_ST7789` generada durante el link;
- el resto del conjunto de simbolos permanece presente.

Para evitar retener artificialmente el objeto global solo como ancla, el siguiente ajuste usa `jwplcDisplayDesiredPeriod_ms`, simbolo que ya es requerido por el runtime normal.

## Secciones principales

Las diferencias de tamano principales entre P2 y P3 fueron solamente:

- `.flash.text`: 217648 -> 217652 bytes (+4)
- `.flash.rodata`: 100848 -> 100856 bytes (+8)

La imagen final conserva el mismo tamano de 406016 bytes.

Estas diferencias son compatibles con layout/alineacion de link al cambiar objetos directos por archive estatico; no indican por si solas una perdida funcional.

## Seleccion de librerias

El run P3 corregido vuelve a seleccionar el mismo conjunto externo que P2:

- Adafruit ST7735/ST7789 1.11.0 desde sketchbook
- Adafruit GFX 1.12.4 desde sketchbook
- Adafruit BusIO 1.17.4 desde sketchbook
- Ethernet 2.0.2 desde sketchbook
- SPI/Wire/SD/FS desde el package JWPLC

No aparece simultaneamente el backend `Ethernet 3.3.8` de Espressif en este run.

La reproducibilidad de dependencias externas sigue siendo un pendiente separado: no debe mezclarse con la aprobacion de P3.

## Conclusion tecnica

El fallo `payload igual=False` del run `20260809_155859` es un falso negativo del criterio de comparacion binaria para este tipo de cambio de link.

La evidencia estructural muestra que:

1. Display no se recompila.
2. Los dos objetos Display del P2 validado son los que forman el archive P3.
3. Todos los objetos comunes de librerias comparados son identicos.
4. Los dos miembros del archive Display se enlazan realmente.
5. El tamano final de app permanece igual.
6. El cold baja de 104.223 s a 95.172 s en esta medicion.

P3 queda tecnicamente prometedor, pero no se declara aun cerrado para release. Falta:

- actualizar el validador para comparar estructura/objetos en lugar de exigir payload raw identico;
- repetir una validacion con el ancla `jwplcDisplayDesiredPeriod_ms`;
- smoke test de autoload;
- prueba fisica en JWPLC Basic;
- validar Basic Core antes de publicar un archive comun.

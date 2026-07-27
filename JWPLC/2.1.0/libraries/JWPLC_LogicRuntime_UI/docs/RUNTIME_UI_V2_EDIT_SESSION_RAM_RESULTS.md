# Resultado físico — sesión de edición RAM v2

## Estado

```text
FECHA: 2026-07-16
PLACA: JWPLC Basic
PUERTO: COM4
COMPILACIÓN ARDUINO IDE: APROBADA
CARGA: APROBADA
PRUEBAS: 18 PASS / 0 FAIL
FRAM: SIN ESCRITURA
SALIDAS FÍSICAS: SIN CONMUTACIÓN
```

## Uso de memoria observado

```text
Flash: 455973 bytes de 3145728 bytes (14 %)
RAM global: 36268 bytes de 327680 bytes (11 %)
RAM disponible para variables locales: 291412 bytes
```

## Resultado serial

```text
PASS: carga programa base
PASS: arranca programa base
PASS: scan inicial
PASS: AND true con segunda entrada negada
PASS: crea borrador RAM
PASS: sesion activa
PASS: borrador inicialmente limpio
PASS: quita negacion de IN2 en borrador
PASS: borrador marcado como modificado
PASS: borrador valido
PASS: aplica borrador y reinicia motor
PASS: scan tras aplicar
PASS: AND false sin negacion
PASS: abre segundo borrador
PASS: cambia IN2 a constante HI
PASS: aplica fuente HI
PASS: scan con HI
PASS: AND true con I0.0 y HI

PASS=18 FAIL=0
```

## Conclusión

La sesión `RuntimeUIV2EditSession` queda aprobada como backend inicial del editor gráfico:

- copia el programa activo a un borrador RAM;
- modifica fuentes y negación sin alterar inmediatamente el motor;
- valida el programa completo;
- recarga y reinicia el motor al confirmar;
- mantiene el programa v2 acíclico;
- permite cancelar el borrador;
- no introduce persistencia FRAM todavía.

## Decisión

Se autoriza integrar la primera pantalla gráfica de edición sobre el mapa FBD aprobado.

Alcance de `JWPLC_LogicRuntime_UI v0.5.0`:

```text
Editar la fuente de una entrada.
Editar la negación de una entrada.
Vista previa gráfica del resultado.
Aplicación diferida fuera del callback TFT.
Cancelación con ESC sin salir de USER.
Limpieza y compuerta de pulsos al cambiar de pantalla.
```

Quedan para etapas posteriores:

- cantidad de entradas de AND/OR/NAND/NOR/XOR;
- parámetro temporal de TON;
- recurso de DigitalInput y DigitalOutput;
- creación, eliminación y cambio de tipo de bloque;
- persistencia en FRAM.

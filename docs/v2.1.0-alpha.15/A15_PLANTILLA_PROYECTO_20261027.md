# A15 · Plantilla estándar de proyecto JWPLC (27/10/2026)

```text
A15_PLANTILLA_PROYECTO_HOST=PASS
A15_PLANTILLA_PROYECTO_ESP32_COMPILE=PASS
A15_PLANTILLA_PROYECTO_FISICO=PENDING
```

Rama: `v2.1.0-alpha.15/OpenPLC-HMI/feature/plantilla-proyecto` (PR hacia `v2.1.0-alpha.15/OpenPLC-HMI/main`).

## Qué es

Pedido del propietario: un formato estandarizado de programa para el JWPLC Basic, con carpetas y archivos
fijos y la ayuda dentro del propio proyecto, para trabajar en sitios sin internet.

Se entrega como ejemplo del package, así queda en el disco al instalarlo por el Gestor de placas:
`JWPLC/2.1.0/libraries/JWPLC_GlobalPeripherals/examples/00.Plantilla_Proyecto_JWPLC/`.

```text
00.Plantilla_Proyecto_JWPLC.ino  orden del ciclo (setup/loop)
A_Config.h  B_Tags.h  K_Utilidades.h
C_Entradas  D_Logica  E_Alarmas  F_Salidas  G_HMI  H_Comunicaciones  I_Persistencia  J_Diagnostico
LEEME.md
docs/REFERENCIA_RAPIDA_JWPLC.md  docs/MAPA_IO.md  docs/PUESTA_EN_MARCHA.md  docs/CAMBIOS.md
```

- Ciclo tipo PLC: entradas → alarmas → lógica → salidas → HMI → comunicaciones → persistencia →
  diagnóstico, sin `delay()`. Entradas leídas una vez por ciclo (`JWPLC_readInputs`), salidas escritas
  una vez y sólo si cambian (`JWPLC_writeOutputs`).
- Lógica de ejemplo: marcha/paro de motor con emergencia y térmico NC, alarmas enclavadas, reconocimiento
  por ESC o Modbus, horómetro en FRAM.
- `G_HMI` detecta `JWPLC_HMI_Generated.h` con `__has_include` y llama `jwplcHMISetup()`; sin él usa la
  pantalla IDLE (RUN/ERR). No toca TFT_eSPI ni `JWPLC_Buttons.update()`.
- Modbus RTU y TCP desactivados por defecto (`A_Config.h`); mismo mapa de holding registers para ambos.
- No cambia headers, `.a` ni APIs del package: sólo añade archivos de ejemplo y una entrada en
  `examples/README.md`.

## Verificación

- `[ejecutado]` Compilación host `g++ -std=gnu++17 -fsyntax-only -Wall -Wextra` del sketch concatenado como
  lo hace el IDE (principal + pestañas en orden alfabético, prototipos generados), contra stubs con las
  firmas copiadas de los headers reales: por defecto, RTU+TCP sin Serial y con `JWPLC_HMI_Generated.h`. PASS.
- `[ejecutado]` Simulación host del ciclo (millis simulado): arranque sin alarmas falsas con NC cerrados,
  marcha → arranque 2 s → marcha, paro, emergencia → FALLA con salidas de marcha apagadas y ERR `EMG`,
  alarma enclavada hasta ESC, horómetro y guardado/lectura en FRAM. PASS. La simulación detectó y se
  corrigió un falso `EMG` al encender (el antirrebote arrancaba en 0); `Antirrebote` toma la primera lectura.
- `[visto]` Cada llamada usada existe en `JWPLC/2.1.0/libraries/*/src/*.h` o `cores/jwcontrol/jwplc_peripherals.h`.
- `[ejecutado]` Compilación ESP32 real (27/10/2026, `tools/alpha15/regenerate_libJWPLC_TFT.sh compile`,
  `jwplc_local:esp32:jwplcbasic`, sobre `v2.1.0-alpha.15/OpenPLC-HMI/main` + esta rama + el curso), sin advertencias:
  por defecto 450 884 B; con `USAR_MODBUS_RTU 1`, `USAR_MODBUS_TCP 1` y `USAR_SERIAL_DIAGNOSTICO 0` 469 852 B.
- `[no verificado]` Si «Guardar como…» del Arduino IDE copia `LEEME.md` y `docs/`. El `LEEME.md` indica
  copiarlos a mano si no aparecen.
- `[no verificado]` Prueba física: cargar en una placa, ver el resumen por Serial, RUN/ERR en la IDLE,
  emergencia y reconocimiento con ESC.

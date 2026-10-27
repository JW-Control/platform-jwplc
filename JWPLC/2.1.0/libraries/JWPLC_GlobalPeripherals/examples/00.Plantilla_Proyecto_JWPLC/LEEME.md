# Proyecto JWPLC Basic: dónde está cada cosa

Esta carpeta sigue la **plantilla estándar JWPLC**. Todos los proyectos tienen las mismas
pestañas, en el mismo orden, para que cualquiera encuentre cada sección sin ayuda externa.
Toda la ayuda necesaria está aquí y en `docs/`: no hace falta internet.

## Empezar un proyecto nuevo

1. Arduino IDE → **Archivo → Ejemplos → JWPLC_GlobalPeripherals → 00.Plantilla_Proyecto_JWPLC**.
2. **Archivo → Guardar como…** con el nombre del proyecto (p. ej. `Bombeo_PlantaNorte`).
3. Revisa que la carpeta nueva tenga `LEEME.md` y `docs/`. Si no se copiaron, cópialos con el
   explorador de archivos desde la carpeta del ejemplo (en el IDE: **Sketch → Mostrar carpeta del sketch**).
4. Llena `A_Config.h` (nombre, cliente, versión) y `docs/MAPA_IO.md`, luego `B_Tags.h`.
5. Reemplaza la lógica de ejemplo (marcha/paro de un motor) en `D_Logica`.

## Las pestañas

| Pestaña | Qué va aquí | Para… |
|---|---|---|
| `00.…ino` (principal) | Sólo el orden del ciclo `setup()` / `loop()` | Casi nunca se toca |
| `A_Config.h` | Nombre, versión, servicios activados, tiempos, parámetros fijos | Activar Modbus, cambiar baudios o tiempos |
| `B_Tags.h` | Nombre de cada entrada/salida, variables, estados, alarmas, temporizadores | Cambiar un borne, crear una variable |
| `C_Entradas` | Lectura de I0_0..I0_7, antirrebote, contactos NC | Invertir una entrada, escalar una señal |
| `D_Logica` | Máquina de estados, secuencias, temporizadores | Cambiar cómo funciona la máquina |
| `E_Alarmas` | Qué dispara cada alarma, enclavamiento y reconocimiento | Agregar o quitar una alarma |
| `F_Salidas` | Escritura de Q0_0..Q0_7 y seguridades de última palabra | Bloquear una salida por seguridad |
| `G_HMI` | Pantalla (IDLE o HMI Designer) y teclas de la botonera | Mostrar un dato, usar una tecla |
| `H_Comunicaciones` | Mapa Modbus RTU/TCP y su copia a variables | Agregar un registro para el SCADA |
| `I_Persistencia` | Datos en FRAM que sobreviven a un corte (consignas, horómetros) | Guardar una consigna |
| `J_Diagnostico` | Mensajes por el monitor serie y tiempo de ciclo | Ver qué pasa durante la puesta en marcha |
| `K_Utilidades.h` | Temporizadores TON/TOF, flancos, antirrebote | No se edita: sólo se usa |

El ciclo, en orden: **entradas → alarmas → lógica → salidas → HMI → comunicaciones →
persistencia → diagnóstico**. Ninguna pestaña usa `delay()`.

## Reglas cortas

- En la lógica se usan los nombres de `B_Tags.h`, nunca `I0_x` / `Q0_x` directamente.
- Las entradas se leen una vez por ciclo (C) y las salidas se escriben una vez al final (F).
- Una pestaña que el proyecto no usa se deja con sus funciones vacías: no se borra.
- Ethernet, pantalla, botonera, SPI y E/S los inicia el package. No llamar `Ethernet.begin()`,
  `JWPLC_Buttons.update()` ni usar TFT_eSPI directamente.
- Cada cambio entregado se anota en `docs/CAMBIOS.md` y se sube `PROYECTO_VERSION`.

## Carpeta docs/

| Archivo | Para qué |
|---|---|
| `docs/REFERENCIA_RAPIDA_JWPLC.md` | Funciones del JWPLC Basic y ejemplos del package, sin internet |
| `docs/MAPA_IO.md` | Tabla de bornes, tags y mapa Modbus del proyecto |
| `docs/PUESTA_EN_MARCHA.md` | Checklist de campo antes de entregar |
| `docs/CAMBIOS.md` | Bitácora de versiones del proyecto |

## Pantallas con el HMI Designer

Si diseñas pantallas con el JWPLC HMI Designer y usas «Escribir en sketch» sobre esta carpeta, aparece
`JWPLC_HMI_Generated.h` y `G_HMI` lo usa solo. Copia en `actualizarHMI()` las asignaciones del
«Sketch base» de `JWPLC_HMI_LEEME.md`. No edites los archivos que escribe el Designer.

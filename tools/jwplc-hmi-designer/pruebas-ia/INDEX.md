# Pruebas de HMI generadas por IA

Cada carpeta es una prueba de hasta dónde llega un agente diseñando una HMI completa con la CLI
`jwhmi` (guía: [`../agent/GUIA_AGENTES.md`](../agent/GUIA_AGENTES.md)), sin abrir la UI.

| Prueba | Fecha | Qué pidió | Resultado |
|---|---|---|---|
| [`01_caldera`](01_caldera/README.md) | 2026-10-23 | HMI exigente libre para mostrar el alcance | 5 páginas, 31/32 objetos, 0 avisos, compila en host y en ESP32. Placa: [no verificado] |
| [`02_botonera`](02_botonera/README.md) | 2026-10-23 | Algo más creativo e interactivo con la botonera | Receta con cuenta atrás, juego de reflejos con récord, motor con rampa y MENU. 0 avisos, compila en ESP32. Placa: [no verificado] |
| [`03_iconos`](03_iconos/README.md) | 2026-10-23 | Iconos con estado (tipo ICON) desde la CLI | Planta de agua simulada: 15 ICON de 24 a 64 px en 2 páginas, paro, auto/manual y falla con la botonera. 0 avisos, compila en ESP32. Placa: [no verificado] |

Las tres están publicadas también como ejemplos de `JWPLC_Display` (*Archivo → Ejemplos → JWPLC_Display*):
`05.HMI_Plantilla_Caldera`, `06.HMI_Botonera_Interactiva` y `07.HMI_Iconos_Planta`, con el mismo `.ino` y el mismo
`JWPLC_HMI_Generated.h`. Si cambias una prueba que ya es ejemplo, actualiza también la copia del ejemplo.

## Cómo agregar una prueba

Carpeta `NN_nombre/` con:

- `<nombre>.json`: el proyecto que escribió el agente (forma corta, `$schema` a `../../schema/jwhmi.schema.json`);
- `<Sketch>/`: salida de `jwhmi build <nombre>.json --sketch <Sketch> --create-ino --preview` (se abre en el Arduino IDE y en el Designer);
- `<nombre>_todas.png`: las páginas juntas;
- `README.md`: el pedido, lo que se verificó, lo que encontró el agente (fallos de la CLI o del Designer) y lo pendiente.

Verificar desde la raíz del repo:

```bash
node tools/jwplc-hmi-designer/cli/jwhmi.cjs check tools/jwplc-hmi-designer/pruebas-ia/NN_nombre/<nombre>.json
```

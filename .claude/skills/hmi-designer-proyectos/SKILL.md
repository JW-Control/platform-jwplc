---
name: hmi-designer-proyectos
description: Crear o modificar un proyecto completo del JWPLC HMI Designer (.jwhmi) sin la UI, con la CLI jwhmi (validar, vista previa PNG, generar JWPLC_HMI_Generated.h y escribirlo en un sketch). Usar cuando pidan diseñar una pantalla/HMI para la TFT del JWPLC Basic, armar un .jwhmi o actualizar el HMI de un sketch.
---

# Proyectos del HMI Designer por JSON

1. Lee `tools/jwplc-hmi-designer/agent/GUIA_AGENTES.md` (flujo, reglas de la TFT, recetas) y, para
   cada propiedad, `agent/REFERENCIA_JWHMI.md` o `node tools/jwplc-hmi-designer/cli/jwhmi.cjs describe <TIPO>`.
2. Parte de un ejemplo de `tools/jwplc-hmi-designer/examples/` o de `jwhmi new`.
   Para equipos con símbolo (motor, bomba, válvula, piloto…) usa `ICON`: `jwhmi icons` lista los
   `iconId`, tamaños y estados (`examples/06_iconos.json`).
3. Itera con `validate` hasta 0 errores y 0 avisos, y **mira** los PNG de `preview` antes de entregar.
4. `check` (compila en host) y `build --sketch <carpeta>` para escribir header, `.jwhmi` y guía.
   El `.ino` del usuario nunca se escribe (sólo `--create-ino` crea uno si no existe).
5. Entrega diciendo qué se verificó: validación, vista previa, compilación en host. La compilación
   ESP32 real y la prueba en la placa quedan `[no verificado]` hasta que las haga el equipo.

Si cambias el Designer (no un proyecto), sigue `tools/jwplc-hmi-designer/CLAUDE.md` y el skill
`hmi-designer-ux`; si cambias `cli/lib/spec.cjs`, corre `jwhmi schema` y `tests/agent-cli.test.cjs`.

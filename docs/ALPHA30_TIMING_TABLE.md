# Alpha30 - Tabla de tiempos

## Entorno

| Campo | Valor |
|---|---|
| Sistema operativo | Windows |
| IDE | Arduino IDE / Arduino CLI |
| FQBN | `jwplc:esp32:jwplcbasic` |
| Board | JWPLC Basic |
| Core | jwcontrol |
| Sketch base | `JWPLC_IO_BlockMirror` |
| Puerto | COM14 |
| Upload speed | 921600 |
| CPUFreq | 240 MHz |
| FlashFreq | pendiente |
| PartitionScheme | default |

---

## Mediciones iniciales

| Caso | Tiempo | Observación |
|---|---:|---|
| Primera compilación/subida desde Arduino IDE | ~3:10 | Observado en alpha29 |
| Segunda compilación/subida sin cambios desde Arduino IDE | ~55-60 s | Usa previamente compilados |
| Arduino CLI limpio | pendiente | |
| Arduino CLI sin cambios | pendiente | |
| Arduino CLI compile + upload | pendiente | |
| Arduino CLI con cambio mínimo | pendiente | |

---

## Después de bootloader precompilado

| Caso | Tiempo antes | Tiempo después | Diferencia |
|---|---:|---:|---:|
| Build limpio | | | |
| Build sin cambios | | | |
| Build + upload | | | |

---

## Después de app-only upload

| Caso | Tiempo antes | Tiempo después | Diferencia |
|---|---:|---:|---:|
| Upload full | | | |
| Upload app-only | | | |

---

## Conclusión

Pendiente.

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

## Prueba A/B bootloader precompilado

| Caso | Repetición | Limpio/incremental | Tiempo | SHA-256 bootloader | Observación |
|---|---:|---|---:|---|---|
| Sin bootloader en variante | 1 | limpio | pendiente | pendiente | genera desde ELF SDK |
| Sin bootloader en variante | 1 | incremental | pendiente | pendiente | genera desde ELF SDK/cache |
| Con bootloader en variante | 1 | limpio | pendiente | pendiente | copia desde variante |
| Con bootloader en variante | 1 | incremental | pendiente | pendiente | copia desde variante |

### Criterio de aceptación

Se considerará que el bootloader precompilado aporta mejora real si:

- la diferencia se repite en varias corridas;
- la mejora no desaparece al limpiar caché;
- el SHA-256 del bootloader es estable para sketches distintos con el mismo FQBN;
- no hay fallas de subida ni arranque en hardware real.

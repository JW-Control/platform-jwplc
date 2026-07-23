# JWPLC Basic v2.1.0-alpha.3 - Remote I/O RTU

Esta carpeta concentra la documentación de la etapa **v2.1.0-alpha.3** para la integración opcional de **JWPLC Remote I/O sobre Modbus RTU / RS-485**.

La convención de nombre con punto se mantiene para ordenar correctamente las etapas del package:

```txt
v2.1.0-alpha.3
```

## Estructura

| Archivo | Uso |
|---|---|
| `JWPLC_V2_1_0_ALPHA3_REMOTE_IO_RTU_NOTES.md` | Notas de etapa para PR/pre-release. |
| `JWPLC_REMOTE_IO_RTU_PROTOCOL.md` | Protocolo Remote I/O RTU v1. |
| `JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md` | Plan de implementación por PoC. |
| `JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md` | Checklist de validación física de la PoC 1. |
| `JWPLC_REMOTE_IO_RTU_HANDOFF_NEXT_CHAT.md` | Handoff técnico para continuar el desarrollo. |

## Rama de trabajo recomendada

```txt
feature/v2.1.0-alpha.3-remote-io-rtu-poc1
```

## Regla principal

Esta etapa prepara Remote I/O RTU como integración opcional. No debe tocar el package estable público ni romper el uso normal de Arduino IDE.

Para la PoC 1, el objetivo es validar un firmware mínimo `JWPLC_RemoteIO_Slave_RTU` con ID fijo, `Serial2`, `FC2`, `FC1`, `FC5` y `FC15`, antes de sumar FRAM, commissioning, UID/MAC o UI de OpenPLC Editor.

# Entrega corregida - JWPLC Basic v2.1.0-alpha.3 Remote I/O RTU

Esta carpeta reemplaza la entrega anterior que conservaba el nombre `alpha13` del ZIP importado.

La versión correcta para esta línea del package es:

```txt
v2.1.0-alpha.3
```

## Qué contiene

| Archivo | Uso |
|---|---|
| `docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md` | Protocolo Remote I/O RTU v1. |
| `docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_IMPLEMENTATION_PLAN.md` | Plan de implementación por PoC. |
| `docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_POC1_CHECKLIST.md` | Checklist de validación física PoC 1. |
| `docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_HANDOFF_NEXT_CHAT.md` | Handoff/prompt técnico para continuar. |
| `docs/v2.1.0-alpha.3/JWPLC_V2_1_0_ALPHA3_REMOTE_IO_RTU_NOTES.md` | Notas de etapa para PR/pre-release. |

## Ruta recomendada de branch

```txt
develop/v2.1.0-alpha.3-remote-io-rtu
```

## Regla principal

Esta etapa prepara Remote I/O RTU como integración opcional. No debe tocar el package estable público ni romper el uso normal de Arduino IDE.

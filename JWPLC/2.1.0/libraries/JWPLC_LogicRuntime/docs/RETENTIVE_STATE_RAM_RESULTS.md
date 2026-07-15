# Resultado de validación — retentivos v1 en RAM

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 34 PASS, 0 FAIL
RETENTIVOS EN RAM: PASS
```

## Compilación física

```text
Flash:       424845 bytes / 3145728 bytes (13 %)
RAM global:   32612 bytes / 327680 bytes (9 %)
RAM restante: 295068 bytes
```

## Alcance confirmado

- `LogicBlockDefinition` continúa ocupando 12 bytes.
- Los inicializadores históricos conservan `flags = 0`.
- El flag `RETENTIVE` solo es válido para `SET/RESET`.
- `TON` mantiene semántica no retentiva.
- El validador y el codec rechazan flags desconocidos.
- El validador y el codec rechazan retención aplicada a `TON`.
- El flag sobrevive a serialización y deserialización.
- El bitmap ignora bloques no retentivos incluso cuando sus bits están activos.
- El estado retentivo puede exportarse, limpiarse e importarse.
- El snapshot puede reimportarse después de `stop()`.

## Seguridad de la prueba

La prueba inicializó E/S para usar la fachada completa del runtime, pero:

- no incluyó bloques `DigitalOutput`;
- no utilizó la FRAM;
- mantuvo Q0 apagadas;
- dejó el autoload normal del package intacto.

## Decisión

La fase RAM queda cerrada. La siguiente etapa valida el registro retentivo A/B con inyección exhaustiva de cortes antes de escribir la región física de FRAM.
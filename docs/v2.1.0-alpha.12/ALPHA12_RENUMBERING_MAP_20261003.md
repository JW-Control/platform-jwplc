# Alpha12 — mapa de renumeración y trazabilidad

Fecha: 2026-10-03

## Decisión

El trabajo desarrollado históricamente bajo:

```text
branch: v2.1.0-alpha.14/feature/modbus-tcp
docs:   docs/v2.1.0-alpha.14/
```

se promoverá como release:

```text
v2.1.0-alpha.12
```

La numeración de desarrollo histórica no se reescribe ni se borra. Se conserva
como evidencia de los gates ejecutados y de sus commits originales.

## Motivo

Alpha11 es la última PreRelease publicada. El siguiente release real del canal
2.1.x debe ser Alpha12.

La planificación queda reorganizada así:

```text
Alpha11 = cerrado/publicado

Alpha12 = Ethernet/W5500 + Modbus TCP + hardening/optimización RTU
          + runtime/full-runtime + ceilings/coexistencia
          + consolidación del package

Alpha13 = TFT/Display
          actualización del alpha originalmente planificado para TFT
          incorporando los cambios reales ya realizados en JWPLC_Display/JWPLC_TFT

Alpha14 = OpenPLC + mejoras de integración
          incorporando el nuevo estado de Modbus TCP/RTU optimizados
          y actualizando el alcance OpenPLC originalmente previsto
```

## Política de trazabilidad

Los documentos existentes bajo `docs/v2.1.0-alpha.14/` son evidencia
histórica válida del desarrollo previo a la renumeración.

No se deben copiar/renombrar en masa porque:

1. contienen SHAs y nombres de branches reales usados durante las pruebas;
2. moverlos ocultaría la historia de cómo se obtuvieron los resultados;
3. cientos de gates ya referencian rutas Alpha14;
4. la evidencia debe conservar el contexto original.

Desde este punto:

```text
RELEASE_IDENTITY=v2.1.0-alpha.12
HISTORICAL_DEVELOPMENT_LABEL=alpha14/modbus-tcp
NEW_CANONICAL_DOCS=docs/v2.1.0-alpha.12/
```

Los documentos Alpha12 pueden enlazar evidencia histórica Alpha14 cuando sea
necesario.

## Rama canónica de cierre

Se crea desde el HEAD final de la campaña de coexistencia:

```text
v2.1.0-alpha.12/feature/modbus-tcp
BASE_FROM_HISTORICAL_HEAD=9d152bfb76324c08d1fc3c84d1593aa9461ae978
```

La rama histórica Alpha14 no se elimina.

## Regla de release

A partir de este punto, cualquier cambio destinado a la próxima PreRelease debe
usar identidad Alpha12 en:

- README raíz;
- PRE_RELEASE;
- checklist;
- status;
- PR;
- tag;
- índice dev;
- artefacto ZIP;
- validación Boards Manager/Arduino CLI/IDE.

No se debe publicar `v2.1.0-alpha.14` desde la rama histórica.

## Roadmap posterior

```text
NEXT_AFTER_ALPHA12=ALPHA13_TFT
ALPHA14_AFTER_ALPHA13=OPENPLC_AND_INTEGRATION
```

El alcance exacto de Alpha13 y Alpha14 se reauditará contra el package publicado
de Alpha12 antes de iniciar cada rama.

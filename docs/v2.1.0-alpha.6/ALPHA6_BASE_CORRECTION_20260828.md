# v2.1.0-alpha.6 — Corrección de base de integración

Fecha: 2026-08-28

## Motivo

Durante el cierre documental y de publicación de Alpha6 se detectó que la rama original:

```text
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

no estaba construida sobre el cierre funcional definitivo de Alpha5.

La comparación con `release/v2.1.x` mostró una divergencia histórica amplia y, de forma crítica, la rama Alpha6 conservaba una arquitectura de core anterior a la normalización final de Alpha5.

Ejemplo observado:

```text
Alpha6 antigua : jwplcbasic.build.core=jwcontrol_p2
Alpha5 final   : jwplcbasic.build.core=jwcontrol_precompiled_stub
```

Por este motivo los benchmarks y gates de compilación ejecutados sobre la línea antigua no podían considerarse evidencia final de publicación, aunque conservaran valor como evidencia funcional de Alpha6.

## Acción correctiva

Se bloqueó la publicación y se creó un backup inmutable del estado Alpha6 previamente validado:

```text
backup/v2.1.0-alpha.6-validated-20260828
```

Se creó una nueva rama desde el cierre real de Alpha5:

```text
v2.1.0-alpha.6/integration/rebase-alpha5-final
base: release/v2.1.x @ 64068556
```

En lugar de mezclar los historiales divergentes, se trasladó el delta funcional Alpha6 sobre Alpha5 final mediante patch 3-way. Resultado:

```text
APPLY_EXIT=0
conflictos reales=0
```

La infraestructura final de Alpha5 quedó preservada, incluyendo:

```text
jwplcbasic.build.core=jwcontrol_precompiled_stub
JWPLC/2.1.0/precompiled/core/JWPLCBASIC/core.a
```

La consolidación W5500 de Alpha6 se mantuvo y la antigua librería `JWPLC_Ethernet_W5x00_Backend` quedó eliminada en favor de la implementación integrada dentro de `JWPLC_Ethernet`.

Commit de integración:

```text
bb925681 feat(alpha6): integrar sobre cierre final alpha5
```

## Revalidación posterior

### Source fallback

Antes de readoptar Display precompilado se validó la combinación Alpha5-final + Alpha6 desde source:

```text
Compile exit           : 0
Display source objects : 2
ALPHA6_ALPHA5_SOURCE_FALLBACK_SMOKE=PASS
```

### JWPLC_Display

El archive anterior no se reutilizó. Se regeneró desde la nueva base y se validó paridad exacta:

```text
Archive bytes           : 368174
SHA256                  : 4da9143e5e80d8ad0890e25bda8802ecee489b2a8c452c3ef1be556cff9541a7
Archive members exactos : True
Source Display compiles : 2
Archive Display compiles: 0
Source app              : 409765
Archive app             : 409765
Source RAM              : 27668
Archive RAM             : 27668
Raw .bin delta          : 0
Structural parity       : True
ALPHA6_DISPLAY_FINAL_ARCHIVE=PASS
```

Commit de adopción:

```text
379246c9 build(display): adoptar archive final alpha6 sobre alpha5
```

### Cold final de producción

```text
HEAD                         : 379246c9
Tiempo                        : 62.261 s
Application .ino.bin          : 456816 bytes
Display precompiled           : True
Display source objects        : 0
Basic core.a observado        : True
Git status                    : clean
ALPHA6_INTEGRATED_FINAL_PRODUCTION_COLD=PASS
```

### Benchmark final

Run:

```text
20260828_174534
alpha6-integrated-final-379246c9
```

Resultado:

```text
12/12 fases = PASS
cold promedio combinado = +9.88 % frente a Alpha5
warm promedio combinado = -4.43 % frente a Alpha5
```

La regresión cold se registra y acepta como costo conocido de 7 TUs source adicionales asociadas al Ethernet consolidado. No se eliminan periféricos del autoload para recuperar tiempo.

## Decisión

La evidencia final de publicación de Alpha6 es únicamente la generada sobre la rama corregida basada en `64068556`.

Los resultados de la línea Alpha6 antigua se conservan como evidencia histórica/funcional, pero no se usan como benchmark final ni como gate final de publicación.

```text
ALPHA6_BASE_CORRECTION=COMPLETE
ALPHA6_TECHNICAL_VALIDATION=PASS
ALPHA6_READY_FOR_PR=YES
```

# Alpha10 - Auditoría de protecciones de library discovery

## Objetivo

Revisar qué mecanismos de selección/aislamiento de librerías siguen siendo necesarios antes de cerrar nuevamente `v2.1.0-alpha.10`.

La prioridad es recuperar tiempo de compilación sin retirar periféricos del autoload, sin romper Arduino IDE y sin alterar APIs ya validadas.

## Modelo soportado

Para las librerías propias del producto se adopta el siguiente contrato:

```text
SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
MANUAL_JW_JWPLC_OVERRIDES=OUT_OF_SCOPE
AUTOLOAD_PERIPHERALS_REMOVED=NO
```

Una copia antigua de una librería `JW_*` o `JWPLC_*` instalada manualmente en el sketchbook no se considera una configuración que el package deba aislar automáticamente.

Esto no impide diagnosticar ese entorno; simplemente evita pagar permanentemente un coste de discovery para defender una instalación paralela no soportada.

## Auditoría

| Mecanismo | Origen | Decisión Alpha10 | Motivo |
|---|---|---|---|
| `JWPLC_Bundled_JWPLC_Ethernet.h` | Alpha10 inicial | **RETIRAR** | Sólo protege una copia homónima/manual de `JWPLC_Ethernet` en sketchbook. El benchmark medido mostró `+1.233 s / +5.6%` warm con ese marker. |
| `JWPLC_Ethernet 1.0.1` asociado al guard | Alpha10 inicial | **VOLVER A 1.0.0** | El incremento se introdujo únicamente junto con el guard; no hubo cambio de API ni runtime que requiera mantenerlo. |
| `JWPLC_Bundled_Adafruit_ST77xx.h` | Alpha5 heredado por Alpha9 | **CONSERVAR** | Adafruit es dependencia externa y puede coexistir legítimamente con copias instaladas por Library Manager/sketchbook. Además participa del stack precompilado del Display. |
| `JWPLC_Bundled_Adafruit_GFX.h` | Alpha5 heredado por Alpha9 | **CONSERVAR** | Protege la selección reproducible del archive `precompiled=full` validado para el package. |
| `JWPLC_Bundled_Adafruit_BusIO.h` | Alpha5 heredado por Alpha9 | **CONSERVAR** | Durante Alpha5 se observó realmente que Arduino seleccionaba BusIO desde el sketchbook en un gate genérico; el marker se adoptó para forzar la copia vendorizada compatible con el archive. |
| `JWPLC_LIBRARY_DISCOVERY_PHASE` | Arquitectura heredada | **CONSERVAR** | Permite mantener livianos los headers de autoload durante discovery y soporta la selección del stack Display precompilado. |
| archives precompilados | Alpha4-Alpha8 | **CONSERVAR** | Son parte de la estrategia de build-speed ya validada; este alpha no reabre su generación ni ABI. |
| autoload de periféricos | Arquitectura JWPLC | **CONSERVAR** | No se obtiene velocidad retirando Display, Ethernet, SD, FRAM, RTC, botonera, RS-485, Modbus RTU o TCA/I/O. |

## Evidencia que obliga a conservar los markers Adafruit

En Alpha5 quedó documentado que, durante el gate genérico de GFX, Arduino resolvió `Adafruit_BusIO` desde el sketchbook mientras Basic y Basic Core usaban la copia vendorizada. Esa observación llevó a exigir `JWPLC_Bundled_Adafruit_BusIO.h` para el piloto BusIO posterior.

Por eso el criterio de Alpha10 diferencia dos casos:

```text
JW/JWPLC manual duplicate      -> unsupported environment; no guard permanente
Third-party Adafruit duplicate -> legitimate coexistence; selección reproducible requerida
```

No se retiran los tres markers Adafruit en este cambio.

## Cambio técnico adoptado

Commit:

```text
35385c7286c8a4fdf33aec1af1175b8bb4f45e64
perf(alpha10): retirar guard de shadowing de JWPLC_Ethernet
```

El commit:

- elimina `JWPLC_Bundled_JWPLC_Ethernet.h`;
- restaura `JWPLC_GlobalPeripherals_Auto.h` al comportamiento de Alpha9;
- restaura `JWPLC_Ethernet` de `1.0.1` a `1.0.0`;
- elimina el verificador específico que exigía ignorar `JWPLC_Ethernet` del sketchbook;
- conserva el smoke CI de periféricos soportados;
- no cambia API pública, runtime, archives ni autoload.

## Estado

```text
ALPHA10_ETHERNET_SHADOW_GUARD=REMOVED
ALPHA10_ADAFRUIT_MARKERS=RETAINED
ALPHA10_GENERALIZED_JW_MARKERS=NOT_ADOPTED
ALPHA10_SUPPORTED_LIBRARY_MODEL=PACKAGE_MANAGED
ALPHA10_PROTECTION_AUDIT=PASS
ALPHA10_PERFORMANCE_REVALIDATION=PENDING
```

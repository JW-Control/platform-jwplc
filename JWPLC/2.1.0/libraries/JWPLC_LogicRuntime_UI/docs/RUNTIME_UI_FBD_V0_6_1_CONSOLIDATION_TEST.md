# Prueba de consolidación — motor v2 y TFT adaptativo

## Estado

```text
CANDIDATA / PENDIENTE DE COMPILACIÓN Y VALIDACIÓN FÍSICA
```

Esta prueba se ejecuta después de la aprobación del anti-parpadeo V14 y antes de añadir eliminación o nuevos tipos de bloque.

## Objetivos

1. Confirmar que el editor utiliza el contrato explícito v2.
2. Confirmar que la UI ya no duplica la semántica de `OPEN`, `HI`, `LO` y negación.
3. Confirmar que solo se instancia la fachada activa `RuntimeUIFBDMap`.
4. Reducir adquisiciones del bus TFT en mapa/detalle estático.
5. Mantener respuesta rápida de botonera durante asistentes y editores.

## Archivos principales

```text
JWPLC_LogicRuntime/src/JWPLC_LogicRuntime_V2.h
JWPLC_LogicRuntime/src/experimental/LogicV2EnginePrototype.h
JWPLC_LogicRuntime/src/experimental/LogicV2EngineInspection.cpp
JWPLC_LogicRuntime/docs/LOGIC_RUNTIME_V2_CONTRACT.md

JWPLC_LogicRuntime_UI/src/model/RuntimeUIV2ReadModel.h
JWPLC_LogicRuntime_UI/src/model/RuntimeUIV2ReadModel.cpp
JWPLC_LogicRuntime_UI/src/edit/RuntimeUIV2EditSession.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMap.h
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMap.cpp
JWPLC_LogicRuntime_UI/src/screens/RuntimeUIFBDMapV14RefreshPolicy.cpp
JWPLC_LogicRuntime_UI/src/JWPLC_LogicRuntime_UI.h
```

## Contrato esperado

```text
Runtime v1: #include <JWPLC_LogicRuntime.h>
Motor v2:   #include <JWPLC_LogicRuntime_V2.h>

LogicV2BlockRecord = 12 bytes
LogicV2InputLink   = 2 bytes
Contrato v2        = 1.0
Esquema de record  = 1
```

## Política TFT esperada

| Pantalla | Periodo esperado |
|---|---:|
| Mapa FBD normal | 100 ms |
| Detalle normal | 100 ms |
| Nodo `+` | 40 ms |
| Selector de tipo | 40 ms |
| Configurar bloque | 40 ms |
| Editor de fuente/entrada | 40 ms |
| Editor TON | 40 ms |

La política se aplica desde la fachada `RuntimeUIFBDMap`; ninguna revisión `Vx` debe instanciarse desde `JWPLC_LogicRuntime_UIClass`.

## Compilación

- [ ] Compila desde checkout limpio.
- [ ] Se compila `LogicV2EngineInspection.cpp`.
- [ ] Se compila `RuntimeUIFBDMap.cpp`.
- [ ] Se compila `RuntimeUIFBDMapV14RefreshPolicy.cpp`.
- [ ] No aparece símbolo duplicado de `LogicV2EnginePrototype::inputValue`.
- [ ] No aparece acceso privado/protegido desde la fachada activa.
- [ ] `JWPLC_LogicRuntime_UI.h` instancia `RuntimeUIFBDMap`, no `RuntimeUIFBDMapV14`.
- [ ] El sketch continúa validando y arrancando el programa RAM.

## Semántica de entradas

Usar el programa actual y recorrer bloques con entradas:

- [ ] `Trg` refleja el mismo valor que consume el TON.
- [ ] Una entrada negada muestra el valor posterior a la negación.
- [ ] `HI` siempre se muestra activo.
- [ ] `LO` siempre se muestra inactivo.
- [ ] `OPEN` en AND/NAND actúa como neutral verdadero.
- [ ] `OPEN` en OR/NOR/XOR actúa como neutral falso.
- [ ] El valor mostrado por la UI coincide con la salida calculada del bloque.

## Mapa y detalle a 10 Hz

- [ ] Entrar al mapa y dejarlo inmóvil durante 15 s.
- [ ] No aparece parpadeo ni limpieza completa.
- [ ] La selección responde con una latencia máxima aproximada de 100 ms.
- [ ] Los cambios lógicos visibles se actualizan correctamente.
- [ ] Entrar a DETALLE y dejar TON en reposo durante 15 s.
- [ ] `T` y `Ta` permanecen estáticos, sin parpadeo.
- [ ] Durante temporización, `Ta` progresa con resolución visual cercana a 100 ms.

## Asistentes y editores a 25 Hz

- [ ] Abrir el nodo `+`; la navegación sigue fluida.
- [ ] Abrir selector de tipo; UP/DOWN no pierde pulsaciones.
- [ ] Abrir CONFIGURAR; LEFT/RIGHT no pierde pulsaciones.
- [ ] Mantener UP/DOWN en un campo TON; el repeat conserva aceleración útil.
- [ ] Editar fuente y lógica; no se siente una pausa de 100 ms entre repeats.
- [ ] Salir de los editores; mapa/detalle vuelven a la frecuencia estática.

## Regresión visual

- [ ] El nodo `+` no vuelve a parpadear a pantalla completa.
- [ ] El panel TON no vuelve a usar el renderer histórico.
- [ ] `T` y `Ta` conservan la misma base.
- [ ] El marco amarillo se ve completo.
- [ ] El encabezado `EDITAR T / Bxx TON / RUN` no se traslapa.
- [ ] `ESC` conserva la navegación jerárquica aprobada.

## Criterio de cierre

```text
Compilación:            APROBADA / FALLA
Semántica de entradas:  APROBADA / FALLA
Mapa/detalle 100 ms:    APROBADO / FALLA
Editores 40 ms:         APROBADOS / FALLA
Regresión visual:       APROBADA / FALLA
Decisión:               CONSOLIDACIÓN APROBADA / REQUIERE CORRECCIÓN
```

Solo después de aprobar esta prueba se habilitará la UI de `ELIMINAR BLOQUE`.

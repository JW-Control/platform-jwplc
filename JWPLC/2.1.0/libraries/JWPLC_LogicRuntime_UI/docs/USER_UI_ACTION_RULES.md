# Reglas de acciones para pantallas USER del Logic Runtime

## Objetivo

Separar estrictamente:

```text
interacción gráfica
≠
operación del runtime o almacenamiento
```

Estas reglas aplican a todas las vistas de `JWPLC_LogicRuntime_UI` que ejecuten acciones:

```text
RuntimeUIProgram
RuntimeUIBlocks
RuntimeUIStorage
RuntimeUIDiagnostics
RuntimeUIBlockEditor
RuntimeUIConfirmDialog
```

## Regla principal

Una pantalla USER nunca debe ejecutar directamente dentro de su callback:

- acceso a FRAM;
- acceso a SD;
- operaciones Ethernet;
- guardado o rollback de programas;
- restauración o guardado retentivo;
- acciones que puedan bloquear;
- operaciones largas de validación o codec.

Durante el callback gráfico, `JWPLC_Display` ya mantiene adquirido y configurado el bus SPI para la TFT.

Ejecutar FRAM o SD desde ese punto puede provocar:

- reentrada o espera innecesaria del lock SPI;
- latencia gráfica;
- bloqueo;
- corrupción por cambio de configuración del bus;
- interferencia con el scan lógico.

## Flujo aprobado

```text
1. El usuario pulsa OK.
2. La vista registra una solicitud pequeña.
3. El callback termina.
4. JWPLC_Display libera el bus SPI de la TFT.
5. JWPLC_LogicRuntime_UI.update() procesa la solicitud desde loop().
6. Se registra un resultado pequeño.
7. El siguiente refresh actualiza solo los campos dinámicos afectados.
```

## Solicitudes

Las solicitudes deben representarse mediante enums pequeños:

```cpp
enum class RuntimeUIProgramAction : uint8_t
{
    None = 0,
    Prepare,
    Start,
    Stop
};
```

La vista expone una operación consumible:

```cpp
RuntimeUIProgramAction takeRequestedAction();
```

Después de leerla debe volver a `None`.

No se usan:

- punteros temporales;
- referencias a widgets;
- callbacks dinámicos;
- objetos pesados en colas;
- cadenas compartidas entre tareas.

## Resultados

El resultado también se representa mediante un enum pequeño:

```cpp
enum class RuntimeUIProgramFeedback : uint8_t
{
    None = 0,
    PrepareActive,
    PrepareFallback,
    PrepareUnformatted,
    StartOk,
    StartFailed,
    StopOk
};
```

La vista convierte el enum en texto únicamente durante su siguiente callback gráfico.

Esto evita escribir en la TFT desde `loop()` y evita compartir buffers de texto entre contextos.

## Trabajo permitido dentro del callback gráfico

```text
leer estados simples del runtime
comparar contra caché
registrar solicitud de un byte
actualizar campos de la TFT
procesar botonera
cambiar de vista
```

## Trabajo permitido desde JWPLC_LogicRuntime_UI.update()

```text
prepareStoredProgram()
restoreStoredRetentiveState()
saveStoredRetentiveState()
start()
stop()
rollback()
guardar programa
operaciones de FRAM o SD
```

Cada acción debe documentar explícitamente si escribe memoria persistente.

## Política de cancelación

Si el usuario sale de USER antes de que `loop()` procese la solicitud:

```text
la acción pendiente se cancela
```

No debe ejecutarse una orden de RUN, STOP, guardar o borrar después de abandonar la pantalla que la originó.

Al salir de una vista se limpian:

- solicitudes pendientes;
- mensajes temporales;
- feedback no consumido;
- confirmaciones parciales.

## Navegación

Cambiar entre vistas puede realizarse dentro del callback porque solo implica dibujo de TFT y estado local de la UI.

Ejemplo:

```text
HOME -> PROGRAMA
PROGRAMA -> HOME
```

Las acciones persistentes o de control siguen siendo diferidas.

## RUN y scan

La interfaz puede solicitar:

```cpp
runtime.start();
```

Pero no se apropia del ciclo de scan.

El sketch conserva:

```cpp
void loop()
{
    JWPLC_LogicRuntime_UI.update();

    if (runtime.state() == JWPLCLogicRuntimeState::Running)
    {
        runtime.tick();
    }
}
```

Esto mantiene visible y explícito el control normal Arduino.

## Confirmaciones para acciones destructivas

Las siguientes acciones requerirán confirmación independiente antes de implementarse:

- formatear almacenamiento;
- sobrescribir un programa;
- borrar programa;
- rollback que cambie el programa activo;
- limpiar retentivos;
- restaurar valores de fábrica.

Patrón previsto:

```text
seleccionar acción
→ pantalla o diálogo de confirmación
→ cuenta regresiva cuando corresponda
→ solicitud diferida
→ feedback
```

## Criterios de revisión

Antes de aprobar una acción nueva:

```text
[ ] El callback gráfico solo registra la solicitud.
[ ] La operación real ocurre desde update().
[ ] No se accede a FRAM/SD/Ethernet con el bus TFT adquirido.
[ ] La solicitud se consume una sola vez.
[ ] Salir de USER cancela la solicitud pendiente.
[ ] El resultado vuelve como enum pequeño.
[ ] La TFT solo se actualiza en el callback siguiente.
[ ] Se documenta si la acción escribe memoria persistente.
[ ] Las acciones destructivas tienen confirmación.
[ ] runtime.tick() permanece explícito en el sketch.
```

## Estado

```text
NORMA APROBADA PARA VISTAS POSTERIORES
PRIMERA IMPLEMENTACIÓN: RuntimeUIProgram v0.2
```
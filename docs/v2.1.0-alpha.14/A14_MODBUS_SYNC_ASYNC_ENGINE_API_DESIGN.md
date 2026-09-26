# Alpha14 — diseño objetivo de motor SYNC / ASYNC para Modbus TCP y RTU

## Estado

**DISEÑO OBJETIVO / PENDIENTE DE IMPLEMENTACION Y VALIDACION.**

No modificar las semanticas legacy de 2.1.x sin un gate de compatibilidad.

## Motivacion

El usuario de JWPLC no deberia tener que memorizar dos familias completas de nombres como:

```cpp
requestReadHoldingRegisters(...)
readHoldingRegistersSync(...)
```

si la diferencia principal es el motor de ejecucion.

El objetivo de API es poder seleccionar el motor una vez y mantener nombres funcionales estables:

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
JWPLC_ModbusTCP.motor(ASYNC);
```

o, cuando el bloqueo sea deliberadamente aceptable:

```cpp
JWPLC_ModbusRTU.motor(SYNC);
JWPLC_ModbusTCP.motor(SYNC);
```

y luego usar nombres equivalentes para las operaciones Modbus.

## Regla arquitectonica

El motor interno canonico debe ser **cooperativo / no bloqueante**.

El modo SYNC no debe implementar un segundo parser o segundo protocolo. Debe ser solamente una envoltura que:

1. inicia la misma transaccion cooperativa;
2. llama al mismo `task()/poll()`;
3. espera hasta estado terminal o timeout;
4. devuelve el resultado final.

Este patron ya existe actualmente en `JWPLC_ModbusRTU`: los metodos `...Sync()` usan el mismo motor `request... + task()`.

## Default recomendado

```text
ASYNC = default recomendado para runtime PLC
SYNC  = opt-in explicito
```

El bloqueo no es una ventaja en si mismo. SYNC existe por simplicidad de flujo cuando la aplicacion puede aceptar perder capacidad de respuesta temporalmente.

## Cuando SYNC puede ser razonable

- commissioning;
- herramientas de diagnostico;
- sketches de prueba;
- comandos manuales de mantenimiento;
- configuracion one-shot antes de arrancar control critico;
- scripts donde la legibilidad secuencial importa mas que la latencia;
- equipos detenidos/en modo seguro donde no hay I/O criticas que atender durante la espera.

No usar SYNC como opcion recomendada dentro del scan normal de PLC, Remote I/O, HMI activa, datalog, watchdog de proceso o control concurrente.

## Master/client vs Slave/server

La seleccion SYNC/ASYNC aplica principalmente al **Master/Client**, donde una llamada inicia una transaccion y espera respuesta.

El **Slave/Server** debe permanecer cooperativo y ser atendido periodicamente; no existe una razon util para convertir el servicio normal del servidor en una espera bloqueante.

## Riesgo principal: significado del retorno

Usar exactamente el mismo metodo con retorno `bool` puede introducir una ambiguedad peligrosa:

- en SYNC, `true` normalmente significa transaccion terminada con exito;
- en ASYNC, `true` solo podria significar solicitud aceptada/iniciada.

Por tanto, no se debe cambiar silenciosamente el significado de los metodos legacy de 2.1.x.

## Compatibilidad 2.1.x

Actualmente RTU ya expone:

```cpp
requestReadHoldingRegisters(...)
requestWriteSingleRegister(...)
task()

readHoldingRegistersSync(...)
writeSingleRegisterSync(...)
```

y conserva wrappers legacy bloqueantes como:

```cpp
readHoldingRegisters(...)
writeSingleRegister(...)
```

Esos nombres/semanticas ya pueden existir en sketches publicados.

Regla:

> no convertir en ASYNC un metodo legacy bloqueante solo porque se agregue `motor(ASYNC)`.

## Estrategia de migracion propuesta

### Fase 1 — 2.1.x

- mantener APIs existentes;
- implementar internamente TCP sobre motor cooperativo;
- conservar wrappers SYNC;
- introducir el concepto de execution motor sin alterar llamadas legacy;
- documentar ASYNC como camino recomendado para codigo nuevo.

### Fase 2 — nueva fachada estable

Definir una fachada comun TCP/RTU donde la seleccion de motor se realice una sola vez.

La fachada debe resolver de forma explicita el estado de una operacion ASYNC mediante estado/resultado, no mediante un `bool` ambiguo.

Posibles estados:

```text
REJECTED
PENDING
SUCCESS
ERROR
```

La forma final de esta fachada requiere un gate de compatibilidad fuente/ABI antes de adoptarse.

## Objetivo de experiencia de usuario

Deseado conceptualmente:

```cpp
JWPLC_ModbusRTU.motor(ASYNC);
JWPLC_ModbusRTU.readHoldingRegisters(...);
```

y:

```cpp
JWPLC_ModbusTCP.motor(ASYNC);
JWPLC_ModbusTCP.readHoldingRegisters(...);
```

sin obligar a agregar el sufijo `Async` a cada operacion.

La API debe mantener claro, sin embargo, cuándo los datos ya estan disponibles.

## Relacion con Alpha14

Alpha14 esta limpiando primero las rutas bloqueantes de Ethernet/W5500:

- NB1: lifecycle TCP;
- NB2: DNS;
- NB3: UDP/socket/backend.

No se debe mezclar esta limpieza interna con un cambio publico de API Modbus hasta que el motor TCP cooperativo este calificado.

Al cerrar el backend, esta propuesta se reutilizara tanto para Modbus TCP como para Modbus RTU.

# Prueba física — motor v2 en RAM

## Objetivo

Validar un motor de ejecución experimental que reserve las capacidades reales del perfil Basic:

```text
100 bloques
512 enlaces variables
```

La prueba mide el coste RAM de mantener copias profundas de bloques, enlaces y valores antes de integrar el modelo v2 en `JWPLC_LogicRuntime`.

## Componentes

```text
src/experimental/LogicV2EnginePrototype.h
src/experimental/LogicV2EnginePrototype.cpp
```

El motor implementa:

```text
loadProgram()
start()
scan()
stop()
unloadProgram()
blockValue()
blockDefinition()
inputLink()
```

## Aislamiento

Todavía no modifica:

```text
LogicEngine v1
JWPLC_LogicRuntime estable
codec v1
FRAM
program store A/B
retentivos
interfaz FBD
```

No conoce salidas físicas ni periféricos.

## Sketch

```text
Archivo
→ Ejemplos
→ JWPLC_LogicRuntime
→ JWPLC_LogicRuntime_V2_Engine_RAM
```

No requiere comandos por Serial.

## Seguridad

```text
No llama JWPLC_LogicRuntime::begin()
No inicializa E/S desde el sketch
No contiene DigitalOutput
No conmuta Q0
No abre ni escribe FRAM
No formatea almacenamiento
```

## Validaciones

### Capacidad y memoria

```text
perfil compilado: 100 bloques / 512 enlaces
LogicV2BlockRecord: 12 bytes
LogicV2InputLink: 2 bytes
valores booleanos: 100 bytes
```

El sketch imprime:

```text
sizeof motor v2
almacen mínimo de arreglos
```

El overhead administrativo debe ser menor o igual a 64 bytes.

### Carga y copia profunda

Se comprueba que:

- el motor copia bloques y enlaces a almacenamiento propio;
- modificar los arreglos fuente después de cargar no cambia el programa interno;
- una carga inválida descarga el programa anterior y deja `FAULT`;
- los errores detallados del validador se conservan.

### Ciclo operativo

```text
EMPTY
→ loadProgram()
→ READY
→ start()
→ RUNNING
→ scan()
→ stop()
→ STOPPED
```

`scan()` se rechaza antes de `start()`, después de `stop()` y cuando el perfil de entradas es insuficiente.

### Ejecución

Se repiten los patrones aprobados en la prueba anterior:

```text
todas las entradas FALSE
patrón mixto
las ocho entradas TRUE
```

Se verifican AND4, AND8, negación por pin, XOR y contador de scans.

### Capacidad máxima

Se construye temporalmente en la pila un programa válido de:

```text
100 bloques
512 enlaces
64 compuertas AND de 8 entradas
```

La fuente temporal no aumenta la RAM global permanente. El motor debe cargarla, ejecutarla y conservar los 512 enlaces en su copia interna.

## Resultado esperado

```text
Resultado: 57 PASS, 0 FAIL
MOTOR V2 EN RAM: PASS
```

## Métricas requeridas

```text
Flash utilizada
RAM global utilizada
RAM restante
sizeof motor v2 impreso por Serial
almacén mínimo de arreglos impreso por Serial
```

La prueba anterior sin motor global utilizó:

```text
RAM global: 27916 bytes
```

La diferencia de RAM global permitirá corroborar el coste real del objeto de motor v2.

## Criterio de avance

Con PASS físico:

1. cerrar el presupuesto RAM Basic;
2. crear un adaptador de programas v1 al modelo RAM v2;
3. validar que los programas históricos producen exactamente los mismos resultados;
4. diseñar la imagen persistente y codec v2;
5. repetir A/B, cortes y FRAM física;
6. retomar el mapa FBD estable con puertos variables.
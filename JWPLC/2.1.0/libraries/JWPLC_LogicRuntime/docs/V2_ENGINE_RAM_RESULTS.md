# Resultados físicos — motor v2 en RAM

## Estado

```text
MOTOR V2 EN RAM: VALIDADO EN HARDWARE
RESULTADO: 57 PASS, 0 FAIL
FRAM: NO UTILIZADA
E/S: NO INICIALIZADA POR EL SKETCH
SALIDAS Q0: NO CONMUTADAS
```

## Compilación validada

```text
Flash:       423937 bytes / 3145728 bytes (13 %)
RAM global:   30268 bytes / 327680 bytes (9 %)
RAM restante: 297412 bytes
Puerto: COM4
```

## Tamaño físico del motor

```text
sizeof LogicV2EnginePrototype: 2348 bytes
almacén mínimo de arreglos:    2324 bytes
overhead administrativo:         24 bytes
```

La prueba anterior, sin una instancia global del motor v2, utilizó:

```text
RAM global: 27916 bytes
```

Diferencia medida:

```text
30268 - 27916 = 2352 bytes
```

La diferencia supera `sizeof(LogicV2EnginePrototype)` en solo cuatro bytes, coherente con alineación o datos auxiliares del sketch. El presupuesto real del motor Basic queda cerrado en aproximadamente 2,35 KiB de RAM global.

## Capacidad validada

Se confirmó físicamente:

- perfil compilado de 100 bloques;
- reserva de 512 enlaces;
- copia profunda de bloques y enlaces;
- 12 bytes por descriptor de bloque;
- 2 bytes por enlace;
- ciclo `EMPTY -> READY -> RUNNING -> STOPPED`;
- rechazo de scans antes de `start()` y después de `stop()`;
- rechazo de perfil de entradas insuficiente;
- AND de cuatro y ocho entradas;
- negación individual por pin;
- XOR por paridad;
- carga y scan de un programa máximo de 100 bloques y 512 enlaces;
- descarga segura al intentar cargar capacidades inválidas;
- conservación del error detallado del validador.

## Decisión

```text
PRESUPUESTO RAM BASIC: APROBADO
100 BLOQUES / 512 ENLACES: APROBADO
COPIA PROFUNDA: APROBADA
CICLO OPERATIVO V2: APROBADO
SUSTITUCIÓN DEL MOTOR V1: TODAVÍA NO
```

Todavía permanecen intactos:

```text
LogicEngine v1
JWPLC_LogicRuntime público
codec v1
program store A/B
retentivos
mapa de FRAM
interfaz FBD estable
```

## Siguiente fase

Crear un adaptador de programas v1 al modelo RAM v2 y comprobar que el subconjunto combinacional histórico:

```text
DigitalInput
NOT
AND de dos entradas
OR de dos entradas
```

produce exactamente los mismos resultados bajo ambos modelos.

`DigitalOutput`, `SET/RESET` y `TON` se incorporarán en fases posteriores, antes de declarar compatibilidad completa v1 -> v2.
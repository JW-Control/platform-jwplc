# Resultado de validación — perfil compilado de 100 bloques

## Estado

```text
VALIDADO EN HARDWARE
Resultado: 10 PASS, 0 FAIL
PERFIL COMPILADO 100 BLOQUES: PASS
```

## Compilación física

```text
Flash:       420065 bytes / 3145728 bytes (13 %)
RAM global:   32612 bytes / 327680 bytes (9 %)
RAM restante: 295068 bytes
```

## Límites confirmados

```text
Bloques compilados:           100
FRAM 8 KiB — límite efectivo: 100
FRAM 32 KiB — límite efectivo: 100
FRAM 8 KiB — físico:          100
FRAM 32 KiB — físico:         400
```

El build actual reserva RAM para 100 bloques. El mapa y el formato persistente mantienen la capacidad física futura de 400 bloques para FRAM de 32 KiB.

## Tamaños medidos

```text
sizeof(LogicBlockDefinition): 12
sizeof(LogicBlockState):       8
sizeof(LogicProgramBuffer):    1256
sizeof(LogicEngine):           2052
sizeof(JWPLCLogicStorage):     2588
sizeof(JWPLC_LogicRuntime):    4688
```

## Validaciones realizadas

- La constante pública conserva el valor de 100 bloques.
- El perfil de 8 KiB mantiene su límite funcional de 100 bloques.
- El perfil físico de 32 KiB conserva capacidad de 400 bloques.
- El build actual limita de forma segura ambos perfiles a 100 bloques.
- `LogicProgramBuffer` reserva exactamente la capacidad compilada.
- La imagen de 100 bloques cabe en el slot de 8 KiB.
- La imagen de 400 bloques sigue cabiendo físicamente en el slot de 32 KiB.
- La selección de perfil para 8192 bytes devuelve 100 bloques.
- Capacidades menores a 8 KiB continúan rechazadas.
- La prueba no inicializó E/S ni accedió a la FRAM.

## Conclusión

La separación entre capacidad física persistente y capacidad compilada en RAM queda validada. El build predeterminado del JWPLC Basic usa 100 bloques sin alterar el formato `JWLR`, el mapa FRAM v1 ni la API pública.

Antes de cerrar definitivamente la optimización se repetirá `JWPLC_LogicRuntime_Stored_Program_Integration` con este build para comprobar la ausencia de regresiones y obtener una comparación directa de consumo frente al mismo ejemplo compilado previamente para 400 bloques.

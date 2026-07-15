# Modelo de bloques compatible con LOGO! — propuesta JWPLC Runtime v2

## Estado de decisión

```text
LOGO! COMO REFERENCIA DE FAMILIARIDAD: APROBADO
COPIA LITERAL DE LOGO!: NO
BLOQUES LÓGICOS LIMITADOS A DOS ENTRADAS: DESCARTADO
ENTRADAS VARIABLES POR BLOQUE: APROBADO
NEGACIÓN INDIVIDUAL POR PIN: APROBADA
FORMATO PERSISTENTE V1: CONSERVAR LECTURA
FORMATO PERSISTENTE V2: NECESARIO PARA EL NUEVO MODELO
```

El objetivo es que una persona familiarizada con Siemens LOGO! reconozca inmediatamente las familias de bloques, símbolos y conceptos de programación, mientras JWPLC mejora la navegación, la capacidad de entradas y la representación sobre la TFT a color.

No se utilizarán marcas, iconos propietarios ni nombres comerciales como identidad de la interfaz. La compatibilidad buscada es conceptual y de experiencia de uso.

## Motivo del cambio

El modelo actual del runtime define cada bloque con únicamente:

```text
sourceA
sourceB
```

Esto permite AND/OR de dos entradas, pero no representa de forma natural:

- funciones básicas de cuatro entradas;
- cantidad variable de entradas;
- negación individual por pin;
- entradas con roles específicos en funciones especiales;
- un editor FBD similar al que esperan usuarios de relés programables.

El mapa FBD estable no debe consolidarse sobre una estructura de dos entradas que ya se sabe transitoria.

## Referencia funcional de LOGO!

Se adoptan como punto de partida cuatro grupos reconocibles:

```text
Co  Conectores, constantes y bornes
GF  Funciones básicas
SF  Funciones especiales
JW  Funciones ampliadas propias de JWPLC
```

La interfaz puede usar nombres en español y abreviaturas técnicas, manteniendo símbolos universales como `&`, `>=1`, `=1`, `!`, `S/R`, `TON` y `TOF`.

## Catálogo inicial

### Conectores — Co

```text
ENTRADA DIGITAL       I0.x
SALIDA DIGITAL        Q0.x
CONSTANTE ALTA        HI / 1
CONSTANTE BAJA        LO / 0
ENTRADA ABIERTA       X
MARCA INTERNA         Mx        futura
TECLA DE PANEL        Kx        futura
```

Los conectores analógicos y de red se incorporarán únicamente cuando exista una fuente real soportada por el hardware o las expansiones.

### Funciones básicas — GF

Primera compatibilidad:

```text
AND
AND con flanco
NAND
NAND con flanco
OR
NOR
XOR
NOT
```

Reglas:

- AND, NAND, OR y NOR usan entradas variables;
- la configuración inicial al insertar será de cuatro pines para resultar familiar;
- el usuario podrá reducir o ampliar la cantidad de pines;
- NOT se conserva como bloque explícito aunque exista negación por pin;
- las variantes con evaluación de flanco se representan como tipos de bloque separados;
- cada pin puede negarse de forma individual.

### Funciones especiales digitales — SF

Orden de implementación recomendado:

#### Fase SF-1

```text
TON       retardo a la conexión
TOF       retardo a la desconexión
TON/TOF   retardo combinado
TONR      retardo retentivo
S/R       relé autoenclavador
IMP       relé de impulsos
PULSE     salida de impulso
EDGE      pulso disparado por flanco
CLOCK     generador asíncrono
CTUD      contador avance/retroceso
SHIFT     registro de desplazamiento
```

#### Fase SF-2

```text
RANDOM        generador aleatorio
STAIR         alumbrado de escalera
COMFORT       interruptor confortable
WEEK          temporizador semanal
YEAR          temporizador anual
HOURS         contador de horas
THRESHOLD     interruptor de frecuencia/umbral
SOFT_SWITCH   interruptor de software
MESSAGE       texto de aviso
```

#### Fase SF-3, dependiente de recursos analógicos

```text
AI_THRESHOLD
AI_DIFFERENTIAL
AI_COMPARE
AI_MONITOR
AI_AMPLIFIER
```

Estos bloques pueden existir en el catálogo del formato, pero la UI solo debe ofrecerlos cuando el perfil de hardware declare recursos analógicos disponibles.

### Funciones propias — JW

Se definirán después de cerrar el núcleo compatible:

```text
Modbus / registros
RS-485 y comunicaciones
alarmas industriales
recetas o secuencias
control de salidas rápidas
bloques de periféricos JWPLC
```

No deben mezclarse prematuramente con GF/SF. La separación permite que el usuario de LOGO! empiece en terreno conocido.

## Modelo de entradas variables

### Política de producto

```text
entradas iniciales GF: 4
máximo inicial por bloque: 8
cantidad almacenada: solo entradas realmente usadas
presupuesto global Basic 8 KiB: 512 enlaces
presupuesto global perfil 32 KiB: 2048 enlaces
```

El formato no debe quedar limitado estructuralmente a ocho entradas. El límite de ocho será una política del validador y de la UI v1, revisable sin volver a cambiar el registro persistente.

Un programa de 100 bloques dispone así de un promedio de 5,12 enlaces por bloque. Entradas, constantes, salidas y bloques de una sola fuente consumen menos, por lo que las compuertas de cuatro u ocho entradas caben con margen práctico.

## Negación individual

Cada enlace conserva un bit de inversión:

```text
fuente normal   B03
fuente negada  !B03
entrada física  I0.2
entrada negada !I0.2
```

Representación gráfica:

```text
pin normal   línea directa
pin negado   burbuja en el pin de entrada
```

La negación pertenece al enlace, no al bloque fuente. Una misma salida puede conectarse normalmente a un consumidor y negada a otro.

## Semántica de entradas abiertas

La entrada `X` debe usar el elemento neutro de cada función:

```text
AND / NAND       X = 1
OR / NOR / XOR   X = 0
```

En funciones especiales, una entrada abierta adopta la semántica documentada por el tipo, normalmente `0`. El validador debe rechazar una entrada abierta cuando el pin sea obligatorio para una operación segura.

## Roles de los pines

El orden de enlaces determina el rol según el tipo:

```text
AND / OR / NAND / NOR   IN1..INn
S/R                     S, R
TON / TOF               TRG, R opcional
CTUD                    CU, CD, R, LOAD/EN según versión
SHIFT                   IN, CLK, DIR, R
```

El rol no se almacena repetido en cada enlace. Está definido por el descriptor del tipo de bloque, evitando gasto innecesario.

## Propuesta de formato v2

### Descriptor de bloque persistente

Se conserva un descriptor compacto de 12 bytes:

```cpp
struct LogicBlockRecordV2
{
    uint8_t  type;
    uint8_t  flags;
    uint16_t firstInput;
    uint8_t  inputCount;
    uint8_t  reserved;
    uint16_t resource;
    uint32_t parameter;
};
```

```text
Tamaño: 12 bytes
```

Por tanto, ampliar las entradas no obliga a inflar todos los bloques.

### Enlace de entrada

Cada entrada ocupa dos bytes:

```cpp
struct LogicInputLinkV2
{
    uint16_t encodedSource;
};
```

Codificación propuesta:

```text
bit 15       negación individual
bits 0..14   índice de fuente o valor especial
```

El rango permite sobradamente los perfiles actuales de 100 y 400 bloques.

Valores especiales se reservarán para:

```text
OPEN
CONST_FALSE
CONST_TRUE
```

El bloque almacena `firstInput` e `inputCount`, apuntando a una sección contigua de enlaces.

### Imagen de programa

```text
cabecera v2
registros de bloques
registros de enlaces
CRC de payload
```

La cabecera debe añadir:

```text
inputLinkCount
inputLinkRecordSize
```

El cálculo de tamaño será:

```text
header + blockCount * 12 + inputLinkCount * 2
```

Para Basic 8 KiB:

```text
64 + 100*12 + 512*2 = 2288 bytes
payload disponible por slot = 2528 bytes
margen = 240 bytes
```

Para el perfil 32 KiB:

```text
64 + 400*12 + 2048*2 = 8960 bytes
payload disponible por slot = 12256 bytes
margen = 3296 bytes
```

Los presupuestos caben sin modificar el mapa físico A/B aprobado.

## Compatibilidad

### Programas v1

Deben seguir cargando:

```text
sourceA -> enlace 0
sourceB -> enlace 1
flags   -> se conservan
```

El loader convierte la imagen v1 al modelo RAM v2. No se reescribe automáticamente la FRAM.

### Escritura nueva

Los programas editados con entradas variables se guardarán como imagen v2. No se publicará el formato v2 hasta validar:

- codec;
- cálculo de tamaño;
- CRC;
- rollback A/B;
- cortes simulados;
- FRAM física reversible;
- integración de runtime;
- retentivos;
- UI FBD.

## Impacto estimado en RAM

Para Basic de 100 bloques:

```text
512 enlaces * 2 bytes = 1024 bytes por arreglo de enlaces
```

La arquitectura actual mantiene varias copias del programa durante carga, codec y ejecución. El incremento real podría situarse aproximadamente entre 3 y 4 KiB si cada buffer mantiene su propio arreglo completo. Debe medirse físicamente y optimizarse antes de congelar el diseño.

No se asumirá el consumo final hasta compilar los perfiles de 100 y 400 bloques.

## Ejecución

Los bloques básicos recorren su rango de enlaces:

```text
AND   inicia TRUE  y combina todas las entradas
OR    inicia FALSE y combina todas las entradas
NAND  NOT(AND)
NOR   NOT(OR)
XOR   paridad de entradas activas
```

Antes de combinar cada entrada:

```text
valor = fuente
si enlace.inverted -> valor = !valor
```

La evaluación continúa en orden de bloques. Toda fuente normal debe apuntar a un bloque anterior, manteniendo el grafo acíclico y determinista.

## Consecuencia para el mapa FBD

El mapa v0.4 debe diseñarse con puertos variables desde el inicio:

```text
┌──────────┐
I1 o───────┤1         │
I2 ────────┤2   >=1   ├──── Q
I3 ────────┤3         │
X  ────────┤4         │
           └──────────┘
```

Reglas visuales:

- puertos visibles en el lateral izquierdo;
- burbuja para negación;
- bloques compactos con símbolo central;
- pines adicionales desplazables cuando excedan la altura disponible;
- mapa estable y navegación por conexiones;
- detalle separado para parámetros y lista completa de entradas.

No conviene terminar el layout FBD sobre `sourceA/sourceB`, porque obligaría a rehacer el renderer y el navegador inmediatamente después.

## Orden de trabajo recomendado

```text
1. cerrar catálogo y descriptor de tipos
2. prototipo RAM de enlaces variables
3. validador GF de 1..8 entradas y negación
4. motor AND/OR/NAND/NOR/XOR/NOT
5. codec v2 con lectura compatible de v1
6. pruebas A/B y FRAM
7. adaptar mapa FBD estable a puertos variables
8. habilitar edición gráfica
9. añadir SF por familias
```

## Criterios de aceptación de la primera fase

```text
[ ] AND de 4 entradas compatible con el modelo mental de LOGO!
[ ] AND/OR configurables entre 2 y 8 entradas
[ ] negación independiente en cada pin
[ ] X usa el elemento neutro correcto
[ ] v1 carga sin cambios de comportamiento
[ ] v2 cabe en los slots actuales
[ ] no se rompe la política de retentivos
[ ] el mapa muestra pines y burbujas claramente
[ ] el usuario navega por el cableado, no por índices
[ ] consumo RAM medido en perfiles 100 y 400
```

# Mapa I/O OpenPLC para JWPLC Basic

Fecha: 2026-05-23  
Etapa: `alpha32-openplc-integration`

## Objetivo

Definir el mapa entre direcciones OpenPLC, Modbus y canales físicos del JWPLC Basic.

## Mapa digital validado

### Entradas digitales

| Canal JWPLC | Dirección OpenPLC | Tipo OpenPLC | Modbus TCP |
|---|---:|---|---:|
| `I0_0` | `%IX0.0` | Digital Input | Discrete Input 0 |
| `I0_1` | `%IX0.1` | Digital Input | Discrete Input 1 |
| `I0_2` | `%IX0.2` | Digital Input | Discrete Input 2 |
| `I0_3` | `%IX0.3` | Digital Input | Discrete Input 3 |
| `I0_4` | `%IX0.4` | Digital Input | Discrete Input 4 |
| `I0_5` | `%IX0.5` | Digital Input | Discrete Input 5 |
| `I0_6` | `%IX0.6` | Digital Input | Discrete Input 6 |
| `I0_7` | `%IX0.7` | Digital Input | Discrete Input 7 |

### Salidas digitales

| Canal JWPLC | Dirección OpenPLC | Tipo OpenPLC | Modbus TCP |
|---|---:|---|---:|
| `Q0_0` | `%QX0.0` | Digital Output | Coil 0 |
| `Q0_1` | `%QX0.1` | Digital Output | Coil 1 |
| `Q0_2` | `%QX0.2` | Digital Output | Coil 2 |
| `Q0_3` | `%QX0.3` | Digital Output | Coil 3 |
| `Q0_4` | `%QX0.4` | Digital Output | Coil 4 |
| `Q0_5` | `%QX0.5` | Digital Output | Coil 5 |
| `Q0_6` | `%QX0.6` | Digital Output | Coil 6 |
| `Q0_7` | `%QX0.7` | Digital Output | Coil 7 |

## Pin Mapping en OpenPLC Editor

La tabla Pin Mapping une:

```txt
Variable lógica OpenPLC <-> Dirección IEC <-> Pin físico del HAL
```

| Columna | Significado |
|---|---|
| `Pin` | Canal físico o simbólico del JWPLC Basic: `I0_0`, `Q0_0`, etc. |
| `Type` | Tipo de señal: Digital Input o Digital Output. |
| `Address` | Dirección IEC usada por OpenPLC: `%IX0.0`, `%QX0.0`, etc. |
| `Name` | Nombre de la variable creada por el usuario en el programa OpenPLC. |

## Campos sugeridos en `hals.json`

```json
"default_ain": "",
"default_aout": "",
"default_din": "I0_0, I0_1, I0_2, I0_3, I0_4, I0_5, I0_6, I0_7",
"default_dout": "Q0_0, Q0_1, Q0_2, Q0_3, Q0_4, Q0_5, Q0_6, Q0_7",
"user_ain": "",
"user_aout": "",
"user_din": "I0_0, I0_1, I0_2, I0_3, I0_4, I0_5, I0_6, I0_7",
"user_dout": "Q0_0, Q0_1, Q0_2, Q0_3, Q0_4, Q0_5, Q0_6, Q0_7"
```

## Variables recomendadas para ejemplos

Entradas:

```txt
I0_0_IN  BOOL  %IX0.0
I0_1_IN  BOOL  %IX0.1
I0_2_IN  BOOL  %IX0.2
I0_3_IN  BOOL  %IX0.3
I0_4_IN  BOOL  %IX0.4
I0_5_IN  BOOL  %IX0.5
I0_6_IN  BOOL  %IX0.6
I0_7_IN  BOOL  %IX0.7
```

Salidas:

```txt
Q0_0_OUT  BOOL  %QX0.0
Q0_1_OUT  BOOL  %QX0.1
Q0_2_OUT  BOOL  %QX0.2
Q0_3_OUT  BOOL  %QX0.3
Q0_4_OUT  BOOL  %QX0.4
Q0_5_OUT  BOOL  %QX0.5
Q0_6_OUT  BOOL  %QX0.6
Q0_7_OUT  BOOL  %QX0.7
```

## Observaciones

- No habilitar AIN/AOUT en esta etapa.
- No usar `Q0_8` ni `Q0_9` para la integración estable actual.
- Usar `Display Format: LED` al probar coils o discrete inputs en ModbusTool.
- En ModbusTool, `Integer` puede mostrar valores como `256` para coils por empaquetamiento interno; no usarlo como referencia visual para señales booleanas.

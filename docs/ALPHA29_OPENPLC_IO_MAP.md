# Alpha29 - Documentación de I/O industrial y mapa preliminar OpenPLC

## Objetivo

Alpha29 no crea un runtime nuevo de I/O.

El I/O industrial del JWPLC Basic ya está integrado de forma nativa en el core mediante el TCA6424A y las funciones estándar de Arduino:

```cpp
pinMode()
digitalRead()
digitalWrite()
```

El objetivo de alpha29 es:

1. Documentar claramente el I/O industrial nativo.
2. Crear ejemplos de uso con `I0_x` y `Q0_x`.
3. Dejar un mapa preliminar para OpenPLC.
4. Corregir el README principal para que no parezca que el TCA está pendiente.
5. Preparar el camino para la siguiente etapa de integración OpenPLC.

---

## Estado actual del I/O

El core intercepta las funciones tipo Arduino y decide si el pin pertenece al ESP32 o al expansor TCA6424A.

Conceptualmente:

```txt
pinMode(pin, mode)
digitalRead(pin)
digitalWrite(pin, value)
        │
        ├── GPIO normal ESP32 -> core ESP32
        │
        └── Pin virtual 0x22xx -> TCA6424A
```

Esto permite que el usuario escriba:

```cpp
pinMode(Q0_0, OUTPUT);
digitalWrite(Q0_0, HIGH);
```

sin saber que internamente esa salida está detrás del expansor.

---

## Convención de pines virtuales

Los pines industriales del JWPLC Basic usan constantes `uint16_t`.

Ejemplo:

```cpp
static const uint16_t I0_0 = 0x2207;
static const uint16_t Q0_0 = 0x2208;
```

La forma `0x22xx` permite identificar que el pin pertenece al expansor.

---

## Entradas digitales

| Señal | Constante | Pin virtual | Canal TCA |
|---|---|---:|---:|
| Entrada 0 | `I0_0` | `0x2207` | 7 |
| Entrada 1 | `I0_1` | `0x2206` | 6 |
| Entrada 2 | `I0_2` | `0x2205` | 5 |
| Entrada 3 | `I0_3` | `0x2204` | 4 |
| Entrada 4 | `I0_4` | `0x2203` | 3 |
| Entrada 5 | `I0_5` | `0x2202` | 2 |
| Entrada 6 | `I0_6` | `0x2201` | 1 |
| Entrada 7 | `I0_7` | `0x2200` | 0 |

## Salidas digitales tipo relé

| Señal | Constante | Pin virtual | Canal TCA |
|---|---|---:|---:|
| Salida 0 | `Q0_0` | `0x2208` | 8 |
| Salida 1 | `Q0_1` | `0x2209` | 9 |
| Salida 2 | `Q0_2` | `0x220A` | 10 |
| Salida 3 | `Q0_3` | `0x220B` | 11 |
| Salida 4 | `Q0_4` | `0x220C` | 12 |
| Salida 5 | `Q0_5` | `0x220D` | 13 |
| Salida 6 | `Q0_6` | `0x220E` | 14 |
| Salida 7 | `Q0_7` | `0x220F` | 15 |

---

## Uso como Arduino industrial

### Configurar entradas

```cpp
pinMode(I0_0, INPUT);
pinMode(I0_1, INPUT);
pinMode(I0_2, INPUT);
pinMode(I0_3, INPUT);
pinMode(I0_4, INPUT);
pinMode(I0_5, INPUT);
pinMode(I0_6, INPUT);
pinMode(I0_7, INPUT);
```

### Configurar salidas

```cpp
pinMode(Q0_0, OUTPUT);
pinMode(Q0_1, OUTPUT);
pinMode(Q0_2, OUTPUT);
pinMode(Q0_3, OUTPUT);
pinMode(Q0_4, OUTPUT);
pinMode(Q0_5, OUTPUT);
pinMode(Q0_6, OUTPUT);
pinMode(Q0_7, OUTPUT);
```

### Estado seguro recomendado

```cpp
const uint16_t outputs[] = {
    Q0_0, Q0_1, Q0_2, Q0_3,
    Q0_4, Q0_5, Q0_6, Q0_7
};

void setup()
{
    for (uint8_t i = 0; i < 8; i++)
    {
        pinMode(outputs[i], OUTPUT);
        digitalWrite(outputs[i], LOW);
    }
}
```

---

## Ejemplo mínimo: entrada a salida

```cpp
void setup()
{
    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);

    digitalWrite(Q0_0, LOW);
}

void loop()
{
    bool inputState = digitalRead(I0_0);
    digitalWrite(Q0_0, inputState ? HIGH : LOW);
}
```

---

## Mapa preliminar OpenPLC

Esta alpha no integra OpenPLC todavía. Solo deja el mapa recomendado.

### Discrete Inputs

| OpenPLC / Modbus | Dirección base 0 | Señal JWPLC | Descripción |
|---|---:|---|---|
| Discrete Input | 0 | `I0_0` | Entrada digital 0 |
| Discrete Input | 1 | `I0_1` | Entrada digital 1 |
| Discrete Input | 2 | `I0_2` | Entrada digital 2 |
| Discrete Input | 3 | `I0_3` | Entrada digital 3 |
| Discrete Input | 4 | `I0_4` | Entrada digital 4 |
| Discrete Input | 5 | `I0_5` | Entrada digital 5 |
| Discrete Input | 6 | `I0_6` | Entrada digital 6 |
| Discrete Input | 7 | `I0_7` | Entrada digital 7 |

### Coils

| OpenPLC / Modbus | Dirección base 0 | Señal JWPLC | Descripción |
|---|---:|---|---|
| Coil | 0 | `Q0_0` | Salida digital / relé 0 |
| Coil | 1 | `Q0_1` | Salida digital / relé 1 |
| Coil | 2 | `Q0_2` | Salida digital / relé 2 |
| Coil | 3 | `Q0_3` | Salida digital / relé 3 |
| Coil | 4 | `Q0_4` | Salida digital / relé 4 |
| Coil | 5 | `Q0_5` | Salida digital / relé 5 |
| Coil | 6 | `Q0_6` | Salida digital / relé 6 |
| Coil | 7 | `Q0_7` | Salida digital / relé 7 |

### Holding Registers

| Registro | Uso recomendado |
|---:|---|
| HR0 | Estado general / heartbeat |
| HR1 | Comando o modo |
| HR2 | Setpoint 1 |
| HR3 | Setpoint 2 |
| HR4-HR15 | Variables de usuario |

### Input Registers

| Registro | Uso recomendado |
|---:|---|
| IR0 | Bitmap de entradas |
| IR1 | Bitmap de salidas |
| IR2 | Estado de comunicación |
| IR3 | Diagnóstico general |

---

## Tabla de equivalencias Modbus clásica

| Tipo | Base 0 | Estilo clásico | JWPLC |
|---|---:|---:|---|
| Coil | 0 | 00001 | `Q0_0` |
| Coil | 1 | 00002 | `Q0_1` |
| Discrete Input | 0 | 10001 | `I0_0` |
| Discrete Input | 1 | 10002 | `I0_1` |
| Input Register | 0 | 30001 | `IR0` |
| Holding Register | 0 | 40001 | `HR0` |

---

## Relación con alpha28

Alpha28 dejó lista la base:

```txt
JWPLC_RS485
JWPLC_ModbusRTU
```

Alpha29 documenta cómo se conectará ese stack con las entradas/salidas reales:

```txt
OpenPLC / Modbus
        ↓
Mapa I/O
        ↓
pinMode / digitalRead / digitalWrite
        ↓
TCA6424A
        ↓
Entradas y relés físicos
```

---

## Criterio para cerrar alpha29

Alpha29 queda cerrada cuando:

- El README principal refleje que TCA/I/O industrial ya es nativo.
- Existan ejemplos claros de I/O.
- El mapa preliminar OpenPLC/Modbus esté documentado.
- Se valide compilación en JWPLC Basic.
- Se valide al menos un ejemplo con entradas/salidas reales.

# Reporte de validación OpenPLC + JWPLC Basic

Fecha: 2026-05-23  
Etapa: `alpha32-openplc-integration`  
Hardware: JWPLC Basic v2.0.0  
Software: OpenPLC Editor v4 + Arduino CLI

## Resumen de pruebas

| Prueba | Resultado |
|---|---|
| OpenPLC Editor muestra `JWPLC BASIC [2.0.0]` | OK |
| Compilación con Arduino CLI | OK |
| Subida por USB | OK |
| Debugger OpenPLC | OK |
| Pin Mapping `I0_0` / `Q0_0` | OK |
| Blink `Q0_0` desde Ladder | OK |
| TFT refleja estados de E/S | OK |
| Modbus TCP con DHCP | OK |
| JWPLC aparece en router | OK |
| Ping a IP asignada | OK |
| Puerto TCP 502 | OK |
| ModbusTool conecta como master TCP | OK |
| FC01 Read Coils | OK |
| FC02 Read Discrete Inputs | OK |
| Lectura de entradas | OK |
| Activación de salidas | OK |
| Dependencia entrada-salida | OK |
| Desconexión/reconexión RJ45 | OK |
| RTU solo | OK |
| TCP solo | OK |
| RTU + TCP simultáneo | Parcial: TCP OK, RTU no trabaja |

## IP validada

```txt
IP: 192.168.0.50
MAC: DE:AD:BE:EF:DE:AD
Puerto: 502
```

## Validación Modbus TCP

### FC01 - Read Coils

Configuración usada:

```txt
Mode: TCP
IP Address: 192.168.0.50
Port: 502
Slave ID: 0
Function: Read coils
Start Address: 0
Size: 1 / 8
Display Format: LED
Poll: 100 ms
```

Resultado:

```txt
Coil 0 refleja Q0_0.
El blink de Q0_0 se visualiza correctamente.
```

### FC02 - Read Discrete Inputs

Configuración usada:

```txt
Function: Read discrete
Start Address: 0
Size: 8
Display Format: LED
```

Resultado:

```txt
Las entradas digitales se leen correctamente.
```

### Relación entrada-salida

Se validó dependencia entre entrada y salida desde lógica OpenPLC, con concordancia en TFT.

## Observación sobre visualización Integer

En ModbusTool, al leer coils en formato `Integer`, se observó cambio entre `0` y `256`.

Esto no representa error de protocolo. La respuesta Modbus real es booleana:

```txt
0x01 = ON
0x00 = OFF
```

Para coils y discrete inputs se recomienda usar:

```txt
Display Format: LED
```

## Validación de Ethernet

Se validó:

- Arranque con RJ45 conectado.
- Arranque sin RJ45 y conexión posterior.
- Desconexión/reconexión sin cuelgue.
- TFT/E/S/RTC se mantienen operativos.
- LED ETH en TFT responde.

## Pendientes

1. Resolver o documentar RTU + TCP simultáneo.
2. Definir MAC por defecto para uso real.
3. Guardar archivos finales modificados de OpenPLC.
4. Guardar proyecto ejemplo `.openplc` o carpeta de prueba.
5. Preparar guía de instalación para usuario final.

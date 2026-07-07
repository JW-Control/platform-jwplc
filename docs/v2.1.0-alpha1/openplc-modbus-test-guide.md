# Guía de pruebas Modbus TCP/RTU para JWPLC Basic con OpenPLC

Fecha: 2026-05-23

## Objetivo

Validar comunicación Modbus TCP y RTU de OpenPLC sobre JWPLC Basic.

## Herramienta recomendada con interfaz

Para clases y pruebas con interfaz gráfica se recomienda:

```txt
ClassicDIY ModbusTool
```

Uso validado:

```txt
Modbus Master 2.1.3.0
```

## Prueba Modbus TCP

### Configuración de conexión

```txt
Mode: TCP
IP Address: 192.168.0.50
Port: 502
Slave ID: 0
```

### Lectura de salidas

```txt
Function: Read coils
Start Address: 0
Size: 8
Display Format: LED
Poll: 100 ms para pruebas rápidas
Poll: 250 ms o 500 ms recomendado para uso estable
```

Mapa:

```txt
Coil 0 -> Q0_0
Coil 1 -> Q0_1
...
Coil 7 -> Q0_7
```

### Lectura de entradas

```txt
Function: Read discrete
Start Address: 0
Size: 8
Display Format: LED
```

Mapa:

```txt
Discrete Input 0 -> I0_0
Discrete Input 1 -> I0_1
...
Discrete Input 7 -> I0_7
```

### Escritura de salidas

Para escribir una salida desde Modbus TCP, usar:

```txt
Function: Write single coil
Start Address: 1
Value: 1 / 0
```

Ejemplo:

```txt
Coil 1 -> Q0_1
```

Nota:

No usar `Q0_0` para escritura externa si el programa Ladder lo está usando como blink, porque la lógica PLC puede sobrescribir el valor en cada ciclo.

## Prueba de puerto TCP desde Windows

```powershell
Test-NetConnection 192.168.0.50 -Port 502
```

Resultado esperado:

```txt
TcpTestSucceeded : True
```

## Prueba de descubrimiento por ARP

```cmd
arp -a | findstr /i "de-ad-be-ef-de-ad"
```

Resultado esperado:

```txt
192.168.0.50    de-ad-be-ef-de-ad    dinámico
```

## Polling recomendado

| Uso | Polling recomendado |
|---|---:|
| Prueba rápida de laboratorio | 100 ms |
| Clase/demostración estable | 250 ms |
| HMI/SCADA ligero | 500 ms |
| Monitoreo lento/logging | 1000 ms o más |

Recomendación general:

```txt
No hacer polling más rápido de lo necesario.
```

## Estado RTU

Validado:

```txt
RTU solo funciona correctamente.
```

Pendiente:

```txt
RTU + TCP simultáneo: TCP funciona, RTU no trabaja correctamente en la prueba actual.
```

## Recomendación para clases

Para clases, preparar tres archivos o configuraciones:

1. Proyecto OpenPLC de blink.
2. Proyecto OpenPLC de entrada -> salida.
3. Configuración exportada de ModbusTool para lectura de coils/discrete inputs.

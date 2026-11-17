# Ejemplos JWPLC_RS485

Serie corta recomendada para taller:

```text
01.RS485_Send
02.RS485_Echo
03.RS485_Status
04.RS485_Puente_USB     puente USB <-> RS-485 para probar con una PC
```

En 2.1.0-alpha.17 se retiraron los duplicados (`RS485_Basic_Send`, `RS485_Basic_Echo`, `RS485_Status`) y las pruebas
internas (`RS485_Bridge` con `Serial2` directo, `RS485_Native`).

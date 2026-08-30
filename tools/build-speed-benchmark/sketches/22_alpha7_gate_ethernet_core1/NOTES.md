## Gate dirigido actual

La reproducción manual del 30-08 mostró dos fenómenos distintos:

1. `ETHNEXT` hacia un nodo que todavía no tiene el cable físico provoca `ERR_ETH` por diseño actual del soak. Esto debe modelarse como espera de operador (`OWNER_PENDING`) y no como fallo del nodo.
2. El retorno de M2 a owner produjo `ETH_LAT` ~455 ms y `SERVICE_MAX_US ETH` ~456 ms, apuntando a la ruta HTTP Ethernet síncrona como bloqueo principal de Core1 en esa captura.

Durante la misma prueba el polling Modbus cooperativo y CRC permanecieron estables. No se requiere modificar `JWPLC_ModbusRTU` para continuar este gate.

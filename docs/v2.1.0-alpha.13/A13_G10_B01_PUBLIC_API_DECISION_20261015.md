# G10-I B01 — decisión pendiente de la API pública

Estado: `B01_PUBLIC_API_DECISION_REQUIRED`; candidato en overlay, sin adopción.

## Consumidores reales acotados

| Ruta | Consulta | Tratamiento de BUS_BUSY en el candidato |
| --- | --- | --- |
| `JWPLC_ModbusTCP_Client.cpp::serviceConnection()` y `serviceResponse()` | `_tcpClient.connected()` | `jwplcProbeSocketStatus()` previo, bajo el mismo mutex; BUS_BUSY conserva la sesión, STALE_LEASE la descarta sin tocar W5500. |
| `JWPLC_ModbusTCP.cpp::serviceServer()` | `_client.connected()` | Probe previo al ACK RX INT y a la lectura del cliente, bajo el mismo mutex; BUS_BUSY reintenta y STALE_LEASE libera el handle. |
| `JWPLC_ModbusTCP.cpp::ensureServerListening()` | `EthernetServer::operator bool()` | Mantiene el mutex externo durante la consulta; el mapa listener/token se verifica antes del status. |
| `EthernetClient::connected()` y `status()` | `EthernetClass::socketStatus()` | API Arduino pública: BUS_BUSY sigue apareciendo como `0` o `SnSR::CLOSED`; un alias con lease obsoleto puede observar el status del propietario nuevo. |
| Sketches Ethernet HTTP/TFT de diagnóstico y código Arduino de terceros | `EthernetClient::connected()` | Un `0` temporal puede iniciar cierre, reconexión o un timeout del llamador. No hay canal para distinguir BUS_BUSY en el retorno escalar actual. |

Los usos de `connected()` en WiFi, Zigbee y otros stacks no son llamadas a
`JWPLC_Ethernet::EthernetClient` y quedan fuera de este contrato.

## Consecuencia y decisión

El probe interno `StatusReadOutcome::{Read,BusBusy,Stale}` protege los
consumidores Modbus anteriores sin cambiar firmas Arduino. La API pública
`connected()` retorna `0` al no obtener SPI; `socketStatus()` retorna
`SnSR::CLOSED`; `status()` hereda ese resultado. Esos valores también
representan desconexiones auténticas, de modo que los consumidores públicos
pueden descartar un socket vigente. Cambiar BUS_BUSY automáticamente a
"conectado" inventaría el estado opuesto y también podría prolongar una
sesión realmente cerrada. Un valor nuevo o una consulta pública adicional
requiere decisión explícita de compatibilidad y pruebas de consumidores.

Por tanto `B01=BLOCKED_SEMANTICS` y `N3_READY=NO`, aunque N1/N2 del overlay
pasen. B02/B03/B04 conservan sus PASS acotados source-backed; B05 aún requiere
reset y reconexión físicos. N3 no autoriza adopción ni hardware.

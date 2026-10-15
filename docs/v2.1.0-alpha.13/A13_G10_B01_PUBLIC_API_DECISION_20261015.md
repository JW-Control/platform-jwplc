# G10-I B01 — decisión pendiente de la API pública

Estado: `B01_PUBLIC_API_DECISION_REQUIRED`; candidato en overlay, sin adopción.

## Hito sobre el candidato integrado

La prueba nativa B01 compila los cuerpos compuestos reales de
`EthernetClient::connected()`, `status()`, `jwplcProbeSocketStatus()`,
`EthernetServer::operator bool()` y `EthernetClass::socketStatus()`. El W5500
simulado es únicamente la frontera de hardware; sus lecturas exigen el mutex.
Antes de la corrección, un alias de `EthernetClient` con lease reciclado leía
`ESTABLISHED` del nuevo propietario. Con SPI ocupado, las tres consultas
escalares devolvían `0`, `CLOSED` y `false` aunque el hardware seguía en
`ESTABLISHED`/`LISTEN`; al liberar SPI, volvían al valor real. El probe devolvía
`BusBusy` sin leer ni modificar el byte de salida. El guard de lease añadido
a `connected()` y `status()` evita ahora que un alias obsoleto lea W5500. La
prueba verifica propietario vigente, CLOSED real, SPI ocupado, recuperación,
reciclaje, lease inválido y retorno inmediato de `status()` para un cliente
sin socket. Rechaza mutaciones que eliminan cualquiera de los dos guards,
cambian BUS_BUSY de `connected()` a conectado o hacen que el cliente vacío
espere SPI.

Representación interna existente: `(StatusReadOutcome, statusOut)`; `statusOut`
es **estado actual válido exclusivamente cuando el outcome es `Read`**. En
`BusBusy` o `Stale` queda intacto y no es una lectura nueva. Si un consumidor
necesita conservar un último estado, debe guardar `{token, status,
lastKnownValid}` únicamente tras `Read`, invalidar la validez al cambiar el
token y no usarlo como estado actual durante BUS_BUSY. Los consumidores
Modbus actuales no necesitan ese caché: conservan su operación pendiente y
reintentan. No se introduce estado persistente sin consumidor que lo justifique.

## Consumidores reales acotados

| Ruta | Consulta | Tratamiento de BUS_BUSY en el candidato |
| --- | --- | --- |
| `JWPLC_ModbusTCP_Client.cpp::serviceConnection()` y `serviceResponse()` | `_tcpClient.connected()` | `jwplcProbeSocketStatus()` previo, bajo el mismo mutex; BUS_BUSY conserva la sesión, STALE_LEASE la descarta sin tocar W5500. |
| `JWPLC_ModbusTCP.cpp::serviceServer()` | `_client.connected()` | Probe previo al ACK RX INT y a la lectura del cliente, bajo el mismo mutex; BUS_BUSY reintenta y STALE_LEASE libera el handle. |
| `JWPLC_ModbusTCP.cpp::ensureServerListening()` | `EthernetServer::operator bool()` | Mantiene el mutex externo durante la consulta; el mapa listener/token se verifica antes del status. |
| `EthernetClient::connected()` y `status()` | `EthernetClass::socketStatus()` | `REVIEW`: BUS_BUSY sigue apareciendo como `0` o `SnSR::CLOSED`; el guard nuevo impide que un lease obsoleto lea el socket nuevo. |
| `EthernetServer::operator bool()` | Listener con lease/token bajo mutex | `REVIEW`: BUS_BUSY retorna `false`, sin lectura W5500 ni reapertura interna; un llamador externo puede interpretarlo como listener cerrado. |
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

Matriz B01: `PASS` para Modbus TCP cliente `serviceConnection`/
`serviceResponse`, Modbus TCP servidor `serviceServer` y `ensureServerListening`,
y rechazo de leases obsoletos en `EthernetClient`; `REVIEW` para `connected()`,
`status()`, `socketStatus()` y `EthernetServer::operator bool()` como APIs
escalares, incluidos los sketches HTTP/TFT y terceros. Mientras BUS_BUSY, los
consumidores Modbus preservan sesión/listener y reintentan; los consumidores
externos no pueden distinguirlo en el retorno escalar.

El candidato ya expone `jwplcProbeSocketStatus(uint8_t&)` como adición
optativa para `EthernetClient`; N2 comprueba que compila junto a las firmas
Arduino. No existe equivalente optativo para el estado del listener ni para
`socketStatus(uint8_t)` sin identidad de propietario. Añadirlo ahora no
corrige a los llamadores que continúan usando `bool`/`uint8_t`, y requeriría
un contrato público de migración. **Decisión humana pendiente:** elegir si
las APIs escalares se documentan como consultas de mejor esfuerzo y los
consumidores que toman decisiones de cierre/reconexión deben migrar a probes
optativos con `Read/BusBusy/Stale`, incluidos listener y raw socket, o si se
acepta explícitamente el riesgo de falsos negativos bajo contención en los
consumidores externos. Cambiar los valores de retorno existentes exige una
decisión de compatibilidad Arduino separada.

Por tanto `B01=BLOCKED_SEMANTICS` y `N3_READY=NO`, aunque N1/N2 del overlay
pasen. B02/B03/B04 conservan sus PASS acotados source-backed; B05 aún requiere
reset y reconexión físicos. N3 no autoriza adopción ni hardware.

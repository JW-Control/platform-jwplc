# Alpha6 — Ethernet no bloqueante y diagnóstico de runtime

Fecha: 2026-08-26

## Base

```txt
Repositorio: JW-Control/platform-jwplc
Base: main
Base commit: 183689eedd6b8f42c38685a62f9797b8e42cfc4a
Branch: v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

## Hallazgo

Se reprodujo conceptualmente el caso:

```txt
W5500 presente
RJ45 conectado
PHY LINK ON
sin servidor DHCP disponible
```

La integración previa ejecutaba `JWPLC_Ethernet.begin()` desde `jwplcSystemTask()`.
El `begin()` síncrono podía permanecer dentro de DHCP durante varios segundos y retenía
el mutex SPI durante la operación completa.

Consecuencias observadas:

- pausa aparente del RTC;
- actualización tardía del TCA/I/O;
- TFT y otros consumidores SPI potencialmente bloqueados;
- LED ETH rojo sin suficiente detalle de causa.

## Decisión de arquitectura

No mover TCA/RTC/I2C al segundo núcleo como corrección del problema.

Se mantiene:

```txt
jwplcSystemTask:
- scan I/O TCA
- RTC
- Ethernet cooperativo
- Display
```

El núcleo opuesto queda disponible para `loop1()` y cargas que realmente requieran
separación, por ejemplo comunicaciones WiFi o lógica de usuario específica.

Principio:

> Ethernet debe ceder el control; el resto del PLC no debe moverse alrededor de una
> operación de red bloqueante.

## Cambios de esta feature

### 1. Probe físico separado

Se agrega:

```cpp
JWPLC_Ethernet.probeHardware();
```

Valida W5500 + PHY/LINK sin solicitar DHCP.

Objetivo inmediato: permitir que `JWPLC_PCB_Acceptance_Test_v3` pruebe hardware sin
activar accidentalmente un DHCP bloqueante antes de seleccionar la topología de red.

### 2. DHCP cooperativo en backend W5x00

Se agregan extensiones internas:

```cpp
Ethernet.beginDHCPAsync(...);
Ethernet.pollDHCP();
Ethernet.dhcpInProgress();
```

El intercambio DHCP queda dividido en pasos cortos:

```txt
START -> DISCOVER -> OFFER -> REQUEST -> ACK -> LEASED
```

Cada `pollDHCP()` retorna sin esperar el timeout completo.

### 3. Servicio cooperativo JWPLC

Se agrega:

```cpp
JWPLC_Ethernet.service();
```

El hook automático pasa a ejecutar un único paso por tick:

```cpp
jwplcEthernetTickCallback()
    -> JWPLC_Ethernet.service()
```

`begin()` se conserva como API síncrona por compatibilidad.

### 4. Estado de runtime y códigos base

Estados:

```txt
NOT_STARTED
PROBING
PHY_READY
LINK_OFF
DHCP_PENDING
READY
ERROR
```

Códigos iniciales:

```txt
INI  inicializando/no iniciado
PHY  W5500 + PHY listos
LNK  sin link físico
DHC  DHCP pendiente o fallido
HW   W5500 no detectado
IP   IP inválida
SPI  SPI/mutex
DIS  Ethernet deshabilitado
---  operativo
```

Estos códigos serán la base de la ampliación visual del panel ETH.

## Compatibilidad

Se conserva:

- `JWPLC_Ethernet.begin()`;
- DHCP e IP estática;
- autoload normal de Ethernet;
- W5500 como parte del runtime estándar;
- mutex SPI compartido;
- Arduino IDE como flujo principal.

No se mueve I2C a otro núcleo.
No se elimina ningún periférico del autoload.

## Precompilación durante el piloto

El backend `JWPLC_Ethernet_W5x00_Backend` estaba en `precompiled=full`.
Durante esta feature se compila temporalmente desde fuente para validar los cambios.

Antes de integrar/cerrar:

- regenerar el `.a` validado o tomar una decisión explícita sobre precompilación;
- restaurar el comportamiento de build definido para Alpha5;
- repetir validación Arduino IDE/CLI.

## Pendiente técnico conocido

`Ethernet.maintain()` conserva todavía la ruta síncrona upstream durante renovación o
rebind del lease DHCP.

Por tanto, esta feature NO debe declararse todavía como Ethernet completamente no
bloqueante.

Pendiente:

- convertir renew/rebind a mantenimiento cooperativo;
- medir tiempo máximo de cada `service()`;
- confirmar que el scan I/O y RTC no presentan pausas por DHCP;
- confirmar coexistencia TFT/FRAM/SD durante DHCP sin servidor.

## Gate físico propuesto

### A. Sin RJ45

Esperado:

```txt
W5500 = PASS
LINK = OFF
DHCP = no iniciado
TCA/RTC = sin pausas
```

### B. Laptop directa, LINK ON, sin DHCP

Esperado:

```txt
W5500 = PASS
LINK = ON
DHCP = DHC / timeout recuperable
TCA/RTC = continúan actualizándose
SPI mutex = sin retenciones de segundos
```

### C. Laptop directa, IP estática

```txt
JWPLC: 192.168.77.2/24
Laptop: 192.168.77.1/24
HTTP: 8080
```

Esperado: HTTP/TCP PASS sin Internet.

### D. Router con DHCP

Esperado:

```txt
LINK = ON
DHCP = PASS
IP/gateway = válidos
```

## Criterio de cierre de la feature

- [ ] Compila JWPLC Basic en Arduino CLI.
- [ ] Compila JWPLC Basic en Arduino IDE.
- [ ] Probe W5500 no dispara DHCP.
- [ ] LINK OFF no bloquea I/O/RTC.
- [ ] LINK ON + sin DHCP no bloquea I/O/RTC.
- [ ] LINK ON + sin DHCP no retiene SPI durante segundos.
- [ ] DHCP real obtiene lease.
- [ ] IP estática conserva HTTP/TCP.
- [ ] Desconexión/reconexión recupera.
- [ ] Renew/rebind DHCP cooperativo.
- [ ] TFT/FRAM/SD coexistencia.
- [ ] Acceptance Test actualizado.
- [ ] Backend precompilado regenerado o decisión documentada.
- [ ] Diagnóstico ETH diferenciado de ERR de aplicación.

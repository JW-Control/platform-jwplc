# Alpha6 — Validación física Ethernet no bloqueante

Fecha: 2026-08-27

Rama:

```txt
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

## Objetivo

Validar que Ethernet/W5500 deje de bloquear el runtime del JWPLC Basic cuando:

- existe LINK físico pero no hay servidor DHCP;
- se utiliza IP estática;
- existe tráfico TCP/HTTP real;
- W5500 comparte SPI con TFT, FRAM y microSD.

La validación se ejecutó manteniendo el autoload normal del package.

## 1. Autoload y resolución de librería

El acceptance compiló sin `#include <Ethernet.h>` ni `#include <JWPLC_Ethernet.h>` manuales.

Resultado:

```txt
Compile exit code        : 0
Ethernet global usada    : False
Backend antiguo usado    : False
Ethernet.h ambiguo       : False
Redefiniciones           : False
Guard W5100 antiguo      : False

ALPHA6_ETHERNET_AUTOLOAD_BUILD=PASS
```

La única librería Ethernet resuelta fue:

```txt
JWPLC_Ethernet 1.0.0
```

Conclusión:

- la API Ethernet queda disponible mediante el autoload JWPLC;
- la librería global `Documents\Arduino\libraries\Ethernet` no participa;
- `JWPLC_Ethernet_W5x00_Backend` deja de ser una librería Arduino independiente;
- el backend W5x00 queda consolidado dentro de `JWPLC_Ethernet`.

## 2. Topología N — laptop directa, LINK ON, sin DHCP

Topología:

```txt
Laptop <---- RJ45 directo ----> JWPLC Basic
LINK ON
Sin servidor DHCP
```

Secuencia observada:

```txt
ETH runtime state=2 code=PHY status=PHY ready
ETH runtime state=4 code=DHC status=DHCP pending
ETH runtime state=6 code=DHC status=DHCP failed
```

Métricas:

```txt
max service() us : 1416
max TCA age ms   : 21
max RTC age ms   : 1003
max mutex wait us: 5
mutex fails      : 0
final code       : DHC
```

Gates:

```txt
ETH_NO_DHCP_TCA=PASS
ETH_NO_DHCP_RTC=PASS
ETH_NO_DHCP_SPI=PASS
ETH_NO_DHCP_EXPECTED_NETWORK_FAILURE=PASS
ETH_NO_DHCP_NONBLOCKING=PASS
ETH_NETWORK_OR_FAILSAFE=PASS
```

Conclusión: la ausencia de servidor DHCP ya no congela TCA, RTC ni el bus SPI.

## 3. Stress SPI 10 min — topología N

```txt
Duration requested ms: 600000
Cycles: 58940

MUTEX
samples: 235760
acquire fails: 0
max wait us: 4403
>1ms: 3740
>10ms: 0
>25ms: 0

ETHERNET / W5500
HW samples/fails: 117880/0
Link samples/drops: 117880/0
ETH SPI lock timeouts: 0
DHCP maintain fails: 0
ETH max probe us: 18

FRAM
reads/fails: 58940/0
max us: 97

microSD
reads/fails: 58940/0
max us: 47
```

Resultado:

```txt
SPI_MUTEX_TIMEOUT_SAFETY=PASS
SPI_MUTEX_LATENCY=PASS
SPI_TFT_ACTIVITY=PASS
SPI_ETH_W5500_STABILITY=PASS
SPI_FRAM_STABILITY=PASS
SPI_SD_STABILITY=PASS
SPI_LONG_10MIN_STRESS=PASS
```

## 4. Topología L — IP estática + HTTP

Configuración:

```txt
Laptop:      192.168.77.1/24
JWPLC Basic: 192.168.77.2/24
HTTP:        puerto 8080
```

Resultado:

```txt
Ethernet enabled: yes
Begin attempted: yes
Ready: yes
Status: OK
Diagnostic: ---
IP: 192.168.77.2

ETH_HTTP_SERVER=READY port 8080
ETH_HTTP_REQUEST=RECEIVED_FROM_LAPTOP
ETH_LAPTOP_TCP_HTTP=PASS
ETH_NETWORK_OR_FAILSAFE=PASS
```

El indicador ETH se observó verde al quedar Ethernet operativo.

## 5. Stress SPI final 10 min — IP estática + HTTP real

Durante los 10 minutos se generaron solicitudes HTTP reales desde la laptop.

```txt
Duration requested ms: 600000
Cycles: 58330
```

### Mutex

```txt
samples: 233320
acquire fails: 0
max wait us: 4417
>1ms: 1285
>10ms: 0
>25ms: 0
```

### Ethernet / W5500

```txt
HW samples/fails: 116660/0
Link samples/drops: 116660/0
Unexpected LINK ON: 0
ETH SPI lock timeouts: 0
DHCP maintain fails: 0
HTTP requests during stress: 591
HTTP lock fails: 0
ETH max probe us: 383
```

### FRAM

```txt
reads/fails: 58330/0
max us: 451
```

### microSD

```txt
reads/fails: 58330/0
max us: 45
```

### TFT

```txt
frames start/end: 2/11819
```

Resultado:

```txt
SPI_MUTEX_TIMEOUT_SAFETY=PASS
SPI_MUTEX_LATENCY=PASS
SPI_TFT_ACTIVITY=PASS
SPI_ETH_W5500_STABILITY=PASS
SPI_FRAM_STABILITY=PASS
SPI_SD_STABILITY=PASS
SPI_LONG_10MIN_STRESS=PASS

PCB_FINAL_ACCEPTANCE=PASS
```

Conclusión: TFT + W5500 + FRAM + microSD coexistieron durante 10 minutos con tráfico HTTP real, sin pérdidas de LINK, fallos de mutex ni fallos de periféricos.

## 6. Ajustes realizados durante el piloto

### Consolidación Ethernet

Se consolidó el backend W5x00 dentro de una sola librería Arduino:

```txt
JWPLC_Ethernet
```

El backend antiguo deja de ser una librería independiente:

```txt
JWPLC_Ethernet_W5x00_Backend
```

### Acceptance

Se corrigió un falso positivo en el contador `ETH SPI lock timeouts`.

`lastError()` es persistente; el acceptance ahora atribuye un timeout únicamente a la operación que realmente devolvió fallo.

También se añadió distinción semántica:

```txt
PASS
FAIL
ABORTED
INCOMPLETE
```

Un aborto manual mediante `X` ya no debe registrarse como fallo técnico.

## 7. Diagnóstico visual ETH / BUS

### Regla de arquitectura

Los indicadores deben permanecer independientes:

```txt
RUN = estado de ejecución
ERR = reservado a aplicación / usuario
BUS = diagnóstico RS-485 / Modbus
ETH = diagnóstico Ethernet
```

Un error automático de BUS o ETH **no debe activar automáticamente `ERR`**.

### ETH — códigos definidos

```txt
---  operativo
INI  inicializando / no iniciado
PHY  W5500 + PHY disponibles
LNK  sin LINK físico
DHC  DHCP pendiente o fallido
HW   W5500 no detectado
IP   configuración IP inválida
SPI  error / timeout de mutex SPI
DIS  Ethernet deshabilitado
```

Estos códigos ya derivan del estado interno de `JWPLC_Ethernet`; queda pendiente mostrarlos visualmente en el panel idle.

### BUS — códigos propuestos para Alpha6

```txt
---  bus disponible / sin error
DIS  RS-485 deshabilitado
INI  RS-485 habilitado pero no iniciado
SER  Serial/RS-485 inválido o error interno
SID  Modbus: Slave ID inválido
MAP  Modbus: mapa de registros inválido
TMO  Modbus: timeout
CRC  Modbus: CRC incorrecto
EXC  Modbus: excepción recibida/generada
RSP  Modbus: respuesta inválida
OVF  Modbus: buffer overflow
FUN  Modbus: función no soportada
```

Notas:

- `JWPLC_MODBUS_NOT_STARTED` no debe considerarse error si el usuario trabaja con RS-485 crudo sin Modbus;
- la actividad normal del bus debe seguir representándose con verde, no mediante un código de error;
- BUS y ETH deben poder mostrar su causa sin depender del LED ERR.

## 8. Pendientes antes de cerrar Alpha6

- [ ] DHCP renew/rebind cooperativo.
- [ ] Validación con router y DHCP real.
- [ ] Validación de desconexión/reconexión de RJ45.
- [ ] Códigos ETH visibles en el panel idle.
- [ ] Implementar códigos BUS equivalentes.
- [ ] Mantener ERR exclusivamente bajo control de aplicación/usuario.
- [ ] Regresión Arduino IDE.
- [ ] Regresión Arduino CLI.
- [ ] Actualizar scripts/tooling que aún nombren el backend antiguo.
- [ ] Documentar decisión final de precompilación.
- [ ] Regenerar `.a` únicamente cerca del cierre de Alpha6.
- [ ] Verificar paridad fuente vs precompilado.

## Estado del gate

```txt
AUTOLOAD / RESOLVER                   PASS
LINK ON SIN DHCP NO BLOQUEANTE        PASS
TCA DURANTE DHCP FALLIDO              PASS
RTC DURANTE DHCP FALLIDO              PASS
SPI DURANTE DHCP FALLIDO              PASS
IP ESTATICA                           PASS
HTTP REAL                             PASS
SPI STRESS 10 MIN SIN DHCP            PASS
SPI STRESS 10 MIN + HTTP REAL         PASS

GATE FISICO ALPHA6                    PASS
```

El gate físico de la corrección inicial queda cerrado.

Alpha6 todavía no se considera cerrada debido a los pendientes de renew/rebind, diagnóstico visual, router DHCP, regresiones y precompilación.

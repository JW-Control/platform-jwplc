# Alpha5 - revisión del probe SPI con enlace Ethernet activo

Fecha: 2026-08-24.

## Objetivo

Repetir el probe de contención SPI de Alpha5 con RJ45 conectado a una red activa, después de confirmar PASS sin cable.

## Resultado con enlace activo

Durante la ventana de inicialización automática de Ethernet se observó:

```txt
[STABLE 2] t=1042 ms | ETH attempted=SI ready=SI error=OK | FRAM=FAIL 49997 us | SD=OK 93885 us
```

Resumen:

```txt
STARTUP
FRAM OK/FAIL: 8/0
FRAM max us: 138
SD OK/FAIL: 8/0
SD max us: 31

STABLE
FRAM OK/FAIL: 9/1
FRAM max us: 49997
SD OK/FAIL: 10/0
SD max us: 93885

REDRAW
FRAM OK/FAIL: 10/0
FRAM max us: 35
SD OK/FAIL: 10/0
SD max us: 14

ETH SPI lock timeout observado: 0
SPI_STARTUP_PROBE=REVIEW
```

## Control sin enlace

En la repetición con `Link OFF`:

```txt
STABLE
FRAM OK/FAIL: 10/0
FRAM max us: 18609
SD OK/FAIL: 10/0
SD max us: 17

SPI_STARTUP_PROBE=PASS
```

Esto reproduce la diferencia entre la ruta sin cable y la ruta con enlace activo.

## Interpretación

El contador `ETH SPI lock timeout observado: 0` no descarta contención. Ese contador sólo indica si Ethernet falló al adquirir el mutex. En el caso observado, Ethernet sí obtuvo el mutex y otros periféricos tuvieron que esperar por él.

Además, `runSample()` captura el timestamp antes de ejecutar las sondas FRAM/SD y sólo imprime el snapshot Ethernet después de ambas. Por ello, `ready=SI` en la línea de `STABLE 2` puede reflejar que Ethernet terminó su inicialización mientras FRAM y SD estaban esperando; no implica que Ethernet ya estuviera libre al inicio de la muestra.

El código actual de `JWPLC_EthernetClass::begin()` adquiere el mutex SPI antes de inicializar W5500 y conserva ese mutex durante la llamada DHCP:

```txt
Ethernet.begin(_mac, _dhcpTimeoutMs, _responseTimeoutMs)
```

La liberación ocurre después de esa llamada. Con enlace activo, la ruta DHCP puede mantener el bus reservado durante decenas o cientos de milisegundos aunque no exista el antiguo `delay(560)` eliminado en Alpha4.

## Estado

**SPI con Link OFF: PASS.**

**SPI con enlace activo + DHCP: REVIEW / contención reproducida.**

No se considera regresión del antiguo `delay(560)`, pero sí una segunda causa de retención prolongada del mutex: la negociación DHCP se ejecuta dentro de la sección crítica global SPI.

Antes de modificar timeouts o relajar el mutex, se requiere un gate de aislamiento usando IP estática con enlace activo. Si el problema desaparece en modo estático, quedará confirmada la ruta DHCP como causa específica.

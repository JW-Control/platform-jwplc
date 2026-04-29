# JWPLC_Ethernet

Librería interna del package JWPLC para manejar Ethernet W5500 en JWPLC Basic.

> Estado: base inicial para `2.0.0-alpha.26`.

## Objetivo

Proveer una API simple para inicializar y monitorear Ethernet sin que el usuario tenga que configurar pines SPI ni detalles del W5500.

API inicial:

```cpp
JWPLC_Ethernet.begin();
JWPLC_Ethernet.isReady();
JWPLC_Ethernet.linkUp();
JWPLC_Ethernet.localIP();
JWPLC_Ethernet.statusString();
JWPLC_Ethernet.maintain();
```

## Uso básico DHCP

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    if (JWPLC_Ethernet.begin())
    {
        Serial.print("IP: ");
        Serial.println(JWPLC_Ethernet.localIP());
    }
    else
    {
        Serial.print("Ethernet error: ");
        Serial.println(JWPLC_Ethernet.statusString());
    }
}

void loop()
{
    JWPLC_Ethernet.maintain();
}
```

## Uso con IP estática

```cpp
#include <JWPLC_Ethernet.h>

void setup()
{
    Serial.begin(115200);
    delay(1200);

    JWPLC_Ethernet.begin(
        IPAddress(192, 168, 1, 50),
        IPAddress(8, 8, 8, 8),
        IPAddress(192, 168, 1, 1),
        IPAddress(255, 255, 255, 0));
}

void loop()
{
}
```

## Notas

- Esta versión usa la librería Arduino `Ethernet` como base.
- La optimización avanzada del W5500 queda para una etapa posterior.
- El CS Ethernet por defecto viene del core JWPLC mediante `JWPLC_ETH_CS`.
- El bus SPI se protege usando los hooks internos `jwplcSPI_acquire()` y `jwplcSPI_release()` en operaciones de estado e inicialización.

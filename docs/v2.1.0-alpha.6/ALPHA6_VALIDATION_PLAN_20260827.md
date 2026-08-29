# Alpha6 — plan de validación posterior a renew/rebind y diagnósticos

Fecha: 2026-08-27

Branch:

```text
v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime
```

## Objetivo

Validar los commits posteriores al gate físico previo sin confundir implementación con
resultado de hardware.

La validación se divide en gates. No avanzar a precompilación ni cierre de Alpha6 hasta
que los gates obligatorios queden registrados.

---

## Gate 0 — sincronización local

Esperado:

- working tree limpio antes de empezar nuevas modificaciones;
- HEAD igual a `origin/v2.1.0-alpha.6/feature/ethernet-nonblocking-runtime`;
- commits nuevos visibles en orden.

Revisar especialmente:

```text
feat(ethernet): hacer cooperativo renew y rebind DHCP
feat(display): mostrar diagnósticos BUS y ETH independientes
chore(ethernet): adaptar tooling a librería unificada
```

---

## Gate 1 — selección de librería unificada

Ejecutar:

```powershell
$CLI = "C:\Users\jeykc\AppData\Local\Programs\arduino-ide\resources\app\lib\backend\resources\arduino-cli.exe"
.\tools\build-speed-benchmark\Verify-JWPLCUnifiedEthernetSelection.ps1 -ArduinoCli $CLI
```

Esperado:

```text
JWPLC_ETHERNET_UNIFIED_SELECTION=PASS
```

No debe aparecer seleccionada:

```text
JWPLC_Ethernet_W5x00_Backend
Documents\Arduino\libraries\Ethernet
```

---

## Gate 2 — compilación CLI del acceptance

Compilar primero sin subir.

FQBN:

```text
jwplc_local:esp32:jwplcbasic
```

Sketch:

```text
tools/build-speed-benchmark/sketches/16_alpha6_ethernet_nonblocking_acceptance
```

Criterio:

```text
compile exit code = 0
```

La compilación es obligatoria porque los commits nuevos modifican tanto
`JWPLC_Ethernet` como `JWPLC_Display`.

---

## Gate 3 — regresión visual básica del IDLE

Después del upload, comprobar que el layout conserve:

- título;
- PWR/RUN/ERR/BUS/ETH;
- 8 entradas;
- 8 salidas;
- reloj/fecha;
- sin solapamientos de texto.

Los códigos BUS/ETH deben caber dentro del mismo rectángulo existente y no alterar
la posición de I/O o RTC.

### ERR

Con `ERR` inicialmente apagado, ningún estado de ETH/BUS debe encenderlo.

Después, probar explícitamente:

```cpp
JWPLC_Display.setErrLed(true);
```

y confirmar que ERR puede encenderse por decisión de aplicación aunque ETH/BUS tengan
sus propios estados.

---

## Gate 4 — matriz ETH visual

### Sin LINK

Esperado:

```text
ETH code = LNK
ETH color = negro/inactivo
ERR = sin cambio
```

### DHCP inicial pendiente

Esperado:

```text
ETH code = DHC
ETH color = negro/inactivo
ERR = sin cambio
```

### DHCP fallido

Esperado:

```text
ETH code = DHC
ETH color = rojo
ERR = sin cambio
```

### IP estática lista

Esperado:

```text
ETH code = ---
ETH color = verde
ERR = sin cambio
```

### W5500/PHY intermedio

Durante el arranque pueden observarse transitoriamente:

```text
INI
PHY
```

No es obligatorio capturarlos visualmente si la transición es demasiado rápida; sí deben
ser coherentes por `diagnosticCode()`/serial.

---

## Gate 5 — matriz BUS visual

### RS-485 listo, Modbus no iniciado

Este caso es crítico para compatibilidad.

Esperado:

```text
BUS code = ---
BUS idle = negro/inactivo
BUS con actividad reciente = verde
ERR = sin cambio
```

No debe aparecer rojo sólo porque Modbus RTU no haya sido iniciado.

### Error Modbus provocado por prueba

Validar al menos un error reproducible, por ejemplo timeout de master contra un slave
inexistente.

Esperado:

```text
BUS code = TMO
BUS color = rojo
ERR = sin cambio
```

Si durante la regresión aparecen naturalmente otras causas, verificar correspondencia:

```text
SID MAP CRC EXC RSP OVF FUN
```

No es necesario provocar de forma artificial todas las causas para cerrar el gate básico.

---

## Gate 6 — router con DHCP real

Conectar JWPLC Basic a router/switch con DHCP.

Acceptance ruta:

```text
R
```

Esperado:

- W5500 detectado;
- LINK ON;
- lease DHCP válido;
- IP distinta de 0.0.0.0;
- gateway válido;
- `JWPLC_Ethernet.isReady() = true`;
- `diagnosticCode() = ---` después de completar DHCP;
- TCA/RTC continúan vivos;
- sin timeouts del mutex SPI.

Este gate valida DHCP inicial sobre los commits nuevos, no todavía necesariamente T1/T2.

---

## Gate 7 — desconexión/reconexión de RJ45

Partir de Ethernet operativo.

1. retirar RJ45;
2. observar `LNK`;
3. confirmar que I/O, RTC y TFT siguen actualizándose;
4. reconectar RJ45;
5. esperar recuperación automática sin reset del ESP32.

Esperado final:

```text
ETH code = ---
ETH ready = yes
```

En DHCP, la recuperación puede incluir nueva negociación `DHC` antes de volver a `---`.

---

## Gate 8 — renew/rebind cooperativo

Este gate requiere una lease DHCP suficientemente corta o un mecanismo de prueba controlado.
No asumir PASS sólo porque el lease inicial funcione.

Se debe observar/registrar por separado:

### Renew T1

- runtime permanece responsivo;
- no hay retención SPI de segundos;
- lease se renueva o, si falla, el runtime conserva el lease vigente mientras reintenta;
- código `DHC` puede aparecer durante el mantenimiento;
- vuelve a `---` tras éxito.

### Rebind T2

- se alcanza la ruta de rebind;
- no bloquea runtime;
- éxito actualiza configuración DHCP;
- fallo no congela el PLC y queda diagnosticado como `DHC`.

### Métricas mínimas

Registrar:

- tiempo máximo observado de `service()`;
- edad máxima TCA;
- edad máxima RTC;
- fallos mutex;
- fallos W5500;
- cambios de LINK;
- resultado renew/rebind.

Si el router no permite lease corta, preparar un hook de test controlado en una etapa
separada; no modificar arbitrariamente timers de producción sólo para cerrar el checklist.

---

## Gate 9 — stress SPI posterior a los cambios

Una vez pasen router/reconnect, repetir al menos el gate rápido de 60 s con:

- TFT;
- W5500;
- FRAM;
- microSD;
- tráfico de red real si aplica.

Si cualquier cambio de Display/DHCP altera latencias o estabilidad, repetir además los
10 minutos antes de cerrar Alpha6.

Criterios históricos de referencia:

```text
mutex acquire fails = 0
>10 ms = 0
ETH SPI lock timeouts = 0
W5500 fails = 0
FRAM fails = 0
SD fails = 0
```

---

## Gate 10 — Arduino IDE

Compilar/subir desde Arduino IDE usando el package local/branch actual.

Verificar:

- no ambigüedad con `Ethernet.h`;
- autoload normal;
- ninguna dependencia manual al backend eliminado;
- sketch simple;
- acceptance o ejemplo Ethernet representativo.

---

## Gate 11 — decisión de precompilación

Sólo después de los gates funcionales:

- decidir/regenerar `libJWPLC_Ethernet.a` unificado;
- medir source vs precompiled;
- validar paridad;
- confirmar que no reaparece una segunda librería backend.

No publicar un `.a` como definitivo antes de esta comparación.

---

## Cierre esperado antes de PR

- [ ] selección unificada PASS;
- [ ] compilación CLI PASS;
- [ ] TFT/layout PASS;
- [ ] códigos ETH PASS;
- [ ] códigos BUS PASS;
- [ ] ERR independiente PASS;
- [ ] router DHCP PASS;
- [ ] reconnect PASS;
- [ ] renew T1 PASS;
- [ ] rebind T2 PASS;
- [ ] stress SPI posterior PASS;
- [ ] Arduino IDE PASS;
- [ ] decisión de precompilación registrada;
- [ ] documentación/checklist final actualizados.

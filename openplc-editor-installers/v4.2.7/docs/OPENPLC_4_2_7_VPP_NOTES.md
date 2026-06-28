# Notas OpenPLC Editor 4.2.7 + JWPLC Basic 2.0.0

## Objetivo alpha3

Preparar la integración de `JWPLC BASIC [2.0.0]` para OpenPLC Editor 4.2.7 usando el flujo nuevo de paquetes `.vpp`.

## Cambio principal frente a 4.1.4

En 4.1.4 la integración se hacía como patch directo sobre archivos del editor, principalmente:

```txt
resources/sources/boards/hals.json
resources/sources/hal/jwplcbasic.cpp
resources/sources/Baremetal/Baremetal.ino
resources/sources/Baremetal/ModbusSlave.cpp
resources/sources/Baremetal/ModbusSlave.h
```

En 4.2.7 la placa debe vivir preferentemente como paquete VPP:

```txt
manifest.json
signature.json
hal/jwplcbasic.cpp
assets/jwplcbasic.svg
```

## Decisión técnica

La integración OpenPLC sigue siendo externa/opcional. No se integra dentro del runtime normal Arduino del package JWPLC.

## Mapa I/O

| JWPLC | IEC OpenPLC | Modbus TCP |
|---|---|---|
| I0_0 | %IX0.0 | Discrete Input 0 |
| I0_1 | %IX0.1 | Discrete Input 1 |
| I0_2 | %IX0.2 | Discrete Input 2 |
| I0_3 | %IX0.3 | Discrete Input 3 |
| I0_4 | %IX0.4 | Discrete Input 4 |
| I0_5 | %IX0.5 | Discrete Input 5 |
| I0_6 | %IX0.6 | Discrete Input 6 |
| I0_7 | %IX0.7 | Discrete Input 7 |
| Q0_0 | %QX0.0 | Coil 0 |
| Q0_1 | %QX0.1 | Coil 1 |
| Q0_2 | %QX0.2 | Coil 2 |
| Q0_3 | %QX0.3 | Coil 3 |
| Q0_4 | %QX0.4 | Coil 4 |
| Q0_5 | %QX0.5 | Coil 5 |
| Q0_6 | %QX0.6 | Coil 6 |
| Q0_7 | %QX0.7 | Coil 7 |

## Compatibilidad Baremetal 4.2.7

OpenPLC Editor 4.2.7 mantiene `pinMask_*` como `uint8_t` en el runtime Baremetal. JWPLC Basic requiere `uint16_t` porque sus pines industriales son virtuales (`0x22xx`).

Parche requerido:

```cpp
#if defined(JWPLC_BASIC)
extern uint16_t pinMask_DIN[];
extern uint16_t pinMask_AIN[];
extern uint16_t pinMask_DOUT[];
extern uint16_t pinMask_AOUT[];
#else
extern uint8_t pinMask_DIN[];
extern uint8_t pinMask_AIN[];
extern uint8_t pinMask_DOUT[];
extern uint8_t pinMask_AOUT[];
#endif
```

## Compatibilidad Modbus TCP

No usar `ETH.begin()` en JWPLC Basic. El hardware usa W5500 por SPI compartido, por lo que la adaptación validada usa `JWPLC_Ethernet`.

La alpha3 reutiliza los archivos Modbus adaptados previamente en:

```txt
installers/openplc-jwplc-basic-v2.0.0/open-plc-editor/resources/sources/Baremetal/ModbusSlave.cpp
installers/openplc-jwplc-basic-v2.0.0/open-plc-editor/resources/sources/Baremetal/ModbusSlave.h
```

## Firma VPP

OpenPLC Editor 4.2.7 exige `signature.json` válido. Para pruebas internas se puede generar una llave Ed25519 JW Control, firmar el paquete y usar un build de OpenPLC que confíe en esa llave.

Para distribución pública, el paquete debe ser firmado por una llave confiable para OpenPLC Editor stock. Pendiente decidir:

- solicitar inclusión de llave pública JW Control al equipo OpenPLC/Autonomy;
- solicitar firma del paquete por pipeline oficial OpenPLC;
- mantener build propio para talleres/laboratorio.

## Checklist alpha3

- [ ] OpenPLC Editor 4.2.7 limpio.
- [ ] Package Arduino JWPLC 2.0.0 instalado.
- [ ] Parche Baremetal `uint16_t` aplicado.
- [ ] ModbusSlave JWPLC/W5500 aplicado.
- [ ] VPP estructuralmente válido.
- [ ] VPP firmado o limitación documentada.
- [ ] `JWPLC BASIC [2.0.0]` aparece en Device Configuration.
- [ ] Compila sketch OpenPLC mínimo.
- [ ] Sube por USB.
- [ ] Lee I0_0..I0_7.
- [ ] Escribe Q0_0..Q0_7.
- [ ] Debugger validado.
- [ ] Modbus TCP validado.
- [ ] Modbus RTU validado o pendiente explícito.

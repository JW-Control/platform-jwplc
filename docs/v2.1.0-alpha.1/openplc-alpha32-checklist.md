# Checklist alpha32 - OpenPLC Integration

Fecha: 2026-05-23  
Branch sugerido: `develop/alpha32-openplc-integration`  
Versión sugerida: `v2.1.0-alpha.1`

## Identificación

- [x] Objetivo definido: integrar OpenPLC sin romper Arduino normal.
- [x] Base estable: `JWPLC Basic v2.0.0`.
- [x] OpenPLC v4 seleccionado.
- [x] Integración definida como externa/opcional.
- [x] `platform-jwplc` no modificado.

## Archivos OpenPLC

- [x] `hals.json` actualizado.
- [x] `jwplcbasic.cpp` actualizado.
- [x] `ModbusSlave.cpp` adaptado para JWPLC Basic + `JWPLC_Ethernet`.
- [x] `ModbusSlave.h` adaptado para JWPLC Basic + `EthernetServer`.
- [x] Imagen preview agregada.
- [ ] Guardar patch final organizado.
- [ ] Guardar proyecto ejemplo.

## Compilación y subida

- [x] OpenPLC Editor reconoce `JWPLC BASIC [2.0.0]`.
- [x] Compilación exitosa.
- [x] Subida por USB exitosa.
- [x] Debugger de OpenPLC funcional.
- [x] No se modificó `platform.txt`.
- [x] No se modificó `boards.txt`.
- [x] No se modificó core `jwcontrol`.

## I/O

- [x] `Q0_0` blink desde Ladder.
- [x] Lectura de entradas desde OpenPLC.
- [x] Activación de salidas desde OpenPLC.
- [x] Dependencia entrada-salida validada.
- [x] Concordancia con TFT.
- [ ] Validar mapa completo `I0_0..I0_7` y `Q0_0..Q0_7` con proyecto dedicado.

## Modbus TCP

- [x] DHCP funcional.
- [x] IP detectada en router.
- [x] Ping OK.
- [x] Puerto 502 abierto.
- [x] ModbusTool conecta.
- [x] FC01 Read Coils OK.
- [x] FC02 Read Discrete Inputs OK.
- [x] Desconexión/reconexión RJ45 no congela.
- [x] Arranque con RJ45 conectado validado.
- [x] Arranque sin RJ45 y conexión posterior validado.

## Modbus RTU

- [x] RTU solo funciona.
- [ ] RTU + TCP simultáneo pendiente: TCP funciona, RTU no trabaja correctamente.

## Configuración final

- [ ] Dejar `JWPLC_MBTCP_DEBUG = 0`.
- [ ] Definir MAC final recomendada.
- [ ] Definir si RTU+TCP simultáneo queda pendiente o fuera de alcance.
- [ ] Crear guía final de usuario.
- [ ] Crear PR en español si se versiona el patch.
- [ ] Crear release/pre-release o paquete del patch si aplica.

## Conclusión

La integración OpenPLC queda funcionalmente validada para:

```txt
OpenPLC Editor v4 + JWPLC Basic v2.0.0 + Arduino CLI + Modbus TCP + I/O digital.
```

Pendiente principal:

```txt
RTU + TCP simultáneo.
```

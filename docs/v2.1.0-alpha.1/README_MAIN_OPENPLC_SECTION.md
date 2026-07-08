# Sección propuesta para README principal

Agregar esta sección en `README.md` después de **Modbus RTU base** o antes del bloque histórico de OpenPLC/Modbus.

```md
## Compatibilidad con OpenPLC Editor v4

A partir de `alpha32-openplc-integration`, el **JWPLC Basic v2.0.0** fue validado como target externo para **OpenPLC Editor v4**.

Esta integración no modifica el package Arduino estable `platform-jwplc v2.0.0`. Se entrega como un patch externo para OpenPLC Editor, ubicado en:

```txt
docs/alpha32_openplc_integration/open-plc-editor/
```

### Estado validado

- OpenPLC Editor reconoce `JWPLC BASIC [2.0.0]`.
- Compilación desde OpenPLC usando Arduino CLI.
- Subida por USB al JWPLC Basic.
- Debugger de OpenPLC operativo.
- Pin Mapping compatible con `I0_0..I0_7` y `Q0_0..Q0_7`.
- Lectura de entradas digitales.
- Activación de salidas digitales.
- Concordancia entre OpenPLC, E/S físicas y TFT.
- Modbus TCP por Ethernet W5500.
- DHCP y puerto TCP 502 validados.
- Pruebas con ModbusTool como master TCP.

### Limitación conocida

Modbus RTU y Modbus TCP fueron validados de forma independiente:

```txt
RTU solo: funcional.
TCP solo: funcional.
RTU + TCP simultáneo: TCP funciona; RTU queda pendiente de revisión.
```

### Instalación del patch OpenPLC

La guía y los scripts de instalación están en:

```txt
docs/alpha32_openplc_integration/
```

Para instalación rápida en Windows:

```txt
docs/alpha32_openplc_integration/installer/install_openplc_jwplc_patch.bat
```

El instalador crea un backup automático y copia los archivos modificados sobre la carpeta local de OpenPLC Editor.

### Mapa Modbus TCP validado

| OpenPLC | Modbus TCP | JWPLC Basic |
|---|---:|---|
| `%IX0.0` | Discrete Input 0 | `I0_0` |
| `%IX0.1` | Discrete Input 1 | `I0_1` |
| `%QX0.0` | Coil 0 | `Q0_0` |
| `%QX0.1` | Coil 1 | `Q0_1` |

Ver mapa completo en:

```txt
docs/alpha32_openplc_integration/openplc-io-map.md
```

> OpenPLC queda como integración externa/opcional. El uso normal del JWPLC Basic desde Arduino IDE sigue funcionando sin requerir OpenPLC.
```

## Nota adicional

En el README principal también se recomienda cambiar frases antiguas como:

```txt
OpenPLC no está integrado todavía.
```

por:

```txt
OpenPLC está validado como integración externa/opcional mediante patch para OpenPLC Editor v4.
```

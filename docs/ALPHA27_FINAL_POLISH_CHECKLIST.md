# Alpha27 Final Polish Checklist

Objetivo de `alpha27-final-polish`: preparar el package JWPLC 2.0.0 para una primera beta funcional, sin añadir periféricos nuevos.

Esta alpha se enfoca en limpieza, documentación, validación de ejemplos, reducción de variantes y revisión de instalación limpia.

---

## 1. Alcance

### Sí entra en alpha27

- Actualizar README principal del repositorio.
- Añadir tabla de compatibilidad por placa.
- Revisar `library.properties` de librerías internas.
- Ordenar nombres y enfoque de ejemplos.
- Definir checklist de instalación limpia.
- Reducir el package a placas objetivo:
  - ESP32 Base Board.
  - JWPLC Basic.
  - JWPLC Basic Core.
- Preparar el camino para `2.0.0-beta.1`.

### No entra en alpha27

- Nuevos periféricos.
- Optimización profunda de Ethernet throughput.
- Modbus TCP.
- Web server.
- MCP / CLI.
- OpenPLC avanzado.
- Soporte para ESP32-S2/S3/C3/C6/H2, salvo que una variante JWPLC real lo requiera.

---

## 2. README principal

Estado esperado:

- Explicar enfoque compacto del package.
- Explicar placas objetivo.
- Explicar diferencias entre `ESP32 Base Board`, `JWPLC Basic` y `JWPLC Basic Core`.
- Incluir tabla de compatibilidad.
- Incluir APIs globales:
  - `JWPLC_Display`
  - `JWPLC_Ethernet`
  - `JWPLC_SD`
  - `JWPLC_FRAM`
  - `JWPLC_RTC`
  - `JWPLC_Buttons`
- Incluir reglas de coexistencia SPI.
- Incluir checklist rápido antes de beta.

---

## 3. Tabla de compatibilidad esperada

| Periférico / API | ESP32 Base Board | JWPLC Basic | JWPLC Basic Core |
|---|---:|---:|---:|
| `JWPLC_Display` | No automático | Sí | Sí, según configuración |
| `JWPLC_Buttons` | No automático | Sí | Sí, según configuración |
| `JWPLC_RTC` | No automático | Sí | Según configuración |
| `JWPLC_FRAM` | No automático | Sí | Disabled / size 0 |
| `JWPLC_SD` | No automático | Sí | Disabled |
| `JWPLC_Ethernet` | No automático | Sí | Disabled |
| TCA / IO industrial | No automático | Sí | Según variante/configuración |

---

## 4. Revisión de `library.properties`

Revisar mínimo:

- `JWPLC_Display/library.properties`
- `JWPLC_GlobalPeripherals/library.properties`
- `JWPLC_Ethernet/library.properties`

Criterios:

- `maintainer` con correo de JW Control.
- `sentence` breve y clara.
- `paragraph` actualizado al estado real del package.
- Dependencias coherentes.
- Evitar que `JWPLC_Display` dependa directamente de Ethernet si Ethernet ya entra por `JWPLC_GlobalPeripherals`.

Dependencia esperada:

```txt
JWPLC_Display -> JWPLC_GlobalPeripherals
JWPLC_GlobalPeripherals -> JW_RTC,JW_FRAM,JW_MatrixButtons,JW_SD,JWPLC_Ethernet
JWPLC_Ethernet -> Ethernet
```

---

## 5. Nombres de ejemplos

Criterios:

- Evitar nombres temporales tipo `sketch_apr29a`, `TFT_SD_04`, etc.
- Usar prefijos por periférico:
  - `Display_...`
  - `Ethernet_...`
  - `SD_...`
  - `FRAM_...`
  - `RTC_...`
- Mantener ejemplos cortos, probables y repetibles.

Ejemplos recomendados actuales:

### Display

- `Display_DotAPI_Minimal`
- `Display_UserUI_Callbacks`
- `Display_Efficient_Redraw`
- `Display_Idle_Return_Modes`

### Ethernet

- `Ethernet_Auto_DHCP_Status`
- `Ethernet_Auto_StaticIP_Status`
- `Ethernet_Display_Status`
- `Ethernet_SPI_Coexistence`

---

## 6. Checklist de instalación limpia

Validar desde Arduino IDE usando el package instalado, no solo la carpeta local del repositorio.

### Instalación

- [ ] Agregar URL del package en Arduino IDE.
- [ ] Instalar versión alpha27 desde Boards Manager.
- [ ] Confirmar que solo aparecen las placas objetivo.

### Placas

- [ ] Compilar sketch vacío con `ESP32 Base Board`.
- [ ] Compilar sketch vacío con `JWPLC Basic`.
- [ ] Compilar sketch vacío con `JWPLC Basic Core`.

### Display

- [ ] Probar pantalla IDLE.
- [ ] Probar `Display_DotAPI_Minimal`.
- [ ] Probar `Display_Idle_Return_Modes`.
- [ ] Validar `IDLE_RETURN_TIMEOUT`.
- [ ] Validar `IDLE_RETURN_ESC_ONLY`.
- [ ] Validar `IDLE_RETURN_DISABLED`.

### Ethernet

- [ ] Probar `Ethernet_Auto_DHCP_Status` con RJ45 conectado.
- [ ] Probar `Ethernet_Auto_DHCP_Status` sin RJ45.
- [ ] Conectar RJ45 después del arranque.
- [ ] Desconectar/reconectar RJ45.
- [ ] Probar `Ethernet_Auto_StaticIP_Status`.
- [ ] Probar `Ethernet_Display_Status`.
- [ ] Probar `Ethernet_SPI_Coexistence`.

### SPI compartido

- [ ] Validar Display + Ethernet.
- [ ] Validar Display + SD.
- [ ] Validar Display + FRAM.
- [ ] Validar Display + Ethernet + SD + FRAM.

---

## 7. Reducción de variantes

Objetivo: mantener el package liviano y enfocado.

Placas visibles esperadas:

- ESP32 Base Board.
- JWPLC Basic.
- JWPLC Basic Core.

Eliminar/ocultar por ahora:

- ESP32-S2 Base Board.
- ESP32-S3 Base Board.
- ESP32-C3 Base Board.
- ESP32-C6 Base Board.
- ESP32-H2 Base Board.
- Variantes adicionales sin producto JWPLC asociado.

También revisar `package_jwplc_index.json` para que el campo `boards` solo liste las placas objetivo.

---

## 8. Criterio para cerrar alpha27

Alpha27 puede cerrarse cuando:

- README principal esté actualizado.
- `library.properties` esté consistente.
- Ejemplos principales compilen.
- `boards.txt` muestre solo las placas objetivo.
- `package_jwplc_index.json` de la nueva versión liste solo las placas objetivo.
- El ZIP alpha27 se instale desde Arduino IDE.
- Se confirme reducción de peso frente a alpha26.

---

## 9. Siguiente etapa sugerida

Si alpha27 queda limpio:

```txt
2.0.0-beta.1
```

Beta 1 debería enfocarse en validación de usuario final, instalación limpia y ejemplos representativos, no en agregar nuevas funciones grandes.

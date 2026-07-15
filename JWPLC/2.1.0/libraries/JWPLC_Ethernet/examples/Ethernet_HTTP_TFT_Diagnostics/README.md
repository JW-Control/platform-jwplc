# Ethernet_HTTP_TFT_Diagnostics

Ejemplo de diagnóstico integrado para **JWPLC Basic**.

## Objetivo

Comprobar en un solo sketch:

- USB Serial a `115200` baudios;
- botonera completa;
- TFT ST7789 en pantalla USER;
- RTC;
- FRAM con persistencia y lectura de verificación;
- Ethernet W5500 automático por DHCP;
- acceso HTTP a `http://example.com/` mediante `EthernetClient`.

## Controles

| Botón | Acción |
|---|---|
| `OK` | Ejecuta `GET http://example.com/`. |
| `LEFT` / `RIGHT` | Cambia entre la página HTTP y la página de botonera/FRAM. |
| `UP` / `DOWN` | Incrementa o decrementa un valor persistente en FRAM. |
| `ESC` | Regresa a la pantalla IDLE. |

## Resultado HTTP esperado

Con RJ45 conectado, DHCP válido y salida a Internet:

```text
HTTP/1.1 200 OK
Titulo: Example Domain
```

La pantalla muestra también IP local, fecha/hora del RTC, contador de arranques, consultas realizadas, consultas correctas y bytes recibidos.

## FRAM

El ejemplo reserva para su bloque de diagnóstico la dirección:

```text
0x0100
```

Guarda:

- contador de arranques;
- total de consultas HTTP;
- total de consultas correctas;
- valor modificable con `UP` / `DOWN`;
- último código HTTP;
- instante Unix de la última prueba cuando el RTC es válido.

## Consideraciones SPI

El W5500 comparte SPI con TFT, FRAM y microSD. La consulta realizada con `EthernetClient` toma el mutex SPI global del JWPLC durante la transacción. Por ello, la TFT puede dejar de refrescar brevemente mientras se espera la respuesta HTTP; al terminar se redibuja con el resultado.

Los callbacks gráficos no consultan Ethernet, RTC ni FRAM directamente: solo dibujan valores cacheados por `loop()`.

## Validación manual

- [ ] Aparece la pantalla USER de diagnóstico.
- [ ] El monitor Serial recibe mensajes a `115200`.
- [ ] La IP DHCP deja de ser `0.0.0.0`.
- [ ] El RTC muestra fecha y hora válidas.
- [ ] `UP` y `DOWN` modifican el valor FRAM.
- [ ] El valor FRAM y el contador de arranques sobreviven a un reinicio.
- [ ] `LEFT` y `RIGHT` cambian de página.
- [ ] `ESC` regresa a IDLE y cualquier botón vuelve a USER.
- [ ] `OK` devuelve un código HTTP válido y el título `Example Domain`.

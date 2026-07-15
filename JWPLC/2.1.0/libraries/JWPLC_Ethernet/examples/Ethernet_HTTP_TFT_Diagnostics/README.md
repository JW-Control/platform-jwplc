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
| `UP` / `DOWN` | Incrementa o decrementa el valor y lo guarda inmediatamente en FRAM. |
| `ESC` | Regresa a la pantalla IDLE. |

## Resultado HTTP esperado

Con RJ45 conectado, DHCP válido y salida a Internet:

```text
HTTP/1.1 200 OK
Titulo: Example Domain
```

La pantalla muestra también IP local, fecha/hora del RTC, contador de arranques, consultas realizadas, consultas correctas y bytes recibidos.

## Guardado del valor FRAM

No existe un botón adicional de guardado. Cada pulsación de `UP` o `DOWN` realiza automáticamente:

1. modificación de `framValue`;
2. escritura del bloque mediante `JWPLC_FRAM.writeBlock()`;
3. lectura inmediata mediante `JWPLC_FRAM.readBlock()`;
4. comparación del bloque leído con el bloque enviado.

Cuando todo es correcto, la pantalla muestra:

```text
Persistencia: GUARDADO OK
```

El monitor Serial muestra:

```text
FRAM value: <valor> | guardado y verificado
```

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

## Refresco de la TFT

La pantalla usa un sistema de regiones pendientes de actualización. El callback gráfico retorna inmediatamente cuando no cambió ningún dato y solo limpia/redibuja la fila afectada cuando cambia Ethernet, RTC, FRAM, HTTP, botonera o estado de error.

La pantalla completa únicamente se reconstruye al entrar en USER o cambiar de página. Esto evita el parpadeo producido por limpiar toda el área cada `100 ms`.

## Indicador ERR

`ERR` se enciende únicamente por alguno de estos motivos:

- FRAM no disponible o fallo de escritura/verificación;
- hardware RTC ausente;
- consulta HTTP intentada y fallida.

Una hora RTC inválida se muestra en amarillo como `HORA INVALIDA`, pero no enciende `ERR` mientras el RTC responda físicamente.

La causa activa aparece en ambas páginas como, por ejemplo:

```text
ERR: FRAM
ERR: RTC AUSENTE
ERR: HTTP
ERR: NINGUNO
```

Por tanto, un resultado `HTTP 200 OK` no apaga una falla independiente de FRAM o de presencia del RTC; la pantalla indica cuál es la causa concreta.

## Consideraciones SPI

El W5500 comparte SPI con TFT, FRAM y microSD. La consulta realizada con `EthernetClient` toma el mutex SPI global del JWPLC durante la transacción. Por ello, la TFT puede dejar de refrescar brevemente mientras se espera la respuesta HTTP; al terminar solo se actualizan las filas relacionadas con el resultado.

Los callbacks gráficos no consultan Ethernet, RTC ni FRAM directamente: solo dibujan valores cacheados por `loop()`.

## Validación manual

- [ ] Aparece la pantalla USER de diagnóstico.
- [ ] El monitor Serial recibe mensajes a `115200`.
- [ ] La IP DHCP deja de ser `0.0.0.0`.
- [ ] El RTC muestra fecha y hora válidas o una advertencia explícita.
- [ ] `UP` y `DOWN` modifican y guardan automáticamente el valor FRAM.
- [ ] Aparece `Persistencia: GUARDADO OK`.
- [ ] El valor FRAM y el contador de arranques sobreviven a un reinicio.
- [ ] `LEFT` y `RIGHT` cambian de página.
- [ ] La TFT no parpadea durante el refresco normal.
- [ ] `ESC` regresa a IDLE y cualquier botón vuelve a USER.
- [ ] `OK` devuelve un código HTTP válido y el título `Example Domain`.
- [ ] `ERR` muestra una causa concreta o `NINGUNO`.

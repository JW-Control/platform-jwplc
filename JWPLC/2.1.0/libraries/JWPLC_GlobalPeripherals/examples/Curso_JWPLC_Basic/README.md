# Curso JWPLC Basic

Un curso paso a paso para programar el **JWPLC Basic** con Arduino IDE,
pensado para quien nunca ha programado un microcontrolador. Cada lección es
un ejemplo que se abre desde:

```text
Archivo → Ejemplos → JWPLC_GlobalPeripherals → Curso_JWPLC_Basic
```

Las lecciones van de lo simple a lo completo: primero se aprende una idea
con pocas líneas y después se usa en un ejemplo más grande. Cada una explica
al inicio qué vas a aprender, qué conectar, cómo probarla y un reto para
practicar.

Placa: **JWPLC Basic** (Herramientas → Placa → JW Control ESP32 Boards).

## Antes de empezar

- Entradas `I0.0` a `I0.7`: 24 V DC, sinking, para sensores PNP. El común de
  los sensores (`1M`) va a 0 V; la entrada se activa cuando recibe +24 V.
- Salidas `Q0.0` a `Q0.7`: relés de contacto seco de 5 A (hasta 250 V AC o
  30 V DC), con su `COM` y su `NO`.
- En el programa se llaman `I0_0` … `I0_7` y `Q0_0` … `Q0_7`. Al arrancar ya
  quedan listas: no hace falta `pinMode()`.

## Hoja de ruta

| Unidad | Lección | Qué se aprende |
|---|---|---|
| **1. Primeros pasos** | 01.Hola_JWPLC | `setup()`, `loop()` y el Monitor Serie |
| | 02.Encender_un_rele | `digitalWrite()`, `HIGH`/`LOW` y `delay()` |
| | 03.Leer_una_entrada | `digitalRead()` y el cableado de un pulsador o sensor |
| | 04.Variables_y_decisiones | `bool`, `if`/`else` y la lógica Y, O, NO |
| | 05.Las_8_entradas_y_salidas | listas (arreglos) y el bucle `for` |
| **2. El tiempo y la lógica** | 06.Tiempo_sin_delay | `millis()`: esperar sin detener el programa |
| | 07.Flancos_un_pulsador | detectar el instante de la pulsación (telerruptor) |
| | 08.Marcha_y_parada | enclavamiento con pulsador de marcha y de parada |
| | 09.Temporizadores | retardo a la conexión y a la desconexión |
| | 10.Contadores | contar piezas y avisar al llegar a una cantidad |
| | 11.Maquina_de_estados | `enum` y `switch`: un semáforo |
| **3. Pantalla y botonera** | 12.Botonera | las 6 teclas del frente |
| | 13.Pantalla_de_inicio | la pantalla de estado y sus indicadores |
| | 14.Mi_primera_pantalla | campos de valor, texto e indicador |
| | 15.Ajustar_un_valor | cambiar un tiempo con las teclas |
| | 16.Guardar_los_ajustes | conservar los ajustes al apagar (FRAM) |
| **4. Reloj, memoria y registro** | 17.Reloj | leer y mostrar la fecha y la hora |
| | 18.Poner_en_hora | ajustar el reloj desde la PC o la botonera |
| | 19.Programar_por_horario | encender y apagar a una hora |
| | 20.Horometro_en_FRAM | horas de trabajo que sobreviven a un corte |
| | 21.microSD_primer_archivo | escribir y leer un archivo de texto |
| | 22.Registro_CSV | mediciones con fecha y hora, un archivo por día |
| **5. Comunicaciones** | 23.Modbus_RTU_esclavo | el Basic responde a un SCADA o a otro PLC |
| | 24.Modbus_RTU_maestro | el Basic lee y manda a otro equipo |
| | 25.Sensor_RS485 | leer un transmisor de temperatura por Modbus |
| | 26.Ethernet_conectar | entrar a la red con DHCP o IP fija |
| | 27.Modbus_TCP_servidor | el Basic en la red de la planta |
| | 28.WiFi_pagina_web | ver las entradas y salidas desde el celular |
| **6. Proyectos** | 29.Proyecto_Porton | portón con finales de carrera y fotocelda |
| | 30.Proyecto_Riego | riego de 4 zonas por horario |
| | 31.Proyecto_Bombeo_alternado | dos bombas que se turnan y se respaldan |
| | 32.Proyecto_Granja_avicola | ventilación, calefacción, comedero y registro |

## Probar sin la placa

Las lecciones se pueden correr en el simulador **JWPLC Sim**. Las pruebas
automáticas de cada lección están en `tools/curso-jwplc-basic/` del
repositorio del package.

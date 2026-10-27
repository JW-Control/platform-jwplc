# Puesta en marcha

Proyecto: ______________________ · Versión: ______ · Fecha: __________ · Técnico: ______________

Marcar cada punto. Lo que no aplique, tachar con «N/A».

## 1. Antes de energizar

- [ ] Tensión de alimentación del JWPLC correcta y protegida.
- [ ] Cableado de entradas y salidas igual a `docs/MAPA_IO.md`.
- [ ] Contactos de seguridad (emergencia, paro, térmicos) cableados NC.
- [ ] Cargas de las salidas dentro de la capacidad del relé (o con relé/contactor intermedio).
- [ ] RS485: A/B correctos, resistencia de terminación si es extremo del bus.
- [ ] Ethernet: cable probado; IP anotada en `MAPA_IO.md`.

## 2. Carga del programa

- [ ] `PROYECTO_VERSION` en `A_Config.h` igual a la última entrada de `docs/CAMBIOS.md`.
- [ ] Placa seleccionada: JWPLC Basic. Compila sin errores.
- [ ] Programa cargado; el monitor serie muestra nombre y versión del proyecto.
- [ ] Copia del proyecto (carpeta completa con `docs/`) guardada en: ______________________

## 3. Pruebas sin carga (motores desconectados o con breaker abierto)

- [ ] Cada entrada cambia en el monitor serie (`IN=0x..`) al accionarla.
- [ ] Cada salida activa su relé cuando la lógica lo pide (`OUT=0x..`).
- [ ] Emergencia apaga todas las salidas de marcha y muestra ERR en pantalla.
- [ ] Reconocimiento de alarmas (ESC o Modbus) funciona y la alarma vuelve si la causa sigue.
- [ ] Tiempo de ciclo (`max=` en el monitor serie) estable y bajo.

## 4. Pruebas con carga

- [ ] Secuencia normal completa (marcha, paro) con la máquina real.
- [ ] Cada alarma probada con su causa real.
- [ ] Corte de energía: al volver, las salidas arrancan apagadas y los datos de FRAM se conservan.
- [ ] Comunicación con el SCADA/HMI externo: lectura y escritura de cada registro del mapa.

## 5. Entrega

- [ ] `docs/CAMBIOS.md` actualizado.
- [ ] Copia final entregada al cliente / archivo de JW Control.

Observaciones:

______________________________________________________________________________

Firma técnico: ________________ · Firma cliente: ________________

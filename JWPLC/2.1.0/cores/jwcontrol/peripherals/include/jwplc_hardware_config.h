#ifndef JWCONTROL_PERIPHERALS_INCLUDE_JWPLC_HARDWARE_CONFIG_FORWARD_H
#define JWCONTROL_PERIPHERALS_INCLUDE_JWPLC_HARDWARE_CONFIG_FORWARD_H

// Alpha5: compatibilidad de headers JWPLC desde el core ESP32 generico.
//
// platform.local.txt expone este directorio como include publico comun, pero
// no debe añadir cores/jwcontrol completo al target generico porque alli existe
// otro Arduino.h. Este forwarding header permite que librerias JWPLC como
// JWPLC_Ethernet y JWPLC_RS485 encuentren la configuracion de hardware sin
// alterar la seleccion del core Arduino.
#include "../../jwplc_hardware_config.h"

#endif

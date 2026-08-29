#ifndef JWCONTROL_PERIPHERALS_INCLUDE_JWPLC_PERIPHERALS_FORWARD_H
#define JWCONTROL_PERIPHERALS_INCLUDE_JWPLC_PERIPHERALS_FORWARD_H

// Alpha5: compatibilidad de headers JWPLC desde el core ESP32 generico.
//
// El directorio peripherals/include ya forma parte del include path comun.
// Se reexportan aqui las declaraciones publicas del runtime JWPLC sin añadir
// cores/jwcontrol completo, evitando que Arduino.h del core JWPLC pueda
// sombrear Arduino.h del core ESP32 generico.
#include "../../jwplc_peripherals.h"

#endif

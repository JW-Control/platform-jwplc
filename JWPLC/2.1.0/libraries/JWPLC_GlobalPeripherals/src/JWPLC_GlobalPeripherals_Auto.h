#ifndef JWPLC_GLOBAL_PERIPHERALS_AUTO_H
#define JWPLC_GLOBAL_PERIPHERALS_AUTO_H

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

// This header belongs to JWPLC_GlobalPeripherals so Arduino Builder can
// discover the library without expanding its full public header repeatedly.
// During discovery only empty, package-owned marker headers are included.
// During the normal build the complete public API remains available.
#if JWPLC_LIBRARY_DISCOVERY_PHASE
#include <JWPLC_Autoload_JW_RTC.h>
#include <JWPLC_Autoload_JW_FRAM.h>
#include <JWPLC_Autoload_JW_SD.h>
#include <JWPLC_Autoload_JW_MatrixButtons.h>
#include <JWPLC_Autoload_JWPLC_Ethernet.h>
#include <JWPLC_Autoload_JWPLC_RS485.h>
#include <JWPLC_Autoload_JWPLC_ModbusRTU.h>
#include <JWPLC_Autoload_SPI.h>
#include <JWPLC_Autoload_Wire.h>
#include <JWPLC_Autoload_SD.h>
#else
#include <JWPLC_GlobalPeripherals.h>
#endif

#endif // JWPLC_GLOBAL_PERIPHERALS_AUTO_H

#ifndef JWCONTROL_JWPLC_HARDWARE_CONFIG_H
#define JWCONTROL_JWPLC_HARDWARE_CONFIG_H

// =====================================================
// Configuración de hardware JWPLC
// =====================================================
// Estos flags permiten compilar el core para distintas variantes
// físicas del JWPLC Basic.
//
// Por defecto, todos los periféricos opcionales quedan habilitados.
// Luego boards.txt podrá sobrescribir estos valores con -D si una
// variante de placa no incluye algún periférico.
//
// Variante principal:
//   JWPLC Basic
//
// Variante económica/esencial propuesta:
//   JWPLC Basic Core
// =====================================================


// =====================================================
// Real Time Clock
// =====================================================

#ifndef JWPLC_HAS_RTC
#define JWPLC_HAS_RTC 1
#endif


// =====================================================
// FRAM
// =====================================================

#ifndef JWPLC_HAS_FRAM
#define JWPLC_HAS_FRAM 1
#endif

#ifndef JWPLC_FRAM_SIZE_BYTES
#define JWPLC_FRAM_SIZE_BYTES 0
#endif

// =====================================================
// Buzzer (piezo pasivo, GPIO26 en JWPLC Basic v2)
// =====================================================
// Lo usa JWPLC_Buzzer (JWPLC_GlobalPeripherals). El pin se toma con LEDC sólo
// en el primer uso; si nadie usa el buzzer, el GPIO26 queda sin configurar.

#ifndef JWPLC_HAS_BUZZER
#define JWPLC_HAS_BUZZER 1
#endif

#ifndef JWPLC_BUZZER_PIN
#define JWPLC_BUZZER_PIN 26
#endif

// =====================================================
// Botonera (matriz 2 filas x 3 columnas, JWPLC Basic v2)
// =====================================================
// La escanea JWPLC_Buttons (JWPLC_GlobalPeripherals) desde el arranque.
//   fila 0: BTN_LEFT, BTN_UP, BTN_RIGHT
//   fila 1: BTN_ESC,  BTN_OK, BTN_DOWN
// Las columnas son GPIO sólo entrada (pull-up externo).

#ifndef JWPLC_BTN_ROW0_PIN
#define JWPLC_BTN_ROW0_PIN 12
#endif

#ifndef JWPLC_BTN_ROW1_PIN
#define JWPLC_BTN_ROW1_PIN 2
#endif

#ifndef JWPLC_BTN_COL0_PIN
#define JWPLC_BTN_COL0_PIN 35
#endif

#ifndef JWPLC_BTN_COL1_PIN
#define JWPLC_BTN_COL1_PIN 34
#endif

#ifndef JWPLC_BTN_COL2_PIN
#define JWPLC_BTN_COL2_PIN 36
#endif

#define JWPLC_BTN_ROWS 2
#define JWPLC_BTN_COLS 3

// =====================================================
// microSD
// =====================================================

#ifndef JWPLC_HAS_SD
#define JWPLC_HAS_SD 0
#endif

#ifndef JWPLC_SD_CS
#define JWPLC_SD_CS 32
#endif

#ifndef JWPLC_SD_DETECT_PIN
#define JWPLC_SD_DETECT_PIN 39
#endif

#ifndef JWPLC_SD_DETECT_ACTIVE_LOW
#define JWPLC_SD_DETECT_ACTIVE_LOW 1
#endif

#ifndef JWPLC_SPI_SD_HZ
#define JWPLC_SPI_SD_HZ 20000000UL
#endif

// =====================================================
// Ethernet
// =====================================================

#ifndef JWPLC_HAS_ETHERNET
#define JWPLC_HAS_ETHERNET 1
#endif

// INTn del W5500 está cableado al GPIO15 en JWPLC Basic v2.
// Es una línea activa en LOW reservada al runtime Ethernet; no forma parte
// de la API de E/S de usuario aunque el core ESP32 conserve aliases genéricos
// para GPIO15 por compatibilidad.
#ifndef JWPLC_ETH_INT_PIN
#define JWPLC_ETH_INT_PIN 15
#endif

// =====================================================
// RS-485
// =====================================================

#ifndef JWPLC_HAS_RS485
#if defined(JWPLC_BASIC) || defined(JWPLC_BASIC_CORE) || \
    defined(ARDUINO_JWPLCBASIC) || defined(ARDUINO_JWPLCBASICCORE) || \
    defined(ARDUINO_JWPLC_BASIC) || defined(ARDUINO_JWPLC_BASIC_CORE)
#define JWPLC_HAS_RS485 1
#else
#define JWPLC_HAS_RS485 0
#endif
#endif

#ifndef JWPLC_RS485_RX_PIN
#define JWPLC_RS485_RX_PIN 16
#endif

#ifndef JWPLC_RS485_TX_PIN
#define JWPLC_RS485_TX_PIN 17
#endif

// JWPLC Basic v2 usa MAX13487E con AutoDirection. Al estar habilitado,
// la UART puede encolar TX sin que el ESP32 tenga que gobernar DE/RE.
#ifndef JWPLC_RS485_AUTO_DIRECTION
#define JWPLC_RS485_AUTO_DIRECTION 1
#endif

// Buffer suficiente para encolar una ADU Modbus RTU maxima sin esperar
// fisicamente a que cada byte abandone la UART.
#ifndef JWPLC_RS485_TX_BUFFER_SIZE
#define JWPLC_RS485_TX_BUFFER_SIZE 512U
#endif

#endif // JWCONTROL_JWPLC_HARDWARE_CONFIG_H
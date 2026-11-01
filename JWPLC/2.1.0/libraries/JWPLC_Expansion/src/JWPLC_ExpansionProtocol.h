// Protocolo JWPLC Remote I/O v2 (módulos de expansión).
//
// Extiende el v1 de docs/v2.1.0-alpha.3/JWPLC_REMOTE_IO_RTU_PROTOCOL.md. Lo
// usan el esclavo (JWPLC_ExpansionSlave) y el maestro (JWPLC_Runtime). Todo
// queda dentro de 0..255 por compatibilidad con OpenPLC.
//
//   Discrete inputs (FC02)   0..7     entradas del módulo
//   Coils (FC01/05/15)       0..7     salidas del módulo
//   Holding (FC03/06/16)     224..239 configuración
//   Input regs (FC04)        240..255 identificación (solo lectura)
//
// Diseño: docs/v2.1.0-alpha.16/A16_RT2_EXPANSION_DISENO_20261101.md
#pragma once

#include <stdint.h>

// Valores de fábrica: el módulo sin configurar responde en el ID 247.
#define JWX_FACTORY_ID 247
#define JWX_MAX_ID 246
#define JWX_FACTORY_BAUD_CODE 4   // 115200
#define JWX_FACTORY_FRAME_CODE 0  // 8N1
#define JWX_FACTORY_WATCHDOG 10   // x 100 ms = 1,0 s
#define JWX_MIN_WATCHDOG 10
#define JWX_MAX_WATCHDOG 50

#define JWX_UNLOCK_KEY 0xA55A
#define JWX_PROTOCOL_VERSION 0x0002
#define JWX_PRODUCT_BASIC 0x0001  // JWPLC Basic como módulo DI8/DO8

// Holding registers (configuración).
#define JWX_HR_KEY 224
#define JWX_HR_ID 225
#define JWX_HR_BAUD 226
#define JWX_HR_COMMAND 227
#define JWX_HR_PARAM 228
#define JWX_HR_RESULT 229
#define JWX_HR_FRAME 230
#define JWX_HR_SAFE_VALUE 231
#define JWX_HR_SAFE_HOLD 232
#define JWX_HR_WATCHDOG 233
#define JWX_HR_FLAGS 234
#define JWX_HR_LINK_HI 235
#define JWX_HR_LINK_LO 236
#define JWX_HR_FIRST 224
#define JWX_HR_COUNT 13  // 224..236

// Comandos (registro 227).
#define JWX_CMD_SAVE_ID 1
#define JWX_CMD_SAVE_BAUD 2
#define JWX_CMD_FACTORY 3
#define JWX_CMD_REBOOT 4
#define JWX_CMD_SAVE_ALL 5

// Resultado (registro 229).
#define JWX_RES_OK 0
#define JWX_RES_BAD_KEY 1
#define JWX_RES_RANGE 2
#define JWX_RES_LOCKED 3
#define JWX_RES_FRAM 4
#define JWX_RES_BUSY 0xFFFF  // comando recibido, todavía sin procesar

// Banderas (registro 234).
#define JWX_FLAG_CONFIGURED 0x0001
#define JWX_FLAG_LINKED 0x0002
#define JWX_FLAG_LOCKED 0x0004

// Input registers (identificación).
#define JWX_IR_MAGIC1 240  // 0x4A57 "JW"
#define JWX_IR_MAGIC2 241  // 0x504C "PL"
#define JWX_IR_PROTOCOL 242
#define JWX_IR_PRODUCT 243
#define JWX_IR_FIRMWARE 244  // mayor en el byte alto, menor en el bajo
#define JWX_IR_IO 245        // DI en el byte alto, DO en el bajo
#define JWX_IR_CAPS 246
#define JWX_IR_STATUS 247    // estado en el byte alto, ID en el bajo
#define JWX_IR_MAC01 248
#define JWX_IR_MAC23 249
#define JWX_IR_MAC45 250
#define JWX_IR_UID_HI 251
#define JWX_IR_UID_LO 252
#define JWX_IR_CFG_CRC 253
#define JWX_IR_FIRST 240
#define JWX_IR_COUNT 16

#define JWX_MAGIC1 0x4A57
#define JWX_MAGIC2 0x504C

// Capacidades (registro 246).
#define JWX_CAP_DIGITAL_IO 0x0001
#define JWX_CAP_ID_BY_MODBUS 0x0002
#define JWX_CAP_FRAM 0x0010
#define JWX_CAP_DISPLAY 0x0020
#define JWX_CAP_SAFE_STATE 0x0040
#define JWX_CAP_LOCKED 0x0080

// Estado (byte alto del registro 247).
#define JWX_STATUS_CONFIGURED 1
#define JWX_STATUS_FACTORY 5
#define JWX_STATUS_LOCKED 6

// Estado seguro de cada salida.
#define SAFE_OFF 0
#define SAFE_ON 1
#define SAFE_HOLD 2

// Códigos de baudrate y trama.
static const uint32_t JWX_BAUDS[] = {9600UL, 19200UL, 38400UL, 57600UL, 115200UL};
#define JWX_BAUD_CODES 5
#define JWX_FRAME_CODES 3  // 0 = 8N1, 1 = 8E1, 2 = 8O1

static inline const char *jwxFrameName(uint8_t code)
{
    return code == 1 ? "8E1" : (code == 2 ? "8O1" : "8N1");
}

/*
  A_Config.h · Datos del proyecto y parámetros fijos

  QUÉ VA AQUÍ:    nombre y versión del proyecto, qué servicios se usan (Modbus, Serial)
                  y constantes que no cambian en marcha (tiempos, direcciones, límites).
  QUÉ NO VA AQUÍ: variables que cambian durante el ciclo (B_Tags.h) ni consignas que
                  el operador ajusta (I_Persistencia).

  La configuración de hardware (SPI, expansor de E/S, Ethernet, pantalla) la hace el
  package JWPLC: no se declara aquí.
*/
#pragma once

// ---------------------------------------------------------------------------
// Identificación del proyecto (cópiala también en docs/CAMBIOS.md)
// ---------------------------------------------------------------------------
#define PROYECTO_NOMBRE   "Plantilla JWPLC"
#define PROYECTO_CLIENTE  "-"
#define PROYECTO_VERSION  "1.0.0"
#define PROYECTO_AUTOR    "-"

// ---------------------------------------------------------------------------
// Servicios (1 = activado, 0 = desactivado)
// ---------------------------------------------------------------------------
#define USAR_SERIAL_DIAGNOSTICO 1  // mensajes por el monitor serie (J_Diagnostico)
#define USAR_MODBUS_RTU         0  // esclavo Modbus RTU por RS485 (H_Comunicaciones)
#define USAR_MODBUS_TCP         0  // servidor Modbus TCP por Ethernet (H_Comunicaciones)

// ---------------------------------------------------------------------------
// Parámetros de comunicaciones
// ---------------------------------------------------------------------------
static constexpr uint32_t SERIAL_BAUDIOS     = 115200;
static constexpr uint8_t  MODBUS_RTU_ID      = 1;
static constexpr uint32_t MODBUS_RTU_BAUDIOS = 9600;
static constexpr uint32_t MODBUS_RTU_FORMATO = SERIAL_8N1;
static constexpr uint8_t  MODBUS_TCP_UNIT_ID = 1;
static constexpr uint16_t MODBUS_TCP_PUERTO  = 502;

// ---------------------------------------------------------------------------
// Tiempos del proyecto (ms)
// ---------------------------------------------------------------------------
static constexpr uint32_t T_ANTIRREBOTE_MS      = 30;    // filtro de entradas
static constexpr uint32_t T_DIAGNOSTICO_MS      = 1000;  // periodo de mensajes por Serial
static constexpr uint32_t T_GUARDADO_MINIMO_MS  = 2000;  // separación mínima entre escrituras en FRAM

// ---------------------------------------------------------------------------
// Memoria FRAM: direcciones fijas de cada bloque (no las cambies con equipos en campo)
// ---------------------------------------------------------------------------
static constexpr uint32_t FRAM_DIR_PERSISTENTES = 0;  // struct Persistentes (I_Persistencia)
static constexpr uint8_t  FRAM_VERSION_DATOS    = 1;  // súbela si cambias la struct Persistentes

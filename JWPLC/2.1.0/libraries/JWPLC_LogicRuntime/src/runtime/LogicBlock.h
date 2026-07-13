#ifndef JWPLC_LOGIC_BLOCK_H
#define JWPLC_LOGIC_BLOCK_H

#include <Arduino.h>

static constexpr uint16_t JWPLC_LOGIC_NO_SOURCE = 0xFFFF;

/**
 * @brief Capacidad máxima reservada en RAM por cada instancia del runtime.
 *
 * El JWPLC Basic actual usa FRAM de 8 KiB y admite 100 bloques. Por eso el
 * build predeterminado reserva únicamente esa capacidad.
 *
 * Un hardware futuro con FRAM de 32 KiB puede compilar la misma librería con:
 *
 *   -DJWPLC_LOGIC_COMPILED_MAX_BLOCKS=400
 *
 * El formato binario y el mapa persistente no cambian al variar este límite.
 */
#ifndef JWPLC_LOGIC_COMPILED_MAX_BLOCKS
#define JWPLC_LOGIC_COMPILED_MAX_BLOCKS 100
#endif

static_assert(JWPLC_LOGIC_COMPILED_MAX_BLOCKS > 0,
              "JWPLC Logic Runtime requiere al menos un bloque compilado");
static_assert(JWPLC_LOGIC_COMPILED_MAX_BLOCKS <= 400,
              "El formato v1 soporta como maximo 400 bloques compilados");

enum class LogicBlockType : uint8_t
{
  DigitalInput = 0,
  DigitalOutput,
  Not,
  And,
  Or,
  SetReset,
  Ton
};

/**
 * @brief Definición persistible de un bloque lógico.
 *
 * Cada bloque ocupa una posición en el programa. Las fuentes apuntan a la
 * salida de bloques anteriores para mantener un orden de ejecución acíclico y
 * determinista.
 */
struct LogicBlockDefinition
{
  LogicBlockType type;
  uint16_t sourceA;
  uint16_t sourceB;
  uint16_t resource;
  uint32_t parameter;
};

/**
 * @brief Estado temporal de ejecución de un bloque.
 *
 * Este estado vive en RAM. Más adelante solo los campos declarados como
 * retentivos se persistirán en FRAM.
 */
struct LogicBlockState
{
  bool value;
  bool timing;
  uint32_t startedAtMs;
};

#endif

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
 *   -DJWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG=400
 *
 * Se conserva JWPLC_LOGIC_COMPILED_MAX_BLOCKS como constante pública para no
 * romper código existente. El formato binario y el mapa persistente no cambian
 * al variar este límite.
 */
#ifndef JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG
#define JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG 100
#endif

static_assert(JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG > 0,
              "JWPLC Logic Runtime requiere al menos un bloque compilado");
static_assert(JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG <= 400,
              "El formato v1 soporta como maximo 400 bloques compilados");

static constexpr uint16_t JWPLC_LOGIC_COMPILED_MAX_BLOCKS =
    static_cast<uint16_t>(JWPLC_LOGIC_COMPILED_MAX_BLOCKS_CONFIG);

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
 * @brief Flags persistibles de cada bloque.
 *
 * El byte ya estaba reservado en el registro binario v1. Los programas
 * existentes contienen cero y conservan exactamente su comportamiento.
 */
static constexpr uint8_t JWPLC_LOGIC_BLOCK_FLAG_NONE = 0x00;
static constexpr uint8_t JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE = 0x01;
static constexpr uint8_t JWPLC_LOGIC_BLOCK_FLAG_KNOWN_MASK =
    JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE;

/**
 * @brief Definición persistible de un bloque lógico.
 *
 * Cada bloque ocupa una posición en el programa. Las fuentes apuntan a la
 * salida de bloques anteriores para mantener un orden de ejecución acíclico y
 * determinista.
 *
 * El constructor mantiene el orden histórico de los cinco argumentos
 * existentes. El sexto argumento opcional añade flags sin romper sketches:
 *
 *   {tipo, fuenteA, fuenteB, recurso, parametro}
 *   {tipo, fuenteA, fuenteB, recurso, parametro, flags}
 */
struct LogicBlockDefinition
{
  LogicBlockType type;
  uint8_t flags;
  uint16_t sourceA;
  uint16_t sourceB;
  uint16_t resource;
  uint32_t parameter;

  constexpr LogicBlockDefinition(
      LogicBlockType typeValue = LogicBlockType::DigitalInput,
      uint16_t sourceAValue = JWPLC_LOGIC_NO_SOURCE,
      uint16_t sourceBValue = JWPLC_LOGIC_NO_SOURCE,
      uint16_t resourceValue = 0,
      uint32_t parameterValue = 0,
      uint8_t flagsValue = JWPLC_LOGIC_BLOCK_FLAG_NONE)
      : type(typeValue),
        flags(flagsValue),
        sourceA(sourceAValue),
        sourceB(sourceBValue),
        resource(resourceValue),
        parameter(parameterValue)
  {
  }

  constexpr bool isRetentive() const
  {
    return (flags & JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE) != 0;
  }
};

static_assert(sizeof(LogicBlockDefinition) == 12,
              "LogicBlockDefinition debe conservar 12 bytes");

/**
 * @brief Estado temporal de ejecución de un bloque.
 *
 * Este estado vive en RAM. Retentivos v1 solo captura el campo value de los
 * bloques SET/RESET marcados con JWPLC_LOGIC_BLOCK_FLAG_RETENTIVE. TON y los
 * demás tipos continúan siendo no retentivos.
 */
struct LogicBlockState
{
  bool value;
  bool timing;
  uint32_t startedAtMs;
};

#endif
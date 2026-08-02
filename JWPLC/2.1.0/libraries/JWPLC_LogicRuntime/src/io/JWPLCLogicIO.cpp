#include "JWPLCLogicIO.h"

#include "jwplc_peripherals.h"

JWPLCLogicIO::JWPLCLogicIO()
    : _inputBitmap(0),
      _outputBitmap(0),
      _lastCommittedOutputBitmap(0),
      _initialized(false),
      _hasCommittedOutputs(false),
      _outputWriteCount(0)
{
}

bool JWPLCLogicIO::begin()
{
#if !defined(JWPLC_BASIC)
  _initialized = false;
  return false;
#else
  for (uint8_t index = 0; index < I0_COUNT; ++index)
  {
    pinMode(I0_X[index], INPUT);
  }

  for (uint8_t index = 0; index < Q0_COUNT; ++index)
  {
    pinMode(Q0_X[index], OUTPUT);
  }

  _inputBitmap = jwplc_digitalReadBlock(I0_X, I0_COUNT);
  _outputBitmap = 0;
  _lastCommittedOutputBitmap = 0;
  _hasCommittedOutputs = false;
  _outputWriteCount = 0;
  _initialized = true;

  // Fuerza un estado físico seguro al iniciar, aunque el shadow previo sea cero.
  allOutputsOff();
  return true;
#endif
}

void JWPLCLogicIO::scanInputs()
{
  if (!_initialized)
  {
    return;
  }

#if defined(JWPLC_BASIC)
  // I0_X usa el snapshot lógico actualizado por la tarea del sistema JWPLC.
  // Esto evita ocho transacciones I2C independientes en cada scan lógico.
  _inputBitmap = jwplc_digitalReadBlock(I0_X, I0_COUNT);
#endif
}

void JWPLCLogicIO::beginOutputScan()
{
  _outputBitmap = 0;
}

bool JWPLCLogicIO::digitalInput(uint8_t index) const
{
  if (!_initialized || index >= digitalInputCount())
  {
    return false;
  }

  return (_inputBitmap & static_cast<uint8_t>(1U << index)) != 0;
}

bool JWPLCLogicIO::setDigitalOutput(uint8_t index, bool value)
{
  if (!_initialized || index >= digitalOutputCount())
  {
    return false;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << index);
  if (value)
  {
    _outputBitmap |= mask;
  }
  else
  {
    _outputBitmap &= static_cast<uint8_t>(~mask);
  }

  return true;
}

void JWPLCLogicIO::commitOutputs()
{
  if (!_initialized)
  {
    return;
  }

#if defined(JWPLC_BASIC)
  if (_hasCommittedOutputs &&
      _outputBitmap == _lastCommittedOutputBitmap)
  {
    return;
  }

  // Q0_X se escribe como un único banco y solo cuando cambia el bitmap.
  // Esto mantiene las ocho salidas coherentes y evita tráfico I2C innecesario.
  jwplc_digitalWriteBlock(Q0_X, Q0_COUNT, _outputBitmap);

  // JWPLC_readOutputs() consulta el shadow del core, no realiza otra lectura I2C.
  if (JWPLC_readOutputs() == _outputBitmap)
  {
    _lastCommittedOutputBitmap = _outputBitmap;
    _hasCommittedOutputs = true;
    ++_outputWriteCount;
  }
  else
  {
    // El siguiente scan reintentará la escritura.
    _hasCommittedOutputs = false;
  }
#endif
}

void JWPLCLogicIO::allOutputsOff()
{
  _outputBitmap = 0;
  _hasCommittedOutputs = false;
  commitOutputs();
}

uint8_t JWPLCLogicIO::digitalInputCount() const
{
#if defined(JWPLC_BASIC)
  return I0_COUNT;
#else
  return 0;
#endif
}

uint8_t JWPLCLogicIO::digitalOutputCount() const
{
#if defined(JWPLC_BASIC)
  return Q0_COUNT;
#else
  return 0;
#endif
}

uint32_t JWPLCLogicIO::outputWriteCount() const
{
  return _outputWriteCount;
}

#include "JWPLCLogicIO.h"

JWPLCLogicIO::JWPLCLogicIO()
    : _inputs{false, false, false, false, false, false, false, false},
      _outputs{false, false, false, false, false, false, false, false},
      _initialized(false)
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
    _inputs[index] = false;
  }

  for (uint8_t index = 0; index < Q0_COUNT; ++index)
  {
    pinMode(Q0_X[index], OUTPUT);
    _outputs[index] = false;
    digitalWrite(Q0_X[index], LOW);
  }

  _initialized = true;
  return true;
#endif
}

void JWPLCLogicIO::scanInputs()
{
  if (!_initialized)
  {
    return;
  }

  for (uint8_t index = 0; index < I0_COUNT; ++index)
  {
    _inputs[index] = digitalRead(I0_X[index]) == HIGH;
  }
}

void JWPLCLogicIO::beginOutputScan()
{
  for (uint8_t index = 0; index < Q0_COUNT; ++index)
  {
    _outputs[index] = false;
  }
}

bool JWPLCLogicIO::digitalInput(uint8_t index) const
{
  if (!_initialized || index >= I0_COUNT)
  {
    return false;
  }

  return _inputs[index];
}

bool JWPLCLogicIO::setDigitalOutput(uint8_t index, bool value)
{
  if (!_initialized || index >= Q0_COUNT)
  {
    return false;
  }

  _outputs[index] = value;
  return true;
}

void JWPLCLogicIO::commitOutputs()
{
  if (!_initialized)
  {
    return;
  }

  for (uint8_t index = 0; index < Q0_COUNT; ++index)
  {
    digitalWrite(Q0_X[index], _outputs[index] ? HIGH : LOW);
  }
}

void JWPLCLogicIO::allOutputsOff()
{
  beginOutputScan();
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

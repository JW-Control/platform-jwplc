#ifndef JWPLC_LOGIC_IO_H
#define JWPLC_LOGIC_IO_H

#include <Arduino.h>

class JWPLCLogicIO
{
public:
  JWPLCLogicIO();

  bool begin();
  void scanInputs();
  void beginOutputScan();
  bool digitalInput(uint8_t index) const;
  bool setDigitalOutput(uint8_t index, bool value);
  void commitOutputs();
  void allOutputsOff();

  uint8_t digitalInputCount() const;
  uint8_t digitalOutputCount() const;

private:
  uint8_t _inputBitmap;
  uint8_t _outputBitmap;
  bool _initialized;
};

#endif

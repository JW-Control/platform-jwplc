#ifndef JWPLC_LOGIC_RUNTIME_H
#define JWPLC_LOGIC_RUNTIME_H

#include <Arduino.h>

#include "runtime/LogicEngine.h"
#include "runtime/LogicProgram.h"
#include "storage/LogicStorageProfile.h"

#ifndef JWPLC_FRAM_SIZE_BYTES
#define JWPLC_FRAM_SIZE_BYTES 0
#endif

enum class JWPLCLogicRuntimeState : uint8_t
{
  Stopped = 0,
  Ready,
  Running,
  Fault
};

enum class JWPLCLogicRuntimeError : uint8_t
{
  None = 0,
  UnsupportedStorage,
  IOInitializationFailed,
  NotReady,
  ProgramNotLoaded,
  InvalidProgram,
  ProgramExecutionFailed
};

/**
 * @brief Motor lógico por bloques para JWPLC Basic.
 *
 * El programa se valida y ejecuta en un orden determinista. El PoC actual usa
 * una definición fija en RAM/Flash; la persistencia en FRAM se añadirá después
 * de validar el motor y las E/S físicas.
 */
class JWPLC_LogicRuntime
{
public:
  JWPLC_LogicRuntime();

  bool begin(uint32_t framBytes = JWPLC_FRAM_SIZE_BYTES);
  bool loadProgram(const LogicProgram &program);
  bool start();
  void stop();
  bool tick();

  JWPLCLogicRuntimeState state() const;
  JWPLCLogicRuntimeError lastError() const;
  LogicValidationError validationError() const;
  const LogicStorageProfile &storageProfile() const;

  bool blockValue(uint16_t index) const;
  uint32_t scanCount() const;
  uint32_t lastScanMicros() const;

  static const char *stateName(JWPLCLogicRuntimeState state);
  static const char *errorName(JWPLCLogicRuntimeError error);

private:
  const LogicStorageProfile *_storageProfile;
  JWPLCLogicRuntimeState _state;
  JWPLCLogicRuntimeError _lastError;
  uint32_t _scanCount;
  uint32_t _lastScanMicros;
  JWPLCLogicIO _io;
  LogicEngine _engine;
};

#endif

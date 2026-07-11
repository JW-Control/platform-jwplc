#ifndef JWPLC_LOGIC_RUNTIME_H
#define JWPLC_LOGIC_RUNTIME_H

#include <Arduino.h>

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
  NotReady,
  ProgramNotLoaded,
  InvalidProgram
};

/**
 * @brief Motor lógico por bloques para JWPLC Basic.
 *
 * PoC 0: expone el ciclo de vida del runtime y selecciona el perfil de
 * almacenamiento. La ejecución de bloques se agregará en la siguiente etapa.
 */
class JWPLC_LogicRuntime
{
public:
  JWPLC_LogicRuntime();

  bool begin(uint32_t framBytes = JWPLC_FRAM_SIZE_BYTES);
  bool start();
  void stop();
  bool tick();

  JWPLCLogicRuntimeState state() const;
  JWPLCLogicRuntimeError lastError() const;
  const LogicStorageProfile &storageProfile() const;

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
};

#endif

#ifndef JWPLC_LOGIC_RUNTIME_H
#define JWPLC_LOGIC_RUNTIME_H

#include <Arduino.h>

#include "runtime/LogicEngine.h"
#include "runtime/LogicProgram.h"
#include "storage/JWPLCLogicStorage.h"
#include "storage/LogicFRAMStorage.h"
#include "storage/LogicMemoryStorage.h"
#include "storage/LogicProgramCodec.h"
#include "storage/LogicProgramStore.h"
#include "storage/LogicStorageLayout.h"
#include "storage/LogicStorageProfile.h"
#include "storage/LogicSuperblockInspector.h"

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
  StoredProgramLoadFailed,
  ProgramExecutionFailed
};

/**
 * @brief Motor lógico por bloques para JWPLC Basic.
 *
 * El programa se valida y ejecuta desde RAM en un orden determinista. La
 * fachada storage() permite conectar explícitamente la FRAM y gestionar el
 * programa persistente sin apropiarse de la memoria durante sketches normales.
 */
class JWPLC_LogicRuntime
{
public:
  JWPLC_LogicRuntime();

  bool begin(uint32_t framBytes = JWPLC_FRAM_SIZE_BYTES);
  bool loadProgram(const LogicProgram &program);

  /**
   * @brief Evalúa el almacenamiento y prepara el programa arrancable en RAM.
   *
   * Devuelve la clasificación detallada de storage().prepareBoot(). Un fallback
   * válido se carga, pero nunca se inicia automáticamente.
   */
  JWPLCLogicStorageBootState prepareStoredProgram();

  /**
   * @brief Compatibilidad booleana para preparar el programa persistente.
   *
   * Devuelve true tanto para el programa activo como para un fallback válido.
   * start() continúa siendo un paso separado y explícito.
   */
  bool loadStoredProgram();

  bool start();
  void stop();
  bool tick();

  JWPLCLogicStorage &storage();
  const JWPLCLogicStorage &storage() const;

  JWPLCLogicRuntimeState state() const;
  JWPLCLogicRuntimeError lastError() const;
  LogicValidationError validationError() const;
  const LogicStorageProfile &storageProfile() const;
  const LogicStorageLayout &storageLayout() const;

  bool hasProgram() const;
  bool blockValue(uint16_t index) const;
  uint32_t scanCount() const;
  uint32_t lastScanMicros() const;
  uint32_t minScanMicros() const;
  uint32_t maxScanMicros() const;
  uint32_t averageScanMicros() const;
  uint32_t outputWriteCount() const;
  void resetScanStatistics();

  static const char *stateName(JWPLCLogicRuntimeState state);
  static const char *errorName(JWPLCLogicRuntimeError error);

private:
  void recordScanDuration(uint32_t elapsedMicros);

  const LogicStorageProfile *_storageProfile;
  JWPLCLogicRuntimeState _state;
  JWPLCLogicRuntimeError _lastError;
  uint32_t _scanCount;
  uint32_t _lastScanMicros;
  uint32_t _minScanMicros;
  uint32_t _maxScanMicros;
  uint64_t _totalScanMicros;
  JWPLCLogicStorage _storage;
  JWPLCLogicIO _io;
  LogicEngine _engine;
};

#endif
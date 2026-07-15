#ifndef JWPLC_LOGIC_RUNTIME_H
#define JWPLC_LOGIC_RUNTIME_H

#include <Arduino.h>

#include <cstring>

#include "runtime/LogicEngine.h"
#include "runtime/LogicProgram.h"
#include "storage/JWPLCLogicStorage.h"
#include "storage/LogicFRAMStorage.h"
#include "storage/LogicMemoryStorage.h"
#include "storage/LogicProgramCodec.h"
#include "storage/LogicProgramStore.h"
#include "storage/LogicRetentiveStore.h"
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
 * @brief Resultado de guardar o restaurar estado retentivo persistente.
 */
enum class JWPLCLogicRetentiveState : uint8_t
{
  NotEvaluated = 0,
  NotReady,
  NoStoredProgram,
  NoRetentiveBlocks,
  NoSnapshot,
  Restored,
  Saved,
  NoMatchingSnapshot,
  StoreError
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

  /**
   * @brief Vista de solo lectura del programa cargado en el motor.
   *
   * El puntero pertenece al runtime y permanece válido hasta loadProgram(),
   * prepareStoredProgram(), begin() o una descarga posterior. No debe guardarse
   * para modificar bloques ni utilizarse como almacenamiento propio.
   */
  const LogicProgram *program() const
  {
    return _engine.program();
  }

  /** @brief Cantidad de bloques del programa actualmente cargado. */
  uint16_t blockCount() const
  {
    const LogicProgram *loadedProgram = _engine.program();
    return loadedProgram ? loadedProgram->blockCount : 0;
  }

  /**
   * @brief Definición de bloque en modo lectura o nullptr si no existe.
   */
  const LogicBlockDefinition *blockDefinition(uint16_t index) const
  {
    const LogicProgram *loadedProgram = _engine.program();
    if (loadedProgram == nullptr ||
        loadedProgram->blocks == nullptr ||
        index >= loadedProgram->blockCount)
    {
      return nullptr;
    }

    return &loadedProgram->blocks[index];
  }

  size_t retentiveStateBytes() const;
  uint16_t retentiveBlockCount() const;
  bool exportRetentiveState(uint8_t *destination,
                            size_t destinationCapacity) const;
  bool importRetentiveState(const uint8_t *source,
                            size_t sourceLength);
  void clearRetentiveStates();

  /**
   * @brief Restaura el snapshot que coincide con la imagen persistente cargada.
   *
   * Debe llamarse después de prepareStoredProgram() y antes de start(). La
   * ausencia de snapshot o una identidad distinta no impiden ejecutar el
   * programa: los estados retentivos permanecen en falso.
   */
  JWPLCLogicRetentiveState restoreStoredRetentiveState();

  /**
   * @brief Guarda el bitmap actual para la imagen persistente cargada.
   *
   * La aplicación debe invocarlo explícitamente antes de stop() o antes de una
   * parada que borre los estados temporales.
   */
  bool saveStoredRetentiveState();

  JWPLCLogicRetentiveState retentiveState() const;
  LogicRetentiveStoreError retentiveStoreError() const;
  const LogicRetentiveStoreStatus &retentiveStoreStatus() const;
  static const char *retentiveStateName(JWPLCLogicRetentiveState state);

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

  bool storedProgramMatchesEngine() const
  {
    if (!_storage.hasLoadedProgram() || !_engine.hasProgram())
    {
      return false;
    }

    const LogicProgram storedProgram = _storage.activeProgram();
    const LogicProgram *engineProgram = _engine.program();
    if (engineProgram == nullptr ||
        storedProgram.name == nullptr ||
        storedProgram.blocks == nullptr ||
        engineProgram->name == nullptr ||
        engineProgram->blocks == nullptr ||
        storedProgram.blockCount != engineProgram->blockCount)
    {
      return false;
    }

    return std::strcmp(storedProgram.name, engineProgram->name) == 0 &&
           std::memcmp(storedProgram.blocks,
                       engineProgram->blocks,
                       static_cast<size_t>(storedProgram.blockCount) *
                           sizeof(LogicBlockDefinition)) == 0;
  }

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
  LogicRetentiveStore _retentiveStore;
  JWPLCLogicRetentiveState _retentiveState =
      JWPLCLogicRetentiveState::NotEvaluated;
};

inline JWPLCLogicRetentiveState
JWPLC_LogicRuntime::restoreStoredRetentiveState()
{
  if (_state == JWPLCLogicRuntimeState::Running ||
      !_storage.isReady() ||
      !_storage.connectRetentiveStore(_retentiveStore))
  {
    _retentiveState = JWPLCLogicRetentiveState::NotReady;
    return _retentiveState;
  }

  if (!storedProgramMatchesEngine())
  {
    _retentiveState = JWPLCLogicRetentiveState::NoStoredProgram;
    return _retentiveState;
  }

  if (_engine.retentiveBlockCount() == 0)
  {
    _retentiveState = JWPLCLogicRetentiveState::NoRetentiveBlocks;
    return _retentiveState;
  }

  uint32_t programId = 0;
  uint32_t generation = 0;
  uint16_t blockCount = 0;
  if (!_storage.loadedProgramIdentity(programId, generation, blockCount))
  {
    _retentiveState = JWPLCLogicRetentiveState::NoStoredProgram;
    return _retentiveState;
  }

  _engine.clearRetentiveStates();

  if (!_retentiveStore.hasSnapshot())
  {
    _retentiveState = JWPLCLogicRetentiveState::NoSnapshot;
    return _retentiveState;
  }

  uint8_t bitmap[LogicRetentiveStore::MAX_BITMAP_BYTES] = {};
  size_t loadedBytes = 0;
  if (!_retentiveStore.load(programId,
                            generation,
                            blockCount,
                            bitmap,
                            sizeof(bitmap),
                            loadedBytes))
  {
    _retentiveState =
        _retentiveStore.lastError() ==
                LogicRetentiveStoreError::NoMatchingSnapshot
            ? JWPLCLogicRetentiveState::NoMatchingSnapshot
            : JWPLCLogicRetentiveState::StoreError;
    return _retentiveState;
  }

  if (loadedBytes != _engine.retentiveStateBytes() ||
      !_engine.importRetentiveState(bitmap, loadedBytes))
  {
    _engine.clearRetentiveStates();
    _retentiveState = JWPLCLogicRetentiveState::StoreError;
    return _retentiveState;
  }

  _retentiveState = JWPLCLogicRetentiveState::Restored;
  return _retentiveState;
}

inline bool JWPLC_LogicRuntime::saveStoredRetentiveState()
{
  if (!_storage.isReady() ||
      !_storage.connectRetentiveStore(_retentiveStore))
  {
    _retentiveState = JWPLCLogicRetentiveState::NotReady;
    return false;
  }

  if (!storedProgramMatchesEngine())
  {
    _retentiveState = JWPLCLogicRetentiveState::NoStoredProgram;
    return false;
  }

  if (_engine.retentiveBlockCount() == 0)
  {
    _retentiveState = JWPLCLogicRetentiveState::NoRetentiveBlocks;
    return false;
  }

  uint32_t programId = 0;
  uint32_t generation = 0;
  uint16_t blockCount = 0;
  if (!_storage.loadedProgramIdentity(programId, generation, blockCount))
  {
    _retentiveState = JWPLCLogicRetentiveState::NoStoredProgram;
    return false;
  }

  uint8_t bitmap[LogicRetentiveStore::MAX_BITMAP_BYTES] = {};
  const size_t bitmapBytes = _engine.retentiveStateBytes();
  if (bitmapBytes == 0 ||
      bitmapBytes > sizeof(bitmap) ||
      !_engine.exportRetentiveState(bitmap, sizeof(bitmap)) ||
      !_retentiveStore.save(programId,
                            generation,
                            blockCount,
                            bitmap,
                            bitmapBytes))
  {
    _retentiveState = JWPLCLogicRetentiveState::StoreError;
    return false;
  }

  _retentiveState = JWPLCLogicRetentiveState::Saved;
  return true;
}

inline JWPLCLogicRetentiveState
JWPLC_LogicRuntime::retentiveState() const
{
  return _retentiveState;
}

inline LogicRetentiveStoreError
JWPLC_LogicRuntime::retentiveStoreError() const
{
  return _retentiveStore.lastError();
}

inline const LogicRetentiveStoreStatus &
JWPLC_LogicRuntime::retentiveStoreStatus() const
{
  return _retentiveStore.status();
}

inline const char *JWPLC_LogicRuntime::retentiveStateName(
    JWPLCLogicRetentiveState state)
{
  switch (state)
  {
  case JWPLCLogicRetentiveState::NotEvaluated:
    return "NOT_EVALUATED";
  case JWPLCLogicRetentiveState::NotReady:
    return "NOT_READY";
  case JWPLCLogicRetentiveState::NoStoredProgram:
    return "NO_STORED_PROGRAM";
  case JWPLCLogicRetentiveState::NoRetentiveBlocks:
    return "NO_RETENTIVE_BLOCKS";
  case JWPLCLogicRetentiveState::NoSnapshot:
    return "NO_SNAPSHOT";
  case JWPLCLogicRetentiveState::Restored:
    return "RESTORED";
  case JWPLCLogicRetentiveState::Saved:
    return "SAVED";
  case JWPLCLogicRetentiveState::NoMatchingSnapshot:
    return "NO_MATCHING_SNAPSHOT";
  case JWPLCLogicRetentiveState::StoreError:
    return "STORE_ERROR";
  default:
    return "UNKNOWN";
  }
}

#endif
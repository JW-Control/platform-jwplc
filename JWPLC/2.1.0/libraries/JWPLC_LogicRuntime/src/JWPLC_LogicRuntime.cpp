#include "JWPLC_LogicRuntime.h"

JWPLC_LogicRuntime::JWPLC_LogicRuntime()
    : _storageProfile(&JWPLCLogicStorageProfiles::NONE),
      _state(JWPLCLogicRuntimeState::Stopped),
      _lastError(JWPLCLogicRuntimeError::None),
      _scanCount(0),
      _lastScanMicros(0),
      _minScanMicros(0),
      _maxScanMicros(0),
      _totalScanMicros(0),
      _storage(),
      _io(),
      _engine()
{
}

bool JWPLC_LogicRuntime::begin(uint32_t framBytes)
{
  _storageProfile = &JWPLCLogicStorageProfiles::forCapacity(framBytes);
  resetScanStatistics();

  if (_storageProfile->maxBlocks == 0)
  {
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::UnsupportedStorage;
    return false;
  }

  if (!_io.begin())
  {
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::IOInitializationFailed;
    return false;
  }

  _engine.attachIO(_io);
  _engine.unloadProgram();
  _state = JWPLCLogicRuntimeState::Ready;
  _lastError = JWPLCLogicRuntimeError::None;
  return true;
}

bool JWPLC_LogicRuntime::loadProgram(const LogicProgram &program)
{
  if (_state == JWPLCLogicRuntimeState::Running)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return false;
  }

  if (!_engine.loadProgram(program, _storageProfile->maxBlocks))
  {
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::InvalidProgram;
    return false;
  }

  _state = JWPLCLogicRuntimeState::Ready;
  _lastError = JWPLCLogicRuntimeError::None;
  return true;
}

JWPLCLogicStorageBootState JWPLC_LogicRuntime::prepareStoredProgram()
{
  if (!_storage.isReady())
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return JWPLCLogicStorageBootState::NotReady;
  }

  if (_state == JWPLCLogicRuntimeState::Running)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return JWPLCLogicStorageBootState::NotReady;
  }

  const JWPLCLogicStorageBootState bootState = _storage.prepareBoot();
  const bool bootable =
      bootState == JWPLCLogicStorageBootState::ActiveProgramLoaded ||
      bootState == JWPLCLogicStorageBootState::FallbackProgramLoaded;

  if (!bootable)
  {
    // La clasificación del almacenamiento puede consultarse antes de begin().
    // Si el motor ya estaba inicializado, se descarga cualquier programa previo.
    _engine.unloadProgram();

    if (_storageProfile->maxBlocks > 0)
    {
      _state = JWPLCLogicRuntimeState::Ready;
    }

    _lastError = JWPLCLogicRuntimeError::StoredProgramLoadFailed;
    return bootState;
  }

  const bool validRuntimeState =
      _state == JWPLCLogicRuntimeState::Ready ||
      _state == JWPLCLogicRuntimeState::Stopped;

  if (!validRuntimeState || _storageProfile->maxBlocks == 0)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return JWPLCLogicStorageBootState::NotReady;
  }

  // Una nueva preparación nunca debe dejar disponible un programa anterior.
  _engine.unloadProgram();

  // LogicEngine realiza una copia profunda. El programa preparado queda
  // independiente del buffer interno que storage() pueda reutilizar después.
  if (!loadProgram(_storage.activeProgram()))
  {
    _engine.unloadProgram();
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::InvalidProgram;
    return JWPLCLogicStorageBootState::InvalidProgram;
  }

  return bootState;
}

bool JWPLC_LogicRuntime::loadStoredProgram()
{
  const JWPLCLogicStorageBootState bootState = prepareStoredProgram();
  return bootState == JWPLCLogicStorageBootState::ActiveProgramLoaded ||
         bootState == JWPLCLogicStorageBootState::FallbackProgramLoaded;
}

bool JWPLC_LogicRuntime::start()
{
  const bool validState =
      _state == JWPLCLogicRuntimeState::Ready ||
      _state == JWPLCLogicRuntimeState::Stopped;

  if (!validState)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return false;
  }

  if (!_engine.hasProgram())
  {
    _lastError = JWPLCLogicRuntimeError::ProgramNotLoaded;
    return false;
  }

  resetScanStatistics();
  _state = JWPLCLogicRuntimeState::Running;
  _lastError = JWPLCLogicRuntimeError::None;
  return true;
}

void JWPLC_LogicRuntime::stop()
{
  _io.allOutputsOff();
  _engine.resetStates();

  if (_state != JWPLCLogicRuntimeState::Fault)
  {
    _state = JWPLCLogicRuntimeState::Stopped;
    _lastError = JWPLCLogicRuntimeError::None;
  }
}

bool JWPLC_LogicRuntime::tick()
{
  if (_state != JWPLCLogicRuntimeState::Running)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return false;
  }

  const uint32_t startedAt = micros();

  if (!_engine.scan(millis()))
  {
    _io.allOutputsOff();
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::ProgramExecutionFailed;
    return false;
  }

  recordScanDuration(micros() - startedAt);
  return true;
}

void JWPLC_LogicRuntime::recordScanDuration(uint32_t elapsedMicros)
{
  _lastScanMicros = elapsedMicros;

  if (_scanCount == 0 || elapsedMicros < _minScanMicros)
  {
    _minScanMicros = elapsedMicros;
  }

  if (elapsedMicros > _maxScanMicros)
  {
    _maxScanMicros = elapsedMicros;
  }

  _totalScanMicros += elapsedMicros;
  ++_scanCount;
}

void JWPLC_LogicRuntime::resetScanStatistics()
{
  _scanCount = 0;
  _lastScanMicros = 0;
  _minScanMicros = 0;
  _maxScanMicros = 0;
  _totalScanMicros = 0;
}

JWPLCLogicStorage &JWPLC_LogicRuntime::storage()
{
  return _storage;
}

const JWPLCLogicStorage &JWPLC_LogicRuntime::storage() const
{
  return _storage;
}

JWPLCLogicRuntimeState JWPLC_LogicRuntime::state() const
{
  return _state;
}

JWPLCLogicRuntimeError JWPLC_LogicRuntime::lastError() const
{
  return _lastError;
}

LogicValidationError JWPLC_LogicRuntime::validationError() const
{
  return _engine.validationError();
}

const LogicStorageProfile &JWPLC_LogicRuntime::storageProfile() const
{
  return *_storageProfile;
}

const LogicStorageLayout &JWPLC_LogicRuntime::storageLayout() const
{
  return JWPLCLogicStorageLayouts::forCapacity(_storageProfile->framBytes);
}

bool JWPLC_LogicRuntime::hasProgram() const
{
  return _engine.hasProgram();
}

bool JWPLC_LogicRuntime::blockValue(uint16_t index) const
{
  return _engine.blockValue(index);
}

size_t JWPLC_LogicRuntime::retentiveStateBytes() const
{
  return _engine.retentiveStateBytes();
}

uint16_t JWPLC_LogicRuntime::retentiveBlockCount() const
{
  return _engine.retentiveBlockCount();
}

bool JWPLC_LogicRuntime::exportRetentiveState(
    uint8_t *destination,
    size_t destinationCapacity) const
{
  return _engine.exportRetentiveState(destination, destinationCapacity);
}

bool JWPLC_LogicRuntime::importRetentiveState(const uint8_t *source,
                                              size_t sourceLength)
{
  return _engine.importRetentiveState(source, sourceLength);
}

void JWPLC_LogicRuntime::clearRetentiveStates()
{
  _engine.clearRetentiveStates();
}

uint32_t JWPLC_LogicRuntime::scanCount() const
{
  return _scanCount;
}

uint32_t JWPLC_LogicRuntime::lastScanMicros() const
{
  return _lastScanMicros;
}

uint32_t JWPLC_LogicRuntime::minScanMicros() const
{
  return _scanCount == 0 ? 0 : _minScanMicros;
}

uint32_t JWPLC_LogicRuntime::maxScanMicros() const
{
  return _maxScanMicros;
}

uint32_t JWPLC_LogicRuntime::averageScanMicros() const
{
  if (_scanCount == 0)
  {
    return 0;
  }

  return static_cast<uint32_t>(_totalScanMicros / _scanCount);
}

uint32_t JWPLC_LogicRuntime::outputWriteCount() const
{
  return _io.outputWriteCount();
}

const char *JWPLC_LogicRuntime::stateName(JWPLCLogicRuntimeState state)
{
  switch (state)
  {
  case JWPLCLogicRuntimeState::Stopped:
    return "STOPPED";
  case JWPLCLogicRuntimeState::Ready:
    return "READY";
  case JWPLCLogicRuntimeState::Running:
    return "RUNNING";
  case JWPLCLogicRuntimeState::Fault:
    return "FAULT";
  default:
    return "UNKNOWN";
  }
}

const char *JWPLC_LogicRuntime::errorName(JWPLCLogicRuntimeError error)
{
  switch (error)
  {
  case JWPLCLogicRuntimeError::None:
    return "NONE";
  case JWPLCLogicRuntimeError::UnsupportedStorage:
    return "UNSUPPORTED_STORAGE";
  case JWPLCLogicRuntimeError::IOInitializationFailed:
    return "IO_INITIALIZATION_FAILED";
  case JWPLCLogicRuntimeError::NotReady:
    return "NOT_READY";
  case JWPLCLogicRuntimeError::ProgramNotLoaded:
    return "PROGRAM_NOT_LOADED";
  case JWPLCLogicRuntimeError::InvalidProgram:
    return "INVALID_PROGRAM";
  case JWPLCLogicRuntimeError::StoredProgramLoadFailed:
    return "STORED_PROGRAM_LOAD_FAILED";
  case JWPLCLogicRuntimeError::ProgramExecutionFailed:
    return "PROGRAM_EXECUTION_FAILED";
  default:
    return "UNKNOWN";
  }
}
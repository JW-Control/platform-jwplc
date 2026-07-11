#include "JWPLC_LogicRuntime.h"

JWPLC_LogicRuntime::JWPLC_LogicRuntime()
    : _storageProfile(&JWPLCLogicStorageProfiles::NONE),
      _state(JWPLCLogicRuntimeState::Stopped),
      _lastError(JWPLCLogicRuntimeError::None),
      _scanCount(0),
      _lastScanMicros(0)
{
}

bool JWPLC_LogicRuntime::begin(uint32_t framBytes)
{
  _storageProfile = &JWPLCLogicStorageProfiles::forCapacity(framBytes);
  _scanCount = 0;
  _lastScanMicros = 0;

  if (_storageProfile->maxBlocks == 0)
  {
    _state = JWPLCLogicRuntimeState::Fault;
    _lastError = JWPLCLogicRuntimeError::UnsupportedStorage;
    return false;
  }

  _state = JWPLCLogicRuntimeState::Ready;
  _lastError = JWPLCLogicRuntimeError::None;
  return true;
}

bool JWPLC_LogicRuntime::start()
{
  if (_state != JWPLCLogicRuntimeState::Ready &&
      _state != JWPLCLogicRuntimeState::Stopped)
  {
    _lastError = JWPLCLogicRuntimeError::NotReady;
    return false;
  }

  _state = JWPLCLogicRuntimeState::Running;
  _lastError = JWPLCLogicRuntimeError::None;
  return true;
}

void JWPLC_LogicRuntime::stop()
{
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

  // PoC 0: el motor de bloques se incorporará en la siguiente etapa.

  _lastScanMicros = micros() - startedAt;
  ++_scanCount;
  return true;
}

JWPLCLogicRuntimeState JWPLC_LogicRuntime::state() const
{
  return _state;
}

JWPLCLogicRuntimeError JWPLC_LogicRuntime::lastError() const
{
  return _lastError;
}

const LogicStorageProfile &JWPLC_LogicRuntime::storageProfile() const
{
  return *_storageProfile;
}

uint32_t JWPLC_LogicRuntime::scanCount() const
{
  return _scanCount;
}

uint32_t JWPLC_LogicRuntime::lastScanMicros() const
{
  return _lastScanMicros;
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
  case JWPLCLogicRuntimeError::NotReady:
    return "NOT_READY";
  case JWPLCLogicRuntimeError::ProgramNotLoaded:
    return "PROGRAM_NOT_LOADED";
  case JWPLCLogicRuntimeError::InvalidProgram:
    return "INVALID_PROGRAM";
  default:
    return "UNKNOWN";
  }
}

#include "JWPLCLogicStorage.h"

#include <cstring>

JWPLCLogicStorage::JWPLCLogicStorage()
    : _profile(&JWPLCLogicStorageProfiles::NONE),
      _layout(&JWPLCLogicStorageLayouts::NONE),
      _backend(),
      _store(),
      _loadedProgram{},
      _scratch{},
      _ready(false),
      _hasLoadedProgram(false),
      _lastError(JWPLCLogicStorageError::None),
      _validationError(LogicValidationError::None)
{
}

void JWPLCLogicStorage::clearLoadedProgram()
{
  memset(&_loadedProgram, 0, sizeof(_loadedProgram));
  _hasLoadedProgram = false;
}

bool JWPLCLogicStorage::begin(JW_FRAM &fram)
{
  _ready = false;
  clearLoadedProgram();
  _validationError = LogicValidationError::None;

  const uint32_t physicalBytes = static_cast<uint32_t>(fram.size());
  _profile = &JWPLCLogicStorageProfiles::forCapacity(physicalBytes);
  _layout = &JWPLCLogicStorageLayouts::forCapacity(physicalBytes);

  if (_profile->maxBlocks == 0 || !_layout->isValid())
  {
    _lastError = JWPLCLogicStorageError::UnsupportedStorage;
    return false;
  }

  if (!_backend.begin(fram, 0, _profile->framBytes))
  {
    _lastError = JWPLCLogicStorageError::BackendInitializationFailed;
    return false;
  }

  if (!_store.begin(_backend, *_profile))
  {
    _lastError = JWPLCLogicStorageError::StoreInitializationFailed;
    return false;
  }

  _ready = true;
  _lastError = JWPLCLogicStorageError::None;
  return true;
}

bool JWPLCLogicStorage::isReady() const
{
  return _ready;
}

bool JWPLCLogicStorage::isFormatted() const
{
  return _ready && _store.isFormatted();
}

bool JWPLCLogicStorage::format()
{
  if (!_ready)
  {
    _lastError = JWPLCLogicStorageError::NotReady;
    return false;
  }

  if (!_store.format())
  {
    _lastError = JWPLCLogicStorageError::StoreInitializationFailed;
    return false;
  }

  clearLoadedProgram();
  _lastError = JWPLCLogicStorageError::None;
  return true;
}

bool JWPLCLogicStorage::save(const LogicProgram &program,
                             uint32_t programId,
                             uint32_t flags)
{
  if (!_ready)
  {
    _lastError = JWPLCLogicStorageError::NotReady;
    return false;
  }

  if (!_store.isFormatted())
  {
    _lastError = JWPLCLogicStorageError::Unformatted;
    return false;
  }

  _validationError = LogicValidator::validate(program,
                                               _profile->maxBlocks,
                                               8,
                                               8);
  if (_validationError != LogicValidationError::None)
  {
    _lastError = JWPLCLogicStorageError::InvalidProgram;
    return false;
  }

  // La secuencia del superblock nunca retrocede, incluso después de rollback.
  // Usarla como base evita reutilizar una generación antigua al guardar luego
  // de reactivar un programa previo.
  uint32_t nextGeneration = _store.status().sequence;
  if (nextGeneration == 0)
  {
    nextGeneration = 1;
  }

  if (!_store.saveProgram(program,
                          programId,
                          nextGeneration,
                          flags,
                          _scratch,
                          sizeof(_scratch)))
  {
    _lastError = JWPLCLogicStorageError::SaveFailed;
    return false;
  }

  clearLoadedProgram();
  _lastError = JWPLCLogicStorageError::None;
  return true;
}

bool JWPLCLogicStorage::loadActive()
{
  if (!_ready)
  {
    _lastError = JWPLCLogicStorageError::NotReady;
    return false;
  }

  if (!_store.isFormatted())
  {
    _lastError = JWPLCLogicStorageError::Unformatted;
    return false;
  }

  clearLoadedProgram();

  if (!_store.loadActive(_loadedProgram,
                         _scratch,
                         sizeof(_scratch)))
  {
    _lastError = JWPLCLogicStorageError::LoadFailed;
    return false;
  }

  _validationError = LogicValidator::validate(_loadedProgram.asProgram(),
                                               _profile->maxBlocks,
                                               8,
                                               8);
  if (_validationError != LogicValidationError::None)
  {
    clearLoadedProgram();
    _lastError = JWPLCLogicStorageError::InvalidProgram;
    return false;
  }

  _hasLoadedProgram = true;
  _lastError = JWPLCLogicStorageError::None;
  return true;
}

bool JWPLCLogicStorage::rollback()
{
  if (!_ready)
  {
    _lastError = JWPLCLogicStorageError::NotReady;
    return false;
  }

  if (!_store.isFormatted())
  {
    _lastError = JWPLCLogicStorageError::Unformatted;
    return false;
  }

  const uint8_t activeSlot = _store.status().activeSlot;
  if (activeSlot > 1)
  {
    _lastError = JWPLCLogicStorageError::RollbackUnavailable;
    return false;
  }

  const uint8_t targetSlot = activeSlot == 0 ? 1 : 0;
  clearLoadedProgram();

  // Primera fase: cargar y verificar el candidato sin cambiar el superblock.
  if (!_store.loadVerifiedSlot(targetSlot,
                               _loadedProgram,
                               _scratch,
                               sizeof(_scratch)))
  {
    clearLoadedProgram();
    _lastError = JWPLCLogicStorageError::RollbackUnavailable;
    return false;
  }

  _validationError = LogicValidator::validate(_loadedProgram.asProgram(),
                                               _profile->maxBlocks,
                                               8,
                                               8);
  if (_validationError != LogicValidationError::None)
  {
    clearLoadedProgram();
    _lastError = JWPLCLogicStorageError::InvalidProgram;
    return false;
  }

  // Segunda fase: activar únicamente el slot ya cargado y validado.
  if (!_store.activateVerifiedSlot(targetSlot))
  {
    clearLoadedProgram();
    _lastError = JWPLCLogicStorageError::RollbackFailed;
    return false;
  }

  _hasLoadedProgram = true;
  _lastError = JWPLCLogicStorageError::None;
  return true;
}

bool JWPLCLogicStorage::hasLoadedProgram() const
{
  return _hasLoadedProgram;
}

LogicProgram JWPLCLogicStorage::activeProgram() const
{
  if (!_hasLoadedProgram)
  {
    const LogicProgram empty = {nullptr, nullptr, 0};
    return empty;
  }

  return _loadedProgram.asProgram();
}

const LogicProgramStoreStatus &JWPLCLogicStorage::status() const
{
  return _store.status();
}

const LogicStorageProfile &JWPLCLogicStorage::profile() const
{
  return *_profile;
}

const LogicStorageLayout &JWPLCLogicStorage::layout() const
{
  return *_layout;
}

JWPLCLogicStorageError JWPLCLogicStorage::lastError() const
{
  return _lastError;
}

LogicProgramStoreError JWPLCLogicStorage::storeError() const
{
  return _store.lastError();
}

LogicValidationError JWPLCLogicStorage::validationError() const
{
  return _validationError;
}

const char *JWPLCLogicStorage::errorName(JWPLCLogicStorageError error)
{
  switch (error)
  {
  case JWPLCLogicStorageError::None:
    return "NONE";
  case JWPLCLogicStorageError::UnsupportedStorage:
    return "UNSUPPORTED_STORAGE";
  case JWPLCLogicStorageError::BackendInitializationFailed:
    return "BACKEND_INITIALIZATION_FAILED";
  case JWPLCLogicStorageError::StoreInitializationFailed:
    return "STORE_INITIALIZATION_FAILED";
  case JWPLCLogicStorageError::NotReady:
    return "NOT_READY";
  case JWPLCLogicStorageError::Unformatted:
    return "UNFORMATTED";
  case JWPLCLogicStorageError::InvalidProgram:
    return "INVALID_PROGRAM";
  case JWPLCLogicStorageError::SaveFailed:
    return "SAVE_FAILED";
  case JWPLCLogicStorageError::LoadFailed:
    return "LOAD_FAILED";
  case JWPLCLogicStorageError::NoLoadedProgram:
    return "NO_LOADED_PROGRAM";
  case JWPLCLogicStorageError::RollbackUnavailable:
    return "ROLLBACK_UNAVAILABLE";
  case JWPLCLogicStorageError::RollbackFailed:
    return "ROLLBACK_FAILED";
  default:
    return "UNKNOWN";
  }
}

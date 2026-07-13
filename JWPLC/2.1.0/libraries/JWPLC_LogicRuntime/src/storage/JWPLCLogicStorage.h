#ifndef JWPLC_LOGIC_STORAGE_H
#define JWPLC_LOGIC_STORAGE_H

#include <Arduino.h>
#include <JW_FRAM.h>

#include "LogicFRAMStorage.h"
#include "LogicProgramStore.h"
#include "LogicStorageLayout.h"
#include "../runtime/LogicValidator.h"

enum class JWPLCLogicStorageError : uint8_t
{
  None = 0,
  UnsupportedStorage,
  BackendInitializationFailed,
  StoreInitializationFailed,
  NotReady,
  Unformatted,
  InvalidProgram,
  SaveFailed,
  LoadFailed,
  NoLoadedProgram,
  RollbackUnavailable,
  RollbackFailed
};

/**
 * @brief Fachada pública del almacenamiento persistente del runtime.
 *
 * No formatea automáticamente la FRAM. begin() solo detecta el perfil,
 * conecta el backend y consulta si existe una firma válida del gestor A/B.
 */
class JWPLCLogicStorage
{
public:
  JWPLCLogicStorage();

  bool begin(JW_FRAM &fram);
  bool isReady() const;
  bool isFormatted() const;

  bool format();
  bool save(const LogicProgram &program,
            uint32_t programId,
            uint32_t flags = 0);
  bool loadActive();

  /**
   * @brief Activa el otro slot verificado sin reescribir su imagen.
   *
   * El candidato se carga, se verifica por CRC/codec y pasa por el validador
   * lógico antes de escribir el nuevo superblock activo.
   */
  bool rollback();

  bool hasLoadedProgram() const;
  LogicProgram activeProgram() const;

  const LogicProgramStoreStatus &status() const;
  const LogicStorageProfile &profile() const;
  const LogicStorageLayout &layout() const;

  JWPLCLogicStorageError lastError() const;
  LogicProgramStoreError storeError() const;
  LogicValidationError validationError() const;

  static const char *errorName(JWPLCLogicStorageError error);

private:
  static constexpr size_t SCRATCH_BYTES =
      JWPLC_LOGIC_IMAGE_HEADER_SIZE +
      (static_cast<size_t>(JWPLC_LOGIC_IMAGE_BLOCK_SIZE) *
       JWPLC_LOGIC_COMPILED_MAX_BLOCKS);

  void clearLoadedProgram();

  const LogicStorageProfile *_profile;
  const LogicStorageLayout *_layout;
  LogicFRAMStorage _backend;
  LogicProgramStore _store;
  LogicProgramBuffer _loadedProgram;
  uint8_t _scratch[SCRATCH_BYTES];
  bool _ready;
  bool _hasLoadedProgram;
  JWPLCLogicStorageError _lastError;
  LogicValidationError _validationError;
};

#endif

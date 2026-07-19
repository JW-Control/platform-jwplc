#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_ACTIVE_RENDERER_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_ACTIVE_RENDERER_H

#include "RuntimeUIFBDMapV14.h"

/**
 * @brief Renderer activo único para las transiciones validadas del mapa FBD.
 *
 * Evita que revisiones históricas dibujen frames intermedios. Intercepta antes
 * de V5/V7 la entrada al editor TON y el retorno DETALLE -> MAPA. También separa
 * rótulos estáticos de valores dinámicos en EDITAR T.
 */
class RuntimeUIFBDMapActiveRenderer : public RuntimeUIFBDMapV14
{
public:
  RuntimeUIFBDMapActiveRenderer();

  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);

protected:
  void drawExistingLogoScreen() override;
  void drawExistingLogoFields() override;
  void drawExistingElapsed(bool force) override;

private:
  static constexpr int16_t FIELD_Y = 103;
  static constexpr int16_t FIELD_H = 38;

  static int16_t fieldX(LogoField field);
  static int16_t fieldWidth(LogoField field);

  void invalidateActiveEditorCaches();
  void drawEditorHeaderTwoLines();
  void drawExistingFieldFull(LogoField field);
  void drawExistingFieldValueOnly(LogoField field);
  const char *existingFieldLabel(LogoField field) const;
  void formatExistingFieldValue(LogoField field,
                                char *destination,
                                size_t capacity) const;

  bool _activeFieldsCacheValid;
  uint32_t _activeMajorCache;
  uint32_t _activeMinorCache;
  LogoTimeBase _activeBaseCache;
  LogoField _activeFocusCache;

  bool _activeElapsedCacheValid;
  bool _activeElapsedColorOn;
  char _activeElapsedCache[20];
};

#endif
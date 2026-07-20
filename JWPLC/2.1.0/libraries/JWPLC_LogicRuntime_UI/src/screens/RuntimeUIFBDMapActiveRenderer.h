#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_ACTIVE_RENDERER_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_ACTIVE_RENDERER_H

#include "RuntimeUIFBDMapV14.h"

/**
 * @brief Renderer activo único para las transiciones validadas del mapa FBD.
 *
 * Las revisiones V5/V7/V13/V14 permanecen como capas internas de compatibilidad,
 * pero esta clase es la única propietaria de:
 * - cabecera visible de MAPA, DETALLE y EDITAR T;
 * - entrada/salida del editor TON existente;
 * - retorno DETALLE -> MAPA;
 * - actualización parcial de campos y Ta.
 *
 * Así se evita que una revisión histórica pinte un frame intermedio antes del
 * layout vigente.
 */
class RuntimeUIFBDMapActiveRenderer : public RuntimeUIFBDMapV14
{
public:
  RuntimeUIFBDMapActiveRenderer();

  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void forceRedraw();

protected:
  void drawMapHeaderInfo() override;
  void updateDetailHeader() override;
  void drawExistingLogoScreen() override;
  void drawExistingLogoFields() override;
  void drawExistingElapsed(bool force) override;
  void drawCompactDetailHeader(bool force) override;

private:
  enum class HeaderView : uint8_t
  {
    None = 0,
    Map,
    Detail,
    EditTon
  };

  static constexpr int16_t FIELD_Y = 103;
  static constexpr int16_t FIELD_H = 38;

  static constexpr int16_t HEADER_TITLE_X = 0;
  static constexpr int16_t HEADER_TITLE_W = 108;
  static constexpr int16_t HEADER_CONTEXT_X = 108;
  static constexpr int16_t HEADER_CONTEXT_W = 125;
  static constexpr int16_t HEADER_CONTEXT_LINE1_Y = 3;
  static constexpr int16_t HEADER_CONTEXT_LINE2_Y = 12;
  static constexpr uint8_t HEADER_CONTEXT_COLUMNS = 20;

  static constexpr uint32_t LOGO_MINIMUM_TIME_MS_ACTIVE = 20UL;

  static int16_t fieldX(LogoField field);
  static int16_t fieldWidth(LogoField field);

  void invalidateActiveEditorCaches();
  void invalidateUnifiedHeaderCache();

  void refreshActiveTonEditor(const JWPLC_IOState *io,
                              const JWPLC_RTCState *rtc);
  void returnEditorToDetailSinglePass(const JWPLC_IOState *io,
                                      const JWPLC_RTCState *rtc);

  void drawExistingFieldFull(LogoField field);
  void drawExistingFieldValueOnly(LogoField field);
  const char *existingFieldLabel(LogoField field) const;
  void formatExistingFieldValue(LogoField field,
                                char *destination,
                                size_t capacity) const;

  HeaderView currentHeaderView() const;
  const char *titleForHeaderView(HeaderView view) const;
  void buildUnifiedHeaderContext(HeaderView view,
                                 char *line1,
                                 size_t line1Capacity,
                                 char *line2,
                                 size_t line2Capacity) const;
  void drawUnifiedHeader(bool force);

  bool _activeFieldsCacheValid;
  uint32_t _activeMajorCache;
  uint32_t _activeMinorCache;
  LogoTimeBase _activeBaseCache;
  LogoField _activeFocusCache;

  bool _activeElapsedCacheValid;
  bool _activeElapsedColorOn;
  char _activeElapsedCache[20];
  bool _activeFooterDefaultVisible;

  bool _unifiedHeaderCacheValid;
  HeaderView _unifiedHeaderViewCache;
  LogicV2EngineState _unifiedHeaderStateCache;
  char _unifiedHeaderLine1Cache[24];
  char _unifiedHeaderLine2Cache[24];
};

#endif
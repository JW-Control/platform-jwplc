#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V13_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V13_H

#include "RuntimeUIFBDMapV12.h"

/**
 * @brief Revisión experimental v0.6.0 del editor TON con base tipo LOGO!.
 *
 * El tiempo se presenta como dos componentes y una base persistida dentro del
 * campo resource del TON, sin ampliar LogicV2BlockRecord:
 * - s: segundos : centésimas;
 * - m: minutos : segundos;
 * - h: horas : minutos.
 *
 * parameter continúa almacenando el tiempo efectivo en milisegundos para que el
 * motor lógico no cambie. Esta revisión sigue siendo RAM-only, sin FRAM ni
 * conmutación de salidas físicas.
 */
class RuntimeUIFBDMapV13 : public RuntimeUIFBDMapV12
{
public:
  RuntimeUIFBDMapV13();

  void attach(RuntimeUIV2ReadModel &model);
  void detach();
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

protected:
  void enterConfigureMain() override;
  void formatParameterValue(uint8_t parameterIndex,
                            char *destination,
                            size_t capacity) const override;
  void drawParameterEditScreen() override;
  bool handleParameterEditInput() override;
  void returnFromParameterEdit(bool accept) override;
  void requestWizardCreateV11() override;

private:
  enum class LogoTimeBase : uint8_t
  {
    Seconds = 0,
    Minutes = 1,
    Hours = 2
  };

  enum class LogoField : uint8_t
  {
    Major = 0,
    Minor,
    Base
  };

  static constexpr uint16_t TON_BASE_SECONDS = 0;
  static constexpr uint16_t TON_BASE_MINUTES = 1;
  static constexpr uint16_t TON_BASE_HOURS = 2;
  static constexpr uint16_t TON_BASE_RESERVED = 3;

  static constexpr uint32_t DETAIL_OVERLAY_REFRESH_MS = 100UL;
  static constexpr uint32_t EXISTING_ELAPSED_REFRESH_MS = 100UL;

  static LogoTimeBase baseFromResource(uint16_t resource);
  static uint16_t resourceFromBase(LogoTimeBase base);
  static const char *baseText(LogoTimeBase base);
  static const char *majorLabel(LogoTimeBase base);
  static const char *minorLabel(LogoTimeBase base);
  static uint32_t minorMaximum(LogoTimeBase base);
  static uint32_t maximumMilliseconds(LogoTimeBase base);
  static LogoTimeBase effectiveBase(uint32_t milliseconds,
                                    uint16_t resource);
  static uint32_t encodeMilliseconds(uint32_t major,
                                     uint32_t minor,
                                     LogoTimeBase base);
  static void decodeMilliseconds(uint32_t milliseconds,
                                 LogoTimeBase base,
                                 uint32_t &major,
                                 uint32_t &minor);
  static void formatLogoTime(uint32_t major,
                             uint32_t minor,
                             LogoTimeBase base,
                             char *destination,
                             size_t capacity);
  static void formatMilliseconds(uint32_t milliseconds,
                                 uint16_t resource,
                                 char *destination,
                                 size_t capacity);

  void resetV13State();
  void resetWizardLogoState();
  void beginWizardLogoEdit();
  void drawWizardLogoFields();
  bool handleLogoFields(uint32_t &major,
                        uint32_t &minor,
                        LogoTimeBase &base,
                        LogoField &focus,
                        bool wizardScreen);

  void beginExistingLogoEditor();
  void refreshExistingLogoEditor();
  void drawExistingLogoScreen();
  void drawExistingLogoFields();
  void drawExistingActual();
  void drawExistingElapsed(bool force);
  void drawExistingFooter(const char *text,
                          uint16_t color);

  void drawDetailLogoOverlay(bool force);

  bool _wizardLogoInitialized;
  bool _wizardLogoEditing;
  uint32_t _wizardMajor;
  uint32_t _wizardMinor;
  LogoTimeBase _wizardBase;
  LogoField _wizardFocus;
  uint32_t _wizardMajorBackup;
  uint32_t _wizardMinorBackup;
  LogoTimeBase _wizardBaseBackup;

  bool _existingLogoInitialized;
  uint32_t _existingMajor;
  uint32_t _existingMinor;
  LogoTimeBase _existingBase;
  LogoField _existingFocus;
  uint32_t _existingOriginalMs;
  uint16_t _existingOriginalResource;
  uint32_t _lastExistingElapsedRefreshMs;
  uint32_t _lastDetailOverlayRefreshMs;
};

#endif
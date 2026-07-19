#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V14_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V14_H

#include "RuntimeUIFBDMapV13.h"

/**
 * @brief Correcciones visuales sobre el TON tipo LOGO! v0.6.0.
 *
 * Mantiene intactos parameter/resource y el motor. Corrige la base de Ta,
 * separa el encabezado del identificador del bloque, conserva el marco amarillo,
 * limita la transición del nodo + al mapa y evita repintados redundantes de T/Ta.
 */
class RuntimeUIFBDMapV14 : public RuntimeUIFBDMapV13
{
public:
  RuntimeUIFBDMapV14();

  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);

protected:
  void drawExistingLogoScreen() override;
  void drawExistingElapsed(bool force) override;
  void drawDetailLogoOverlay(bool force) override;
  void drawTonDetailElapsedLive(bool force) override;

private:
  static void formatMillisecondsInBase(uint32_t milliseconds,
                                       LogoTimeBase base,
                                       char *destination,
                                       size_t capacity);
  static const char *engineStateText(LogicV2EngineState state);
  static uint16_t engineStateColor(LogicV2EngineState state);

  void invalidateDetailLogoCache();

  bool _detailLogoCacheValid;
  uint16_t _detailLogoCacheBlock;
  bool _detailLogoCacheParameterSelected;
  bool _detailLogoCacheColorOn;
  char _detailLogoCacheConfigured[16];
  char _detailLogoCacheElapsed[16];
};

#endif
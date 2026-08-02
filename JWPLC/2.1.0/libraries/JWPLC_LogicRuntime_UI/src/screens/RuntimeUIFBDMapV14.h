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

  /**
   * @brief Periodo recomendado del callback USER según la pantalla activa.
   *
   * Mapa y detalle estable usan 10 Hz. Asistentes y editores usan 25 Hz para
   * conservar respuesta de botonera y repeat sin adquirir el bus TFT a 20 Hz
   * permanentemente cuando no hay interacción.
   */
  uint32_t recommendedRefreshPeriodMs() const;

protected:
  void drawExistingLogoScreen() override;
  void drawExistingElapsed(bool force) override;
  void drawDetailLogoOverlay(bool force) override;
  void drawTonDetailElapsedLive(bool force) override;

  /**
   * @brief Hook del encabezado compacto histórico de DETALLE.
   *
   * El renderer activo lo anula porque la cabecera unificada es la única que debe
   * escribir las dos filas centrales. V14 conserva su implementación para pruebas
   * históricas aisladas.
   */
  virtual void drawCompactDetailHeader(bool force);

private:
  static void formatMillisecondsInBase(uint32_t milliseconds,
                                       LogoTimeBase base,
                                       char *destination,
                                       size_t capacity);
  static const char *engineStateText(LogicV2EngineState state);
  static uint16_t engineStateColor(LogicV2EngineState state);

  void invalidateDetailLogoCache();
  void invalidateExistingElapsedCache();
  void invalidateCompactDetailHeader();

  bool _detailLogoCacheValid;
  uint16_t _detailLogoCacheBlock;
  bool _detailLogoCacheParameterSelected;
  bool _detailLogoCacheColorOn;
  char _detailLogoCacheConfigured[16];
  char _detailLogoCacheElapsed[16];

  bool _existingElapsedCacheValid;
  bool _existingElapsedCacheColorOn;
  char _existingElapsedCacheText[36];

  bool _compactDetailHeaderValid;
  uint16_t _compactDetailHeaderBlock;
  uint8_t _compactDetailHeaderInput;
  bool _compactDetailHeaderParameter;
};

#endif
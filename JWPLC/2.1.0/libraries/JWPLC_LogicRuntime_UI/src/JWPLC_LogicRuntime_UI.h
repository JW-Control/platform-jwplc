#ifndef JWPLC_LOGIC_RUNTIME_UI_H
#define JWPLC_LOGIC_RUNTIME_UI_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>

#include "screens/RuntimeUIHome.h"

/**
 * @brief Interfaz USER modular del JWPLC Logic Runtime.
 *
 * El núcleo lógico permanece independiente de TFT y botonera. Esta librería
 * compañera se enlaza de forma explícita mediante begin(runtime).
 */
class JWPLC_LogicRuntime_UIClass
{
public:
  JWPLC_LogicRuntime_UIClass();

  /**
   * @brief Enlaza una instancia del runtime con la vista USER.
   *
   * Puede llamarse desde setup() aunque la TFT todavía esté inicializándose.
   */
  bool begin(JWPLC_LogicRuntime &runtime);

  /**
   * @brief Sincroniza RUN/ERR de IDLE con el estado del runtime.
   *
   * Es una operación liviana. Debe llamarse desde loop(); no dibuja la vista
   * USER porque ese refresco continúa administrado por JWPLC_Display.
   */
  void update();

  void end();
  bool isAttached() const;
  JWPLC_LogicRuntime *runtime();
  const JWPLC_LogicRuntime *runtime() const;

  void forceRedraw();

  // Entrada desde los callbacks USER fuertes definidos por esta librería.
  void onDisplayEnter();
  void onDisplayRefresh(const JWPLC_IOState *io,
                        const JWPLC_RTCState *rtc);
  void onDisplayExit();

private:
  bool criticalErrorActive() const;
  void syncIdleIndicators();

  JWPLC_LogicRuntime *_runtime;
  bool _attached;
  bool _userVisible;
  bool _lastRunLed;
  bool _lastErrLed;
  RuntimeUIHome _home;
};

extern JWPLC_LogicRuntime_UIClass JWPLC_LogicRuntime_UI;

#endif

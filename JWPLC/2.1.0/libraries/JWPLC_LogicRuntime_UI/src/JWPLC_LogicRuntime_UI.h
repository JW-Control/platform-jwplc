#ifndef JWPLC_LOGIC_RUNTIME_UI_H
#define JWPLC_LOGIC_RUNTIME_UI_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>

#include "RuntimeUIView.h"
#include "screens/RuntimeUIHome.h"
#include "screens/RuntimeUIProgram.h"

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
   * @brief Ejecuta trabajo no gráfico fuera del callback de la TFT.
   *
   * Sincroniza RUN/ERR de IDLE y procesa acciones diferidas como preparar,
   * iniciar o detener el programa. Debe llamarse desde loop().
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

  void switchView(RuntimeUIView nextView);
  void enterCurrentView();
  void refreshCurrentView(const JWPLC_IOState *io,
                          const JWPLC_RTCState *rtc);
  void exitCurrentView();
  void collectViewRequests();
  void processPendingProgramAction();

  JWPLC_LogicRuntime *_runtime;
  bool _attached;
  bool _userVisible;
  bool _lastRunLed;
  bool _lastErrLed;
  RuntimeUIView _currentView;
  volatile RuntimeUIProgramAction _pendingProgramAction;
  RuntimeUIHome _home;
  RuntimeUIProgram _program;
};

extern JWPLC_LogicRuntime_UIClass JWPLC_LogicRuntime_UI;

#endif
#ifndef JWPLC_LOGIC_RUNTIME_UI_H
#define JWPLC_LOGIC_RUNTIME_UI_H

#include <Arduino.h>
#include <JWPLC_LogicRuntime.h>
#include <JWPLC_Display.h>

#include "RuntimeUIView.h"
#include "model/RuntimeUIV2ReadModel.h"
#include "screens/RuntimeUIHome.h"
#include "screens/RuntimeUIProgram.h"
#include "screens/RuntimeUIDiagram.h"
#include "screens/RuntimeUIBlocks.h"
#include "screens/RuntimeUIFBDMapV7.h"

/**
 * @brief Interfaz USER modular del JWPLC Logic Runtime.
 *
 * El núcleo lógico permanece independiente de TFT y botonera. La API histórica
 * para JWPLC_LogicRuntime v1 se conserva. El overload v2 abre directamente el
 * mapa FBD con edición RAM encapsulada.
 */
class JWPLC_LogicRuntime_UIClass
{
public:
  JWPLC_LogicRuntime_UIClass();

  /** @brief Enlaza el runtime v1 histórico con HOME/PROGRAMA/DIAGRAMA. */
  bool begin(JWPLC_LogicRuntime &runtime);

  /**
   * @brief Enlaza el motor RAM v2 con el mapa FBD.
   *
   * Puede llamarse desde setup() aunque la TFT todavía esté inicializándose.
   * La edición v2 no escribe FRAM ni conmuta salidas físicas.
   */
  bool begin(LogicV2EnginePrototype &engine);

  /**
   * @brief Ejecuta trabajo no gráfico común fuera del callback de la TFT.
   *
   * El scan lógico v2 sigue siendo responsabilidad del sketch.
   */
  void update();

  /**
   * @brief Aplica una confirmación del editor v2 desde el loop del sketch.
   *
   * Debe llamarse inmediatamente después de `update()`. No dibuja ni adquiere
   * el bus SPI de la TFT.
   */
  void processV2EditorPending()
  {
    if (_backend == Backend::EngineV2)
    {
      _fbdMapV2.processPendingEdit();
    }
  }

  void end();
  bool isAttached() const;
  JWPLC_LogicRuntime *runtime();
  const JWPLC_LogicRuntime *runtime() const;
  LogicV2EnginePrototype *v2Engine();
  const LogicV2EnginePrototype *v2Engine() const;

  void forceRedraw();

  // Entrada desde los callbacks USER fuertes definidos por esta librería.
  void onDisplayEnter();
  void onDisplayRefresh(const JWPLC_IOState *io,
                        const JWPLC_RTCState *rtc);
  void onDisplayExit();

private:
  enum class Backend : uint8_t
  {
    None = 0,
    RuntimeV1,
    EngineV2
  };

  bool criticalErrorActive() const;
  void syncIdleIndicators();

  void switchView(RuntimeUIView nextView);
  void enterCurrentView();
  void refreshCurrentView(const JWPLC_IOState *io,
                          const JWPLC_RTCState *rtc);
  void exitCurrentView();
  void collectViewRequests();
  void processPendingProgramAction();

  Backend _backend;
  JWPLC_LogicRuntime *_runtime;
  LogicV2EnginePrototype *_v2Engine;
  bool _attached;
  bool _userVisible;
  bool _lastRunLed;
  bool _lastErrLed;
  RuntimeUIView _currentView;
  volatile RuntimeUIProgramAction _pendingProgramAction;
  RuntimeUIHome _home;
  RuntimeUIProgram _program;
  RuntimeUIDiagram _diagram;
  RuntimeUIBlocks _blocks;
  RuntimeUIV2ReadModel _v2Model;
  RuntimeUIFBDMapV7 _fbdMapV2;
};

extern JWPLC_LogicRuntime_UIClass JWPLC_LogicRuntime_UI;

#endif

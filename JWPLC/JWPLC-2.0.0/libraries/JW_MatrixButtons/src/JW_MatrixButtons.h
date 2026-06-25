#pragma once
#include <Arduino.h>

// Opcional: soporte de task en ESP32 (FreeRTOS)
#if defined(ARDUINO_ARCH_ESP32)
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#endif

class JW_MatrixButtons
{
public:
  enum EvType : uint8_t
  {
    EV_PRESS = 1,
    EV_RELEASE = 2,
    EV_REPEAT = 3
  };

  struct BtnEvent
  {
    uint8_t id;
    EvType type;
    int16_t mult;     // Para repeat: 1/10/100/1000 o el perfil configurado.
    uint32_t held_ms; // Tiempo sostenido.
  };

  struct BtnMapItem
  {
    uint8_t id;
    uint8_t row;
    uint8_t col;
  };

  JW_MatrixButtons();

  // =========================
  // Configuración principal
  // =========================

  // Modo matriz R x C.
  bool begin(const uint8_t *rowPins, uint8_t nRows,
             const uint8_t *colPins, uint8_t nCols,
             const BtnMapItem *map, uint8_t mapLen,
             uint8_t buttonCount,
             bool invertLogic = false,
             uint32_t debounceMs = 35);

  // Modo botones directos 1 x N.
  //
  // En este modo:
  // - No se usan filas.
  // - No se usa mapa.
  // - No se configura ningún pin como OUTPUT.
  // - Cada botón se configura con pinMode(buttonPins[i], inputMode).
  bool beginDirect(const uint8_t *buttonPins,
                   uint8_t buttonCount,
                   bool invertLogic = false,
                   uint32_t debounceMs = 35,
                   uint8_t inputMode = INPUT);

  // Ajustes finos (opcionales)
  void setScanDelays(uint16_t settleUs, uint16_t betweenRowsUs);
  void setRepeatEnabled(uint8_t id, bool enabled);
  void setRepeatInitialDelay(uint32_t ms);

  // Umbrales por número de repeats, steps y delays.
  void setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                        int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                        uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);

  // Llamar en loop, ideal cada 3-10 ms si no se usa task.
  void update();

  // =========================
  // ESP32: correr update() en un task
  // =========================
  // - En otros micros estas funciones devuelven false / no hacen nada.
  // - Si el task está activo, no necesitas llamar update() en loop.
  bool startTask(uint8_t core = 1,
                 uint32_t stackBytes = 4096,
                 uint8_t priority = 1,
                 uint16_t periodMs = 5);
  void stopTask();
  bool taskRunning() const;
  void setTaskPeriodMs(uint16_t periodMs);
  uint16_t taskPeriodMs() const;

  // =========================
  // Eventos
  // =========================
  // Eventos generados en el último update().
  uint8_t eventCount() const;
  bool getEvent(uint8_t index, BtnEvent &out) const;

  // Helpers de estado.
  // pressed()/released() son latcheados y consumibles:
  // si ocurre un PRESS/RELEASE, queda pendiente hasta que lo leas.
  bool isDown(uint8_t id) const;
  bool pressed(uint8_t id) const;
  bool released(uint8_t id) const;

  // Limpieza de estados/eventos pendientes.
  void clearPendingPresses() const;
  void clearPendingReleases() const;
  void clearPendingRepeats() const;
  void clearEventQueue() const;
  void clearPendingInput() const;

  // Helper genérico de eje.
  bool applyAxis(uint32_t *val, uint32_t minv, uint32_t maxv,
                 uint8_t decId, uint8_t incId,
                 bool circularWrapOnPress = true,
                 bool snapToStepOnRepeat = true) const;

  // Overload cómodo por referencia.
  inline bool applyAxis(uint32_t &val, uint32_t minv, uint32_t maxv,
                        uint8_t decId, uint8_t incId,
                        bool circularWrapOnPress = true,
                        bool snapToStepOnRepeat = true) const
  {
    return applyAxis(&val, minv, maxv, decId, incId, circularWrapOnPress, snapToStepOnRepeat);
  }

private:
  static const uint8_t MAX_ROWS = 8;
  static const uint8_t MAX_COLS = 8;
  static const uint8_t MAX_BTNS = 32;
  static const uint8_t MAX_EVENTS = 40;
  static const uint8_t REPEAT_Q = 8; // Cola por botón para repeats.

  struct DebouncedKey
  {
    bool stable;
    bool lastRaw;
    uint32_t lastChange;
  };

  enum ScanMode : uint8_t
  {
    SCAN_MODE_MATRIX = 0,
    SCAN_MODE_DIRECT
  };

  // Config
  ScanMode _scanMode;

  const uint8_t *_rowPins;
  const uint8_t *_colPins;
  uint8_t _nRows;
  uint8_t _nCols;

  const BtnMapItem *_map;
  uint8_t _mapLen;
  uint8_t _btnCount;

  const uint8_t *_directPins;
  uint8_t _directCount;
  uint8_t _inputMode;

  bool _invert;
  uint32_t _debounceMs;

  // Scan delays para modo matriz.
  uint16_t _settleUs;
  uint16_t _betweenRowsUs;

  // Raw + debounced keys para modo matriz.
  DebouncedKey _keyDeb[MAX_ROWS][MAX_COLS];
  bool _raw[MAX_ROWS][MAX_COLS];
  bool _deb[MAX_ROWS][MAX_COLS];

  // Debounce para modo directo.
  DebouncedKey _directDeb[MAX_BTNS];

  // Estado lógico por botón.
  bool _btnStable[MAX_BTNS];
  bool _btnPrev[MAX_BTNS];
  uint32_t _btnPressStart[MAX_BTNS];

  // Repeat config/state.
  bool _repeatEnabled[MAX_BTNS];
  uint32_t _repeatInitialDelay;

  uint16_t _thr1, _thr2, _thr3;
  int16_t _s1, _s2, _s3, _s4;
  uint32_t _d1, _d2, _d3, _d4;

  uint32_t _nextRepeatAt[MAX_BTNS];
  uint16_t _repeatCount[MAX_BTNS];

  // Eventos del último update.
  BtnEvent _events[MAX_EVENTS];

  // Mutable porque los helpers const limpian eventos pendientes sin cambiar
  // la identidad lógica del objeto.
  mutable uint8_t _evN;

  // Latches: persisten hasta que el sketch los consume.
  mutable uint8_t _pressPend[MAX_BTNS];
  mutable uint8_t _releasePend[MAX_BTNS];
  mutable int16_t _repQ[MAX_BTNS][REPEAT_Q];
  mutable uint8_t _repHead[MAX_BTNS];
  mutable uint8_t _repTail[MAX_BTNS];
  mutable uint8_t _repCountPend[MAX_BTNS];

#if defined(ARDUINO_ARCH_ESP32)
  // Sincronización + task.
  mutable SemaphoreHandle_t _mtx;
  volatile bool _taskRun;
  volatile TaskHandle_t _taskHandle;
  volatile uint16_t _taskPeriod;
  static void taskTrampoline(void *arg);
#endif

  inline void lock() const
  {
#if defined(ARDUINO_ARCH_ESP32)
    if (_mtx)
      xSemaphoreTake(_mtx, portMAX_DELAY);
#endif
  }

  inline void unlock() const
  {
#if defined(ARDUINO_ARCH_ESP32)
    if (_mtx)
      xSemaphoreGive(_mtx);
#endif
  }

  void resetStates();

  bool readCol(uint8_t pin) const;
  bool readDirectPin(uint8_t pin) const;

  void scanRaw(bool raw[MAX_ROWS][MAX_COLS]);
  void debounceUpdate(DebouncedKey &k, bool rawNow);

  void updateMatrix();
  void updateDirect();

  void mapButtons();
  void pushEvent(uint8_t id, EvType type, int16_t mult, uint32_t held);
  void emitEdgesAndRepeats();

  void latchEvent_(const BtnEvent &e);
  void repQPush_(uint8_t id, int16_t mult) const;
  bool repQPop_(uint8_t id, int16_t &mult) const;
};

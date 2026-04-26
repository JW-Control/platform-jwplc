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
    int16_t mult;     // para repeat: 1/10/100/1000 (o lo que configures)
    uint32_t held_ms; // tiempo sostenido
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
  bool begin(const uint8_t *rowPins, uint8_t nRows,
             const uint8_t *colPins, uint8_t nCols,
             const BtnMapItem *map, uint8_t mapLen,
             uint8_t buttonCount,
             bool invertLogic = false,
             uint32_t debounceMs = 35);

  // Ajustes finos (opcionales)
  void setScanDelays(uint16_t settleUs, uint16_t betweenRowsUs);
  void setRepeatEnabled(uint8_t id, bool enabled);
  void setRepeatInitialDelay(uint32_t ms);

  // Umbrales por número de repeats, steps y delays (por defecto ya vienen bien)
  void setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                        int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                        uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4);

  // Llamar en loop, ideal cada 3–10 ms (si NO usas task)
  void update();

  // =========================
  // ESP32: correr update() en un task (otro núcleo si quieres)
  // =========================
  // - En otros micros (sin FreeRTOS) estas funciones devuelven false / no hacen nada.
  // - Si el task está activo, NO necesitas llamar update() en loop.
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
  // Eventos generados en el último update() (útil para debug/log)
  uint8_t eventCount() const;
  bool getEvent(uint8_t index, BtnEvent &out) const;

  // Helpers de estado
  // NOTA: pressed()/released() son "latcheados":
  // - si ocurre un PRESS/RELEASE, queda pendiente hasta que lo leas.
  // - esto ayuda muchísimo si update() corre en un task o tu loop a veces tarda.
  bool isDown(uint8_t id) const;
  bool pressed(uint8_t id) const;  // consume 1 PRESS pendiente
  bool released(uint8_t id) const; // consume 1 RELEASE pendiente

  // Limpieza de estados/eventos pendientes
  void clearPendingPresses() const;  // limpia los PRESS latcheados que aún no fueron consumidos por pressed()
  void clearPendingReleases() const; // limpia los RELEASE latcheados
  void clearPendingRepeats() const;  // vacía la cola interna de repeat pendiente
  void clearEventQueue() const;      // limpia _events del último update()
  void clearPendingInput() const;    // helper general que llama a las cuatro anteriores

  // Helper genérico de “eje”
  // - circularWrapOnPress: si estás en max y haces INC (PRESS) => salta a min (y viceversa)
  // - snapToStepOnRepeat: antes de sumar/restar en REPEAT, alinea val al múltiplo del step
  bool applyAxis(uint32_t *val, uint32_t minv, uint32_t maxv,
                 uint8_t decId, uint8_t incId,
                 bool circularWrapOnPress = true,
                 bool snapToStepOnRepeat = true) const;

  // Overload cómodo: puedes pasar la variable “directa” (referencia)
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
  static const uint8_t REPEAT_Q = 8; // cola por botón para repeats

  struct DebouncedKey
  {
    bool stable;
    bool lastRaw;
    uint32_t lastChange;
  };

  // Config
  const uint8_t *_rowPins;
  const uint8_t *_colPins;
  uint8_t _nRows;
  uint8_t _nCols;

  const BtnMapItem *_map;
  uint8_t _mapLen;
  uint8_t _btnCount;

  bool _invert;
  uint32_t _debounceMs;

  // Scan delays
  uint16_t _settleUs;
  uint16_t _betweenRowsUs;

  // Raw + debounced keys
  DebouncedKey _keyDeb[MAX_ROWS][MAX_COLS];
  bool _raw[MAX_ROWS][MAX_COLS];
  bool _deb[MAX_ROWS][MAX_COLS];

  // Buttons state
  bool _btnStable[MAX_BTNS];
  bool _btnPrev[MAX_BTNS];
  uint32_t _btnPressStart[MAX_BTNS];

  // Repeat config/state
  bool _repeatEnabled[MAX_BTNS];
  uint32_t _repeatInitialDelay;

  uint16_t _thr1, _thr2, _thr3;
  int16_t _s1, _s2, _s3, _s4;
  uint32_t _d1, _d2, _d3, _d4;

  uint32_t _nextRepeatAt[MAX_BTNS];
  uint16_t _repeatCount[MAX_BTNS];

  // Eventos del último update
  BtnEvent _events[MAX_EVENTS];
  uint8_t _evN;

  // Latches (persisten hasta que los consumas)
  mutable uint8_t _pressPend[MAX_BTNS];
  mutable uint8_t _releasePend[MAX_BTNS];
  mutable int16_t _repQ[MAX_BTNS][REPEAT_Q];
  mutable uint8_t _repHead[MAX_BTNS];
  mutable uint8_t _repTail[MAX_BTNS];
  mutable uint8_t _repCountPend[MAX_BTNS];

#if defined(ARDUINO_ARCH_ESP32)
  // Sincronización + task
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
  void scanRaw(bool raw[MAX_ROWS][MAX_COLS]);
  void debounceUpdate(DebouncedKey &k, bool rawNow);
  void mapButtons();
  void pushEvent(uint8_t id, EvType type, int16_t mult, uint32_t held);
  void emitEdgesAndRepeats();

  void latchEvent_(const BtnEvent &e);
  void repQPush_(uint8_t id, int16_t mult) const;
  bool repQPop_(uint8_t id, int16_t &mult) const;
};

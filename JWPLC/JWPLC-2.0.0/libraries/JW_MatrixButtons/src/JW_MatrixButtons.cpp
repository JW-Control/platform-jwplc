#include "JW_MatrixButtons.h"

JW_MatrixButtons::JW_MatrixButtons()
    : _rowPins(nullptr), _colPins(nullptr), _nRows(0), _nCols(0),
      _map(nullptr), _mapLen(0), _btnCount(0),
      _invert(false), _debounceMs(35),
      _settleUs(120), _betweenRowsUs(40),
      _repeatInitialDelay(350),
      _thr1(12), _thr2(30), _thr3(70),
      _s1(1), _s2(10), _s3(100), _s4(1000),
      _d1(110), _d2(95), _d3(80), _d4(65),
      _evN(0)
{
#if defined(ARDUINO_ARCH_ESP32)
  _mtx = nullptr;
  _taskRun = false;
  _taskHandle = nullptr;
  _taskPeriod = 5;
#endif
  resetStates();
}

bool JW_MatrixButtons::begin(const uint8_t *rowPins, uint8_t nRows,
                           const uint8_t *colPins, uint8_t nCols,
                           const BtnMapItem *map, uint8_t mapLen,
                           uint8_t buttonCount,
                           bool invertLogic,
                           uint32_t debounceMs)
{
  stopTask();

#if defined(ARDUINO_ARCH_ESP32)
  if (!_mtx)
  {
    _mtx = xSemaphoreCreateMutex();
  }
#endif

  if (!rowPins || !colPins || !map)
    return false;
  if (nRows == 0 || nCols == 0 || nRows > MAX_ROWS || nCols > MAX_COLS)
    return false;
  if (buttonCount == 0 || buttonCount > MAX_BTNS)
    return false;
  if (mapLen == 0 || mapLen > buttonCount)
    return false;

  _rowPins = rowPins;
  _colPins = colPins;
  _nRows = nRows;
  _nCols = nCols;
  _map = map;
  _mapLen = mapLen;
  _btnCount = buttonCount;
  _invert = invertLogic;
  _debounceMs = debounceMs;

  for (uint8_t r = 0; r < _nRows; r++)
  {
    pinMode(_rowPins[r], OUTPUT);
    digitalWrite(_rowPins[r], LOW);
  }
  for (uint8_t c = 0; c < _nCols; c++)
  {
    pinMode(_colPins[c], INPUT);
  }

  lock();
  resetStates();
  _evN = 0;
  unlock();
  return true;
}

void JW_MatrixButtons::setScanDelays(uint16_t settleUs, uint16_t betweenRowsUs)
{
  lock();
  _settleUs = settleUs;
  _betweenRowsUs = betweenRowsUs;
  unlock();
}

void JW_MatrixButtons::setRepeatEnabled(uint8_t id, bool enabled)
{
  if (id >= _btnCount)
    return;
  lock();
  _repeatEnabled[id] = enabled;
  unlock();
}

void JW_MatrixButtons::setRepeatInitialDelay(uint32_t ms)
{
  lock();
  _repeatInitialDelay = ms;
  unlock();
}

void JW_MatrixButtons::setRepeatProfile(uint16_t thr1, uint16_t thr2, uint16_t thr3,
                                      int16_t s1, int16_t s2, int16_t s3, int16_t s4,
                                      uint32_t d1, uint32_t d2, uint32_t d3, uint32_t d4)
{
  lock();
  _thr1 = thr1;
  _thr2 = thr2;
  _thr3 = thr3;
  _s1 = s1;
  _s2 = s2;
  _s3 = s3;
  _s4 = s4;
  _d1 = d1;
  _d2 = d2;
  _d3 = d3;
  _d4 = d4;
  unlock();
}

// =========================
// ESP32 task
// =========================

bool JW_MatrixButtons::startTask(uint8_t core, uint32_t stackBytes, uint8_t priority, uint16_t periodMs)
{
#if !defined(ARDUINO_ARCH_ESP32)
  (void)core;
  (void)stackBytes;
  (void)priority;
  (void)periodMs;
  return false;
#else
  if (!_mtx)
    _mtx = xSemaphoreCreateMutex();

  _taskPeriod = periodMs;

  if (_taskHandle)
  {
    _taskRun = true;
    return true;
  }

  _taskRun = true;

  uint32_t stackWords = stackBytes / sizeof(StackType_t);
  if (stackWords < 1024)
    stackWords = 1024;

  TaskHandle_t handle = nullptr;
  BaseType_t ok = xTaskCreatePinnedToCore(
      &JW_MatrixButtons::taskTrampoline,
      "JWMB",
      (uint32_t)stackWords,
      this,
      (UBaseType_t)priority,
      &handle,
      (BaseType_t)core);

  if (ok != pdPASS)
  {
    _taskRun = false;
    _taskHandle = nullptr;
    return false;
  }

  _taskHandle = handle;
  return true;
#endif
}

void JW_MatrixButtons::stopTask()
{
#if defined(ARDUINO_ARCH_ESP32)
  if (!_taskHandle)
    return;

  _taskRun = false;

  uint32_t t0 = millis();
  while (_taskHandle && (millis() - t0) < 200)
    delay(1);

  if (_taskHandle)
  {
    vTaskDelete((TaskHandle_t)_taskHandle);
    _taskHandle = nullptr;
  }
#endif
}

bool JW_MatrixButtons::taskRunning() const
{
#if defined(ARDUINO_ARCH_ESP32)
  return _taskHandle != nullptr;
#else
  return false;
#endif
}

void JW_MatrixButtons::setTaskPeriodMs(uint16_t periodMs)
{
#if defined(ARDUINO_ARCH_ESP32)
  _taskPeriod = periodMs;
#else
  (void)periodMs;
#endif
}

uint16_t JW_MatrixButtons::taskPeriodMs() const
{
#if defined(ARDUINO_ARCH_ESP32)
  return (uint16_t)_taskPeriod;
#else
  return 0;
#endif
}

#if defined(ARDUINO_ARCH_ESP32)
void JW_MatrixButtons::taskTrampoline(void *arg)
{
  JW_MatrixButtons *self = static_cast<JW_MatrixButtons *>(arg);
  while (self && self->_taskRun)
  {
    self->update();
    vTaskDelay(pdMS_TO_TICKS(self->_taskPeriod));
  }

  if (self)
    self->_taskHandle = nullptr;

  vTaskDelete(nullptr);
}
#endif

// =========================
// Eventos / estado
// =========================

uint8_t JW_MatrixButtons::eventCount() const
{
  lock();
  uint8_t n = _evN;
  unlock();
  return n;
}

bool JW_MatrixButtons::getEvent(uint8_t index, BtnEvent &out) const
{
  lock();
  if (index >= _evN)
  {
    unlock();
    return false;
  }
  out = _events[index];
  unlock();
  return true;
}

bool JW_MatrixButtons::isDown(uint8_t id) const
{
  if (id >= _btnCount)
    return false;
  lock();
  bool v = _btnStable[id];
  unlock();
  return v;
}

bool JW_MatrixButtons::pressed(uint8_t id) const
{
  if (id >= _btnCount)
    return false;
  lock();
  bool ok = (_pressPend[id] > 0);
  if (ok)
    _pressPend[id]--;
  unlock();
  return ok;
}

bool JW_MatrixButtons::released(uint8_t id) const
{
  if (id >= _btnCount)
    return false;
  lock();
  bool ok = (_releasePend[id] > 0);
  if (ok)
    _releasePend[id]--;
  unlock();
  return ok;
}

void JW_MatrixButtons::clearPendingPresses() const {
  lock();
  for (uint8_t i = 0; i < _btnCount; i++) {
    _pressPend[i] = 0;
  }
  unlock();
}

void JW_MatrixButtons::clearPendingReleases() const {
  lock();
  for (uint8_t i = 0; i < _btnCount; i++) {
    _releasePend[i] = 0;
  }
  unlock();
}

void JW_MatrixButtons::clearPendingRepeats() const {
  lock();
  for (uint8_t i = 0; i < _btnCount; i++) {
    _repHead[i] = 0;
    _repTail[i] = 0;
    _repCountPend[i] = 0;
  }
  unlock();
}

void JW_MatrixButtons::clearEventQueue() const {
  lock();
  _evN = 0;
  unlock();
}

void JW_MatrixButtons::clearPendingInput() const {
  lock();
  for (uint8_t i = 0; i < _btnCount; i++) {
    _pressPend[i] = 0;
    _releasePend[i] = 0;
    _repHead[i] = 0;
    _repTail[i] = 0;
    _repCountPend[i] = 0;
  }
  _evN = 0;
  unlock();
}

// =========================
// Core scanning
// =========================

void JW_MatrixButtons::update()
{
  if (!_rowPins || !_colPins || _nRows == 0 || _nCols == 0)
    return;

  lock();

  // 1) scan raw
  scanRaw(_raw);

  // 2) debounce
  uint32_t now = millis();
  for (uint8_t r = 0; r < _nRows; r++)
  {
    for (uint8_t c = 0; c < _nCols; c++)
    {
      debounceUpdate(_keyDeb[r][c], _raw[r][c]);
      // estable
      _deb[r][c] = _keyDeb[r][c].stable;
    }
  }

  // 3) map to button ids
  mapButtons();

  // 4) generate edges + repeats
  _evN = 0;
  emitEdgesAndRepeats();

  // 5) latch events (para no perderlos)
  for (uint8_t i = 0; i < _evN; i++)
  {
    latchEvent_(_events[i]);
  }

  unlock();
  (void)now;
}

void JW_MatrixButtons::resetStates()
{
  for (uint8_t r = 0; r < MAX_ROWS; r++)
  {
    for (uint8_t c = 0; c < MAX_COLS; c++)
    {
      _raw[r][c] = false;
      _deb[r][c] = false;
      _keyDeb[r][c].stable = false;
      _keyDeb[r][c].lastRaw = false;
      _keyDeb[r][c].lastChange = 0;
    }
  }

  for (uint8_t i = 0; i < MAX_BTNS; i++)
  {
    _btnStable[i] = false;
    _btnPrev[i] = false;
    _btnPressStart[i] = 0;

    _repeatEnabled[i] = false;
    _nextRepeatAt[i] = 0;
    _repeatCount[i] = 0;

    _pressPend[i] = 0;
    _releasePend[i] = 0;
    _repHead[i] = 0;
    _repTail[i] = 0;
    _repCountPend[i] = 0;
    for (uint8_t k = 0; k < REPEAT_Q; k++)
      _repQ[i][k] = 0;
  }
}

bool JW_MatrixButtons::readCol(uint8_t pin) const
{
  int v = digitalRead(pin);
  return _invert ? (v == LOW) : (v == HIGH);
}

void JW_MatrixButtons::scanRaw(bool raw[MAX_ROWS][MAX_COLS])
{
  // filas una por una
  for (uint8_t r = 0; r < _nRows; r++)
  {
    // activar solo esta fila
    for (uint8_t rr = 0; rr < _nRows; rr++)
      digitalWrite(_rowPins[rr], (rr == r) ? HIGH : LOW);

    if (_settleUs)
      delayMicroseconds(_settleUs);

    for (uint8_t c = 0; c < _nCols; c++)
      raw[r][c] = readCol(_colPins[c]);

    if (_betweenRowsUs)
      delayMicroseconds(_betweenRowsUs);
  }

  // apagar filas
  for (uint8_t rr = 0; rr < _nRows; rr++)
    digitalWrite(_rowPins[rr], LOW);
}

void JW_MatrixButtons::debounceUpdate(DebouncedKey &k, bool rawNow)
{
  uint32_t now = millis();

  if (rawNow != k.lastRaw)
  {
    k.lastRaw = rawNow;
    k.lastChange = now;
  }

  if ((now - k.lastChange) >= _debounceMs)
  {
    k.stable = rawNow;
  }
}

void JW_MatrixButtons::mapButtons()
{
  // reset
  for (uint8_t i = 0; i < _btnCount; i++)
    _btnStable[i] = false;

  for (uint8_t i = 0; i < _mapLen; i++)
  {
    const BtnMapItem &m = _map[i];
    if (m.id >= _btnCount)
      continue;
    if (m.row >= _nRows || m.col >= _nCols)
      continue;

    _btnStable[m.id] = _deb[m.row][m.col];
  }
}

void JW_MatrixButtons::pushEvent(uint8_t id, EvType type, int16_t mult, uint32_t held)
{
  if (_evN >= MAX_EVENTS)
    return;
  _events[_evN].id = id;
  _events[_evN].type = type;
  _events[_evN].mult = mult;
  _events[_evN].held_ms = held;
  _evN++;
}

void JW_MatrixButtons::emitEdgesAndRepeats()
{
  uint32_t now = millis();

  for (uint8_t id = 0; id < _btnCount; id++)
  {
    bool cur = _btnStable[id];
    bool prev = _btnPrev[id];

    if (cur && !prev)
    {
      // PRESS
      _btnPressStart[id] = now;
      _repeatCount[id] = 0;
      _nextRepeatAt[id] = now + _repeatInitialDelay;
      pushEvent(id, EV_PRESS, 0, 0);
    }
    else if (!cur && prev)
    {
      // RELEASE
      uint32_t held = (now >= _btnPressStart[id]) ? (now - _btnPressStart[id]) : 0;
      pushEvent(id, EV_RELEASE, 0, held);
      _repeatCount[id] = 0;
      _nextRepeatAt[id] = 0;
    }

    // REPEAT
    if (cur && _repeatEnabled[id])
    {
      uint32_t held = (now >= _btnPressStart[id]) ? (now - _btnPressStart[id]) : 0;

      if (_repeatCount[id] == 0)
      {
        // primer repeat
        if (now >= _nextRepeatAt[id])
        {
          _repeatCount[id] = 1;
          int16_t step = _s1;
          uint32_t delayMs = _d1;
          _nextRepeatAt[id] = now + delayMs;
          pushEvent(id, EV_REPEAT, step, held);
        }
      }
      else
      {
        if (now >= _nextRepeatAt[id])
        {
          _repeatCount[id]++;

          int16_t step = _s1;
          uint32_t delayMs = _d1;

          if (_repeatCount[id] >= _thr3)
          {
            step = _s4;
            delayMs = _d4;
          }
          else if (_repeatCount[id] >= _thr2)
          {
            step = _s3;
            delayMs = _d3;
          }
          else if (_repeatCount[id] >= _thr1)
          {
            step = _s2;
            delayMs = _d2;
          }

          _nextRepeatAt[id] = now + delayMs;
          pushEvent(id, EV_REPEAT, step, held);
        }
      }
    }

    _btnPrev[id] = cur;
  }
}

// =========================
// Latching helpers
// =========================

void JW_MatrixButtons::latchEvent_(const BtnEvent &e)
{
  if (e.id >= _btnCount)
    return;

  if (e.type == EV_PRESS)
  {
    if (_pressPend[e.id] < 255)
      _pressPend[e.id]++;
  }
  else if (e.type == EV_RELEASE)
  {
    if (_releasePend[e.id] < 255)
      _releasePend[e.id]++;
  }
  else if (e.type == EV_REPEAT)
  {
    repQPush_(e.id, e.mult);
  }
}

void JW_MatrixButtons::repQPush_(uint8_t id, int16_t mult) const
{
  if (id >= _btnCount)
    return;

  uint8_t &h = _repHead[id];
  uint8_t &t = _repTail[id];
  uint8_t &n = _repCountPend[id];

  if (n >= REPEAT_Q)
  {
    // drop oldest
    h = (uint8_t)((h + 1) % REPEAT_Q);
    n--;
  }

  _repQ[id][t] = mult;
  t = (uint8_t)((t + 1) % REPEAT_Q);
  n++;
}

bool JW_MatrixButtons::repQPop_(uint8_t id, int16_t &mult) const
{
  if (id >= _btnCount)
    return false;

  uint8_t &h = _repHead[id];
  uint8_t &n = _repCountPend[id];

  if (n == 0)
    return false;

  mult = _repQ[id][h];
  h = (uint8_t)((h + 1) % REPEAT_Q);
  n--;
  return true;
}

// =========================
// applyAxis
// =========================

bool JW_MatrixButtons::applyAxis(uint32_t *val, uint32_t minv, uint32_t maxv,
                               uint8_t decId, uint8_t incId,
                               bool circularWrapOnPress,
                               bool snapToStepOnRepeat) const
{
  if (!val)
    return false;
  if (minv > maxv)
    return false;
  if (decId >= _btnCount || incId >= _btnCount)
    return false;

  uint32_t v = *val;
  bool changed = false;

  // Consumir presses pendientes de los 2 botones
  uint8_t decPress = 0, incPress = 0;
  lock();
  decPress = _pressPend[decId];
  incPress = _pressPend[incId];
  _pressPend[decId] = 0;
  _pressPend[incId] = 0;
  unlock();

  // --- PRESS: dec
  for (uint8_t i = 0; i < decPress; i++)
  {
    if (v <= minv)
    {
      if (circularWrapOnPress && (minv < maxv))
      {
        v = maxv;
        changed = true;
      }
    }
    else
    {
      v--;
      changed = true;
    }
  }

  // --- PRESS: inc
  for (uint8_t i = 0; i < incPress; i++)
  {
    if (v >= maxv)
    {
      if (circularWrapOnPress && (minv < maxv))
      {
        v = minv;
        changed = true;
      }
    }
    else
    {
      v++;
      changed = true;
    }
  }

  // Consumir repeats (cola) y aplicarlos en orden
  auto applyRepeatStep = [&](bool isInc, int16_t step) {
    if (step <= 0)
      step = 1;

    if (isInc)
    {
      if (v >= maxv)
        return; // ignorar repeats en el tope
      uint32_t base = v;
      if (snapToStepOnRepeat)
        base = (step > 0) ? (uint32_t)((v / (uint32_t)step) * (uint32_t)step) : v;

      uint32_t nv = base + (uint32_t)step;
      if (nv > maxv)
        nv = maxv;
      if (nv != v)
      {
        v = nv;
        changed = true;
      }
    }
    else
    {
      if (v <= minv)
        return; // ignorar repeats en el tope
      uint32_t base = v;
      if (snapToStepOnRepeat)
        base = (step > 0) ? (uint32_t)((v / (uint32_t)step) * (uint32_t)step) : v;

      // base podría ser menor que v, pero aquí solo usamos base como punto de salto
      uint32_t nv;
      if (base >= (uint32_t)step)
        nv = base - (uint32_t)step;
      else
        nv = 0;

      if (nv < minv)
        nv = minv;
      if (nv != v)
      {
        v = nv;
        changed = true;
      }
    }
  };

  // Pop repeats de dec e inc
  // Nota: el orden entre dec/inc no importa si el usuario no presiona ambos a la vez.
  // Si los presiona, se aplicarán primero dec y luego inc.
  for (;;)
  {
    int16_t step = 0;
    lock();
    bool ok = repQPop_(decId, step);
    unlock();
    if (!ok)
      break;
    applyRepeatStep(false, step);
  }

  for (;;)
  {
    int16_t step = 0;
    lock();
    bool ok = repQPop_(incId, step);
    unlock();
    if (!ok)
      break;
    applyRepeatStep(true, step);
  }

  *val = v;
  return changed;
}

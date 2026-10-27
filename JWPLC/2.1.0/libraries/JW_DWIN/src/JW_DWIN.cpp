#include "JW_DWIN.h"

JW_DWIN::JW_DWIN(JW_DWIN_Transport &transport)
    : _transport(transport),
      _started(false),
      _epoch(1),  // Los objetos nacen con epoch 0: no confirmados
      _failures(0),
      _offlineFailures(JW_DWIN_OFFLINE_FAILURES),
      _offlineRetryMs(JW_DWIN_OFFLINE_RETRY_MS),
      _failureTotal(0),
      _lastAttemptMs(0),
      _lastSuccessMs(0),
      _reconnected(false),
      _page(0),
      _touchDelayMs(25),
      _dbg(nullptr),
      _dbgMask(DWIN_DBG_NONE)
{
}

bool JW_DWIN::begin()
{
    _started = _transport.begin();
    if (!_started && _dbg && (_dbgMask & DWIN_DBG_ERR))
    {
        dbgHeader("begin");
        _dbg->println(_transport.lastErrorString());
    }
    return _started;
}

bool JW_DWIN::setPage(uint16_t page)
{
    if (_dbg && (_dbgMask & DWIN_DBG_FLOW))
    {
        dbgHeader("setPage");
        _dbg->printf("page=0x%04X\n", page);
    }

    const uint16_t frame[2] = {0x5A01, page};
    const bool ok = writeRegisters(0x0084, frame, 2, "setPage");
    if (ok)
    {
        _page = page;
    }
    return ok;
}

void JW_DWIN::sendValue(DWIN_Value &obj, uint16_t newValue, bool forceSend, bool debug)
{
    if (obj.val != newValue || obj.epoch != _epoch || forceSend)
    {
        obj.val = newValue;
        obj.fval = obj.val / divider(obj.decimal);
        // Sin confirmación de la pantalla se reintenta en la próxima llamada.
        obj.epoch = writeRegister(vp(obj.page, obj.addr), newValue, "sendValue")
            ? _epoch
            : 0;

        if (debug && _dbg)
        {
            dbgHeader("sendValue-usr");
            _dbg->print("fval=");
            _dbg->println(obj.fval, obj.decimal);
        }
    }
}

bool JW_DWIN::readValue(DWIN_Value &obj, bool debug)
{
    uint16_t newVal = 0;
    const uint16_t epochBefore = _epoch;
    if (!readRegister(vp(obj.page, obj.addr), newVal, "readValue"))
    {
        return false;
    }

    if (_epoch != epochBefore)
    {
        // Esta lectura restableció el enlace: el valor leído puede ser el
        // inicial de una pantalla reiniciada, no una edición del operador.
        obj.epoch = 0;
        return false;
    }

    obj.epoch = _epoch;
    if (newVal != obj.val)
    {
        obj.val = newVal;
        obj.fval = obj.val / divider(obj.decimal);
        if (debug && _dbg)
        {
            dbgHeader("readValue-usr");
            _dbg->print("fval=");
            _dbg->println(obj.fval, obj.decimal);
        }
        return true;
    }
    return false;
}

bool JW_DWIN::sendText(DWIN_String &obj, const String &newText, bool forceSend)
{
    if (obj.val == newText && obj.epoch == _epoch && !forceSend)
    {
        return true;
    }

    if (obj.Length <= 0 || obj.Length > JW_DWIN_MAX_TEXT)
    {
        return false;
    }

    // Texto de largo fijo: se recorta o se completa con espacios.
    String full = newText;
    if (full.length() > (unsigned)obj.Length)
        full.remove(obj.Length);
    while (full.length() < (unsigned)obj.Length)
        full += ' ';

    const char *text = full.c_str();
    const uint16_t numRegisters = (obj.Length + 1) / 2;
    uint16_t buffer[(JW_DWIN_MAX_TEXT + 1) / 2];

    for (uint16_t i = 0; i < numRegisters; i++)
    {
        uint16_t v = 0;
        uint16_t idx = i * 2;
        if (idx < (uint16_t)obj.Length)
            v |= (uint16_t)(uint8_t)text[idx] << 8;
        if ((idx + 1) < (uint16_t)obj.Length)
            v |= (uint16_t)(uint8_t)text[idx + 1];
        buffer[i] = v;
    }

    obj.val = newText;
    const bool ok = writeRegisters(vp(obj.page, obj.addr), buffer, numRegisters, "sendText");
    obj.epoch = ok ? _epoch : 0;
    return ok;
}

void JW_DWIN::checkKey(DWIN_Key &key)
{
    const uint16_t address = vp(key.page, key.addr);
    uint16_t readVal = 0;
    if (readRegister(address, readVal, "checkKey") && readVal != 0)
    {
        key.Enable = 1;
        // Limpiar en HMI
        writeRegister(address, 0, "checkKey");
    }
}

template <typename T>
bool JW_DWIN::nextSpan(T *const objs[], uint8_t count, uint32_t &from,
                       uint16_t &start, uint16_t &n)
{
    // El tramo empieza en el menor VP aún sin leer y suma los siguientes
    // mientras el hueco y el largo de la lectura lo permitan.
    uint32_t first = 0x10000;
    for (uint8_t i = 0; i < count; i++)
    {
        if (!objs[i])
            continue;
        const uint32_t a = vp(objs[i]->page, objs[i]->addr);
        if (a >= from && a < first)
            first = a;
    }
    if (first > 0xFFFF)
        return false;

    uint32_t end = first;
    while (true)
    {
        uint32_t next = 0x10000;
        for (uint8_t i = 0; i < count; i++)
        {
            if (!objs[i])
                continue;
            const uint32_t a = vp(objs[i]->page, objs[i]->addr);
            if (a > end && a < next)
                next = a;
        }
        if (next > 0xFFFF || next - end - 1 > JW_DWIN_READ_MAX_GAP ||
            next - first + 1 > JW_DWIN_MAX_READ)
            break;
        end = next;
    }
    from = end + 1;
    start = first;
    n = end - first + 1;
    return true;
}

bool JW_DWIN::checkKeys(DWIN_Key *const keys[], uint8_t count)
{
    bool ok = true;
    uint32_t from = 0; // Primer VP aún sin leer
    uint16_t start, n;

    while (nextSpan(keys, count, from, start, n))
    {
        uint16_t values[JW_DWIN_MAX_READ];
        if (!readRegisters(start, n, values, "checkKeys"))
        {
            ok = false;
            continue;
        }

        for (uint8_t i = 0; i < count; i++)
        {
            if (!keys[i])
                continue;
            const uint32_t a = vp(keys[i]->page, keys[i]->addr);
            if (a >= start && a < start + n && values[a - start] != 0)
                keys[i]->Enable = 1;
        }
        // Limpiar en HMI una vez por VP (dos teclas pueden compartirlo).
        for (uint8_t i = 0; i < count; i++)
        {
            if (!keys[i])
                continue;
            const uint32_t a = vp(keys[i]->page, keys[i]->addr);
            if (a >= start && a < start + n && values[a - start] != 0)
            {
                values[a - start] = 0;
                writeRegister(a, 0, "checkKeys");
            }
        }
    }
    return ok;
}

bool JW_DWIN::readValues(DWIN_Value *const objs[], uint8_t count, bool changed[])
{
    if (changed)
        for (uint8_t i = 0; i < count; i++)
            changed[i] = false;

    bool ok = true;
    uint32_t from = 0; // Primer VP aún sin leer
    uint16_t start, n;

    while (nextSpan(objs, count, from, start, n))
    {
        uint16_t values[JW_DWIN_MAX_READ];
        const uint16_t epochBefore = _epoch;
        if (!readRegisters(start, n, values, "readValues"))
        {
            ok = false;
            continue;
        }

        for (uint8_t i = 0; i < count; i++)
        {
            if (!objs[i])
                continue;
            DWIN_Value &obj = *objs[i];
            const uint32_t a = vp(obj.page, obj.addr);
            if (a < start || a >= start + n)
                continue;
            // Como readValue: la lectura que restablece el enlace no cuenta.
            if (_epoch != epochBefore)
            {
                obj.epoch = 0;
                continue;
            }
            obj.epoch = _epoch;
            if (values[a - start] != obj.val)
            {
                obj.val = values[a - start];
                obj.fval = obj.val / divider(obj.decimal);
                if (changed)
                    changed[i] = true;
            }
        }
    }
    return ok;
}

void JW_DWIN::keyEncoder(DWIN_Key &minus, DWIN_Key &plus,
                         uint32_t &count,
                         uint32_t minv, uint32_t maxv)
{
    checkKey(minus);
    if (minus.Enable)
    {
        minus.Enable = 0;
        if (count == minv)
            count = maxv;
        else
            count--;
        if (_dbg && (_dbgMask & DWIN_DBG_FLOW))
        {
            dbgHeader("keyEncoder");
            _dbg->printf("count-- -> 0x%08lX\n", (unsigned long)count);
        }
    }

    checkKey(plus);
    if (plus.Enable)
    {
        plus.Enable = 0;
        if (count == maxv)
            count = minv;
        else
            count++;
        if (_dbg && (_dbgMask & DWIN_DBG_FLOW))
        {
            dbgHeader("keyEncoder");
            _dbg->printf("count++ -> 0x%08lX\n", (unsigned long)count);
        }
    }
}

bool JW_DWIN::readRegister(uint16_t vpAddress, uint16_t &value)
{
    return readRegister(vpAddress, value, "readRegister");
}

bool JW_DWIN::readRegisters(uint16_t vpAddress, uint16_t count, uint16_t *values)
{
    if (count == 0)
        return false;
    return readRegisters(vpAddress, count, values, "readRegisters");
}

bool JW_DWIN::writeRegister(uint16_t vpAddress, uint16_t value)
{
    return writeRegister(vpAddress, value, "writeRegister");
}

bool JW_DWIN::isOnline() const
{
    return _failures < _offlineFailures;
}

uint32_t JW_DWIN::failureCount() const
{
    return _failureTotal;
}

uint32_t JW_DWIN::lastSuccessMs() const
{
    return _lastSuccessMs;
}

uint16_t JW_DWIN::currentPage() const
{
    return _page;
}

bool JW_DWIN::reconnected()
{
    const bool value = _reconnected;
    _reconnected = false;
    return value;
}

void JW_DWIN::setOfflinePolicy(uint8_t failures, uint32_t retryMs)
{
    _offlineFailures = failures > 0 ? failures : 1;
    _offlineRetryMs = retryMs;
}

void JW_DWIN::setTouchDelay(uint16_t delayMs)
{
    _touchDelayMs = delayMs;
}

void JW_DWIN::invalidateCache()
{
    _epoch++;
    if (_epoch == 0)
        _epoch = 1;
}

const char *JW_DWIN::lastErrorString() const
{
    return _transport.lastErrorString();
}

void JW_DWIN::setDebug(bool enable)
{
    _dbg = enable ? &Serial : nullptr;
    _dbgMask = enable ? DWIN_DBG_ALL : DWIN_DBG_NONE;
}

void JW_DWIN::setDebug(Stream &out, uint16_t mask)
{
    _dbg = &out;
    _dbgMask = mask;
}

void JW_DWIN::setDebugMask(uint16_t mask)
{
    _dbgMask = mask;
}

float JW_DWIN::divider(uint8_t decimals)
{
    float d = 1.0f;
    for (uint8_t i = 0; i < decimals; i++)
        d *= 10.0f;
    return d;
}

// Sin enlace solo se permite un intento cada _offlineRetryMs, para que el
// loop de la aplicación siga atendiendo el proceso sin pantalla.
bool JW_DWIN::allowAttempt()
{
    if (!_started)
        return false;
    if (isOnline())
        return true;
    return (uint32_t)(millis() - _lastAttemptMs) >= _offlineRetryMs;
}

void JW_DWIN::registerResult(bool ok)
{
    _lastAttemptMs = millis();
    if (ok)
    {
        _lastSuccessMs = _lastAttemptMs;
        if (!isOnline())
        {
            // La DWIN pudo reiniciarse: toda la cache deja de ser válida.
            invalidateCache();
            _reconnected = true;
            if (_dbg && (_dbgMask & DWIN_DBG_ERR))
            {
                dbgHeader("link");
                _dbg->println("online");
            }
        }
        _failures = 0;
        return;
    }

    _failureTotal++;
    if (_failures < 255)
        _failures++;
    if (_failures == _offlineFailures && _dbg && (_dbgMask & DWIN_DBG_ERR))
    {
        dbgHeader("link");
        _dbg->print("offline: ");
        _dbg->println(_transport.lastErrorString());
    }
}

bool JW_DWIN::writeRegister(uint16_t address, uint16_t value, const char *tag)
{
    if (!allowAttempt())
        return false;
    const uint32_t t0 = micros();
    const bool ok = _transport.writeRegister(address, value);
    registerResult(ok);
    dbgResult(tag, address, value, ok, t0);
    return ok;
}

bool JW_DWIN::writeRegisters(uint16_t address, const uint16_t *values, uint16_t count,
                             const char *tag)
{
    if (!allowAttempt())
        return false;
    const uint32_t t0 = micros();
    const bool ok = _transport.writeRegisters(address, count, values);
    registerResult(ok);
    dbgResult(tag, address, count, ok, t0);
    return ok;
}

bool JW_DWIN::readRegister(uint16_t address, uint16_t &value, const char *tag)
{
    if (!allowAttempt())
        return false;
    const uint32_t t0 = micros();
    uint16_t buffer = 0;
    const bool ok = _transport.readRegisters(address, 1, &buffer);
    registerResult(ok);
    if (ok)
    {
        value = buffer;
        if (_dbg && (_dbgMask & DWIN_DBG_RX))
        {
            dbgHeader("RX");
            _dbg->printf("addr=0x%04X val=%u\n", address, buffer);
        }
    }
    dbgResult(tag, address, buffer, ok, t0);
    return ok;
}

bool JW_DWIN::readRegisters(uint16_t address, uint16_t count, uint16_t *values,
                            const char *tag)
{
    if (!allowAttempt())
        return false;
    const uint32_t t0 = micros();
    const bool ok = _transport.readRegisters(address, count, values);
    registerResult(ok);
    if (ok && _dbg && (_dbgMask & DWIN_DBG_RX))
    {
        dbgHeader("RX");
        _dbg->printf("addr=0x%04X n=%u\n", address, count);
    }
    dbgResult(tag, address, count, ok, t0);
    return ok;
}

void JW_DWIN::dbgHeader(const char *tag)
{
    if (!_dbg)
        return;
    _dbg->print('[');
    _dbg->print((uint32_t)millis());
    _dbg->print("] ");
    _dbg->print(tag);
    _dbg->print(" | ");
}

void JW_DWIN::dbgResult(const char *tag, uint16_t address, uint16_t value,
                        bool ok, uint32_t t0)
{
    if (!_dbg)
        return;
    const bool show = (_dbgMask & DWIN_DBG_TX) || (!ok && (_dbgMask & DWIN_DBG_ERR));
    if (!show)
        return;
    dbgHeader(tag);
    _dbg->printf("addr=0x%04X val=%u %s", address, value,
                 ok ? "OK" : _transport.lastErrorString());
    if (_dbgMask & DWIN_DBG_TIME)
    {
        _dbg->printf(" t=%luus", (unsigned long)(micros() - t0));
    }
    _dbg->println();
}

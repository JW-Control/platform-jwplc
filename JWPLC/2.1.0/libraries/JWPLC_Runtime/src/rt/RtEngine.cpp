#include "RtEngine.h"

#include <string.h>

namespace jwrt
{

void Engine::load(const Program &p)
{
    _p = p;
    if (_p.count > MAX_BLOCKS)
    {
        _p.count = MAX_BLOCKS;
    }
    buildOrder();
    reset();
}

void Engine::reset()
{
    memset(_out, 0, sizeof(_out));
    memset(_prevIn, 0, sizeof(_prevIn));
    memset(_t0, 0, sizeof(_t0));
    memset(_acc, 0, sizeof(_acc));
    memset(_cnt, 0, sizeof(_cnt));
    memset(_timing, 0, sizeof(_timing));
    _q = 0;
    _m = 0;
    memset(_xq, 0, sizeof(_xq));
    _alarms = 0;
}

// Recorrido en profundidad: cada bloque se evalúa después de sus fuentes.
// state: 0 = sin visitar, 1 = en curso (lazo), 2 = listo.
void Engine::visit(uint8_t b, uint8_t state[])
{
    if (state[b] != 0)
    {
        return; // listo, o lazo: esa entrada leerá el scan anterior
    }
    state[b] = 1;
    for (uint8_t k = 0; k < MAX_PINS; k++)
    {
        const uint8_t s = _p.blocks[b].in[k];
        if (srcIsBlock(s) && srcBlock(s) < _p.count)
        {
            visit(srcBlock(s), state);
        }
    }
    state[b] = 2;
    _order[_orderCount++] = b;
}

void Engine::buildOrder()
{
    uint8_t state[MAX_BLOCKS];
    memset(state, 0, sizeof(state));
    _orderCount = 0;
    for (uint8_t b = 0; b < _p.count; b++)
    {
        visit(b, state);
    }
}

bool Engine::readSrc(uint8_t src) const
{
    if (src == SRC_HI)
    {
        return true;
    }
    if (src >= SRC_I && src < SRC_I + NUM_I)
    {
        return (_in >> (src - SRC_I)) & 1;
    }
    if (src >= SRC_Q && src < SRC_Q + NUM_Q)
    {
        return (_q >> (src - SRC_Q)) & 1;
    }
    if (src >= SRC_M && src < SRC_M + NUM_M)
    {
        return (_m >> (src - SRC_M)) & 1;
    }
    if (srcIsRemote(src))
    {
        return (_xi[srcModule(src)] >> ((src - SRC_XI) % X_IO)) & 1;
    }
    if (srcIsBlock(src) && srcBlock(src) < _p.count)
    {
        return _out[srcBlock(src)];
    }
    return false; // lo, x
}

bool Engine::sourceValue(uint8_t src) const
{
    return readSrc(src);
}

// Entrada k del bloque b, con su negación. openValue es el neutro del bloque
// (1 en AND, 0 en OR) para que una entrada libre no cambie el resultado.
bool Engine::pin(uint8_t b, uint8_t k, bool openValue) const
{
    const uint8_t s = _p.blocks[b].in[k];
    bool v = (s == SRC_OPEN) ? openValue : readSrc(s);
    if (s != SRC_OPEN && ((_p.blocks[b].neg >> k) & 1))
    {
        v = !v;
    }
    return v;
}

bool Engine::pinValue(uint8_t b, uint8_t k) const
{
    if (b >= _p.count || k >= MAX_PINS)
    {
        return false;
    }
    const uint8_t t = _p.blocks[b].type;
    const bool neutral = (t == BT_AND || t == BT_NAND);
    return pin(b, k, neutral);
}

void Engine::evalBlock(uint8_t b, uint32_t now, const ClockNow &clock)
{
    Block &blk = _p.blocks[b];
    const uint8_t base = (uint8_t)((blk.flags & BF_BASE_MASK) >> BF_BASE_SHIFT);
    const bool wasOut = _out[b];
    bool out = wasOut;
    uint8_t cur = 0; // valores de las entradas en este scan (para flancos)

    switch (blk.type)
    {
    case BT_AND:
    case BT_NAND:
    {
        bool v = true;
        bool any = false;
        for (uint8_t k = 0; k < MAX_PINS; k++)
        {
            if (blk.in[k] != SRC_OPEN)
            {
                any = true;
                v = v && pin(b, k, true);
            }
        }
        if (!any)
        {
            v = false; // AND sin entradas no conecta nada
        }
        out = (blk.type == BT_AND) ? v : !v;
        break;
    }
    case BT_OR:
    case BT_NOR:
    {
        bool v = false;
        for (uint8_t k = 0; k < MAX_PINS; k++)
        {
            v = v || pin(b, k, false);
        }
        out = (blk.type == BT_OR) ? v : !v;
        break;
    }
    case BT_XOR:
        out = pin(b, 0, false) != pin(b, 1, false);
        break;
    case BT_NOT:
        out = !pin(b, 0, false);
        break;
    case BT_EDGE_UP:
    {
        const bool v = pin(b, 0, false);
        cur = v;
        out = v && !(_prevIn[b] & 1);
        break;
    }
    case BT_EDGE_DN:
    {
        const bool v = pin(b, 0, false);
        cur = v;
        out = !v && (_prevIn[b] & 1);
        break;
    }
    case BT_SR:
    {
        const bool s = pin(b, 0, false);
        const bool r = pin(b, 1, false);
        if (r)
        {
            out = false;
        }
        else if (s)
        {
            out = true;
        }
        break;
    }
    case BT_IMP:
    {
        const bool trg = pin(b, 0, false);
        const bool s = pin(b, 1, false);
        const bool r = pin(b, 2, false);
        cur = trg;
        if (r)
        {
            out = false;
        }
        else if (s)
        {
            out = true;
        }
        else if (trg && !(_prevIn[b] & 1))
        {
            out = !out;
        }
        break;
    }
    case BT_TON:
    {
        const uint32_t t = timeMs(blk.p1, base);
        if (pin(b, 0, false))
        {
            if (!_timing[b] && !out)
            {
                _timing[b] = true;
                _t0[b] = now;
            }
            if (_timing[b] && now - _t0[b] >= t)
            {
                _timing[b] = false;
                out = true;
            }
        }
        else
        {
            _timing[b] = false;
            out = false;
        }
        break;
    }
    case BT_TOF:
    {
        const uint32_t t = timeMs(blk.p1, base);
        if (pin(b, 1, false))
        {
            out = false;
            _timing[b] = false;
        }
        else if (pin(b, 0, false))
        {
            out = true;
            _timing[b] = false;
        }
        else if (out)
        {
            if (!_timing[b])
            {
                _timing[b] = true;
                _t0[b] = now;
            }
            if (now - _t0[b] >= t)
            {
                _timing[b] = false;
                out = false;
            }
        }
        break;
    }
    case BT_TP:
    {
        const uint32_t t = timeMs(blk.p1, base);
        const bool trg = pin(b, 0, false);
        cur = trg;
        if (pin(b, 1, false))
        {
            out = false;
            _timing[b] = false;
        }
        else
        {
            if (trg && !(_prevIn[b] & 1) && !_timing[b])
            {
                _timing[b] = true;
                _t0[b] = now;
                out = true;
            }
            if (_timing[b] && now - _t0[b] >= t)
            {
                _timing[b] = false;
                out = false;
            }
        }
        break;
    }
    case BT_TONR:
    {
        const uint32_t t = timeMs(blk.p1, base);
        if (pin(b, 1, false))
        {
            if (_acc[b] != 0 || out)
            {
                _retDirty = true;
            }
            _acc[b] = 0;
            _timing[b] = false;
            out = false;
        }
        else if (pin(b, 0, false))
        {
            if (_timing[b])
            {
                const uint32_t add = now - _t0[b];
                if (_acc[b] < t)
                {
                    _acc[b] = (_acc[b] + add > t) ? t : _acc[b] + add;
                    _retDirty = true;
                }
            }
            _timing[b] = true;
            _t0[b] = now;
            out = _acc[b] >= t;
        }
        else
        {
            _timing[b] = false;
            out = _acc[b] >= t; // Q se mantiene hasta R
        }
        break;
    }
    case BT_GEN:
    {
        const bool en = pin(b, 0, false);
        cur = en;
        if (!en)
        {
            out = false;
        }
        else
        {
            if (!(_prevIn[b] & 1))
            {
                out = true;
                _t0[b] = now;
            }
            const uint32_t th = timeMs(blk.p1, base);
            const uint32_t tl = timeMs(blk.p2, base);
            const uint32_t span = out ? th : tl;
            if (span > 0 && now - _t0[b] >= span)
            {
                out = !out;
                _t0[b] += span;
                if (now - _t0[b] >= span) // atraso grande: se resincroniza
                {
                    _t0[b] = now;
                }
            }
        }
        break;
    }
    case BT_CTUD:
    {
        const bool r = pin(b, 0, false);
        const bool cnt = pin(b, 1, false);
        const bool dir = pin(b, 2, false);
        cur = cnt;
        if (r)
        {
            if (_cnt[b] != 0)
            {
                _retDirty = true;
            }
            _cnt[b] = 0;
        }
        else if (cnt && !(_prevIn[b] & 1))
        {
            _cnt[b] += dir ? -1 : 1;
            if (_cnt[b] > 999999)
            {
                _cnt[b] = 999999;
            }
            if (_cnt[b] < -99999)
            {
                _cnt[b] = -99999;
            }
            _retDirty = true;
        }
        out = _cnt[b] >= blk.p1;
        break;
    }
    case BT_WEEK:
    {
        out = false;
        if (clock.valid && blk.p1 != blk.p2)
        {
            const uint8_t today = clock.weekday;
            const uint8_t yesterday = (uint8_t)((today + 6) % 7);
            const bool dToday = (blk.aux >> today) & 1;
            const bool dYest = (blk.aux >> yesterday) & 1;
            const int32_t m = clock.minute;
            if (blk.p1 < blk.p2)
            {
                out = dToday && m >= blk.p1 && m < blk.p2;
            }
            else // pasa la medianoche
            {
                out = (dToday && m >= blk.p1) || (dYest && m < blk.p2);
            }
        }
        break;
    }
    case BT_ALARM:
        out = pin(b, 0, false);
        if (out)
        {
            _alarms |= (1ULL << b);
        }
        else
        {
            _alarms &= ~(1ULL << b);
        }
        break;
    default:
        out = false;
        break;
    }

    if ((blk.flags & BF_RETAIN) && (blk.type == BT_SR || blk.type == BT_IMP) && out != wasOut)
    {
        _retDirty = true;
    }
    _prevIn[b] = cur;
    _out[b] = out;
}

void Engine::scan(uint8_t inputs, uint32_t nowMs, const ClockNow &clock)
{
    _in = inputs;
    for (uint8_t i = 0; i < _orderCount; i++)
    {
        evalBlock(_order[i], nowMs, clock);
    }
    uint8_t q = 0;
    for (uint8_t i = 0; i < NUM_Q; i++)
    {
        bool v = _p.q[i] != SRC_OPEN && readSrc(_p.q[i]);
        if (_p.q[i] != SRC_OPEN && ((_p.qNeg >> i) & 1))
        {
            v = !v;
        }
        q |= (uint8_t)(v << i);
    }
    uint8_t m = 0;
    for (uint8_t i = 0; i < NUM_M; i++)
    {
        bool v = _p.m[i] != SRC_OPEN && readSrc(_p.m[i]);
        if (_p.m[i] != SRC_OPEN && ((_p.mNeg >> i) & 1))
        {
            v = !v;
        }
        m |= (uint8_t)(v << i);
    }
    for (uint8_t x = 0; x < NUM_X; x++)
    {
        uint8_t bits = 0;
        for (uint8_t i = 0; i < X_IO; i++)
        {
            const uint8_t s = _p.xq[x][i];
            bool v = s != SRC_OPEN && readSrc(s);
            if (s != SRC_OPEN && ((_p.xqNeg[x] >> i) & 1))
            {
                v = !v;
            }
            bits |= (uint8_t)(v << i);
        }
        _xq[x] = bits;
    }
    _q = q;
    _m = m;
}

bool Engine::outputValue(uint8_t o) const
{
    if (o < NUM_Q)
    {
        return (_q >> o) & 1;
    }
    return o < NUM_OUT && ((_xq[(o - NUM_Q) / X_IO] >> ((o - NUM_Q) % X_IO)) & 1);
}

uint32_t Engine::elapsedMs(uint8_t b, uint32_t nowMs) const
{
    if (b >= _p.count)
    {
        return 0;
    }
    const Block &blk = _p.blocks[b];
    const uint8_t base = (uint8_t)((blk.flags & BF_BASE_MASK) >> BF_BASE_SHIFT);
    const uint32_t t = timeMs(blk.p1, base);
    switch (blk.type)
    {
    case BT_TON:
        if (_out[b])
        {
            return t;
        }
        return _timing[b] ? nowMs - _t0[b] : 0;
    case BT_TOF:
    case BT_TP:
        return _timing[b] ? nowMs - _t0[b] : 0;
    case BT_TONR:
        return _acc[b];
    case BT_GEN:
        return pinValue(b, 0) ? nowMs - _t0[b] : 0;
    default:
        return 0;
    }
}

uint32_t Engine::retained(uint8_t b) const
{
    if (b >= _p.count)
    {
        return 0;
    }
    switch (_p.blocks[b].type)
    {
    case BT_SR:
    case BT_IMP:
        return _out[b] ? 1 : 0;
    case BT_TONR:
        return _acc[b];
    case BT_CTUD:
        return (uint32_t)_cnt[b];
    default:
        return 0;
    }
}

void Engine::setRetained(uint8_t b, uint32_t value)
{
    if (b >= _p.count || !(_p.blocks[b].flags & BF_RETAIN))
    {
        return;
    }
    switch (_p.blocks[b].type)
    {
    case BT_SR:
    case BT_IMP:
        _out[b] = value != 0;
        break;
    case BT_TONR:
        _acc[b] = value;
        break;
    case BT_CTUD:
        _cnt[b] = (int32_t)value;
        break;
    default:
        break;
    }
}

void Engine::setParam(uint8_t b, int32_t p1, int32_t p2, uint8_t aux)
{
    if (b >= _p.count)
    {
        return;
    }
    _p.blocks[b].p1 = p1;
    _p.blocks[b].p2 = p2;
    _p.blocks[b].aux = aux;
}

} // namespace jwrt

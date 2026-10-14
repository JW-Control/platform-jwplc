// Alpha13 G10-P1G-C. No forma parte de JWPLC/2.1.0/libraries.
// Compilación host usa stubs de FreeRTOS y jwplc_spi_bus.
#include "JWPLC_W5500_SafeAccess.h"
#include "jwplc_spi_bus.h"
#if defined(ARDUINO) || defined(ESP_PLATFORM)
// ESP-IDF requiere inicializar los tipos/configuracion de FreeRTOS antes
// de task.h. En el harness host se usan los stubs de task.h aislados.
#include "freertos/FreeRTOS.h"
#endif
#include "freertos/task.h"
#include <atomic>

namespace jwplc_w5500_candidate {
namespace {
constexpr uint8_t kSockets = 8;
std::atomic<TaskHandle_t> g_owner{nullptr};
uint32_t g_depth = 0; // solo el task propietario lo lee/escribe.
uint32_t g_epoch = 1;
uint32_t g_allocation = 0;
uint32_t g_autoOwner = 0;
// A 32-bit monotonically increasing allocation lets a producer publish a
// close request even when another task holds SPI. Never wrap this counter.
std::atomic<uint32_t> g_pendingClose[kSockets] = {};

struct Slot {
    uint32_t epoch = 0;
    uint32_t allocation = 0;
    uint32_t owner = 0;
    uint16_t listenerPort = 0;
    bool valid = false;
};
Slot g_slots[kSockets] = {};

void clearSlot(Slot& s) { s = Slot{}; }

void incrementEpochLocked() {
    ++g_epoch;
    if (g_epoch == 0) g_epoch = 1;
    for (auto& s : g_slots) clearSlot(s);
}

bool same(const SocketToken& t, const Slot& s) {
    return bool(t) && s.valid && t.chipEpoch == s.epoch &&
           t.allocation == s.allocation && t.ownerCookie == s.owner;
}
}

bool ownedByThisTask() {
    TaskHandle_t me = xTaskGetCurrentTaskHandle();
    return me != nullptr && g_owner.load(std::memory_order_acquire) == me;
}

bool enter(uint32_t timeoutMs) {
    TaskHandle_t me = xTaskGetCurrentTaskHandle();
    if (me == nullptr) return false;
    if (g_owner.load(std::memory_order_acquire) == me) {
        ++g_depth;
        return true;
    }
    // No permitir que un scope W5500 "herede" sin control un mutex
    // tomado por otra operación (ej. SD/FRAM) en el mismo task.
    if (!jwplcSPI_acquire(timeoutMs)) return false;
    jwplcSPI_deselectAll();
    g_depth = 1;
    g_owner.store(me, std::memory_order_release);
    return true;
}

void leave() {
    if (!ownedByThisTask() || g_depth == 0) return;
    if (--g_depth == 0) {
        g_owner.store(nullptr, std::memory_order_release);
        jwplcSPI_release();
    }
}

uint32_t currentEpochLocked() {
    return ownedByThisTask() ? g_epoch : 0;
}

SocketToken allocateSocketLocked(uint8_t socket, uint32_t ownerCookie) {
    if (!ownedByThisTask() || socket >= kSockets || ownerCookie == 0)
        return {};
    if (g_allocation == UINT32_MAX) return {}; // fail closed before reuse
    ++g_allocation;
    Slot& s = g_slots[socket];
    s.epoch = g_epoch;
    s.allocation = g_allocation;
    s.owner = ownerCookie;
    s.listenerPort = 0;
    s.valid = true;
    SocketToken t;
    t.chipEpoch = s.epoch;
    t.allocation = s.allocation;
    t.ownerCookie = s.owner;
    t.socketIndex = socket;
    return t;
}

SocketToken allocateSocketLockedAuto(uint8_t socket) {
    if (!ownedByThisTask() || socket >= kSockets) return {};
    ++g_autoOwner;
    if (g_autoOwner == 0) {
        // Evitar cookie reciclada tras overflow de 32 bits.
        incrementEpochLocked();
        g_autoOwner = 1;
    }
    return allocateSocketLocked(socket, g_autoOwner);
}

SocketToken currentSocketTokenLocked(uint8_t socket) {
    if (!ownedByThisTask() || socket >= kSockets) return {};
    const Slot& slot = g_slots[socket];
    if (!slot.valid) return {};
    SocketToken token;
    token.chipEpoch = slot.epoch;
    token.allocation = slot.allocation;
    token.ownerCookie = slot.owner;
    token.socketIndex = socket;
    return token;
}

bool matchesLocked(const SocketToken& token) {
    return ownedByThisTask() && token.socketIndex < kSockets &&
           same(token, g_slots[token.socketIndex]);
}

bool invalidateSocketLocked(const SocketToken& token) {
    if (!matchesLocked(token)) return false;
    clearSlot(g_slots[token.socketIndex]);
    return true;
}

void mapListenerLocked(uint8_t socket, uint16_t port,
                       const SocketToken& token) {
    if (!matchesLocked(token) || socket != token.socketIndex) return;
    g_slots[socket].listenerPort = port;
}

bool listenerMatchesLocked(uint8_t socket, uint16_t port,
                           const SocketToken& token) {
    return socket == token.socketIndex && matchesLocked(token) &&
           port != 0 && g_slots[socket].listenerPort == port;
}

StatusReadOutcome readStatusIfCurrent(
    const SocketToken& token, SocketStatusReadFn read,
    void* context, uint32_t timeoutMs, uint8_t* statusOut) {
    BusScope bus(timeoutMs);
    if (!bus) return StatusReadOutcome::BusBusy;
    if (!matchesLocked(token) || read == nullptr || statusOut == nullptr)
        return StatusReadOutcome::Stale;
    // No write to statusOut until the source returned an actual value.
    const uint8_t value = read(token.socketIndex, context);
    *statusOut = value;
    return StatusReadOutcome::Read;
}

CloseOutcome closeIfCurrent(const SocketToken& token, SocketCloseFn close,
                            void* context, uint32_t timeoutMs) {
    BusScope bus(timeoutMs);
    if (!bus) return CloseOutcome::BusBusy; // NO abandonar handle.
    if (!matchesLocked(token)) return CloseOutcome::Stale;
    if (close == nullptr) return CloseOutcome::Stale;
    // Atomicidad: comprobación y CLOSE están bajo el MISMO lock.
    close(token.socketIndex, context);
    clearSlot(g_slots[token.socketIndex]);
    return CloseOutcome::Closed;
}

bool requestDeferredClose(const SocketToken& token) {
    if (!bool(token)) return false;
    auto& pending = g_pendingClose[token.socketIndex];
    uint32_t observed = pending.load(std::memory_order_acquire);
    while (observed < token.allocation &&
           !pending.compare_exchange_weak(observed, token.allocation,
                                          std::memory_order_acq_rel)) {}
    // An older owner may publish late, but may never displace a newer ticket.
    return true;
}

uint8_t serviceDeferredClose(SocketCloseFn close, void* context,
                             uint32_t timeoutMs) {
    if (close == nullptr) return 0;
    BusScope bus(timeoutMs);
    if (!bus) return 0;
    uint8_t drained = 0;
    for (uint8_t index = 0; index < kSockets; ++index) {
        auto& pending = g_pendingClose[index];
        const uint32_t ticket = pending.load(std::memory_order_acquire);
        if (ticket == 0) continue;
        Slot& slot = g_slots[index];
        if (slot.valid && slot.allocation == ticket) {
            close(index, context);
            clearSlot(slot);
            ++drained;
        }
        uint32_t expected = ticket;
        (void)pending.compare_exchange_strong(expected, 0,
                                               std::memory_order_acq_rel);
    }
    return drained;
}

bool resetAndInvalidate(ResetFn reset, void* context, uint32_t timeoutMs) {
    BusScope bus(timeoutMs);
    if (!bus || reset == nullptr) return false;
    // Generación cambia solo con reset verdadero solicitado; NOT_READY
    // y LINK_OFF no llaman a esta función.
    incrementEpochLocked();
    return reset(context); // excluido hasta terminar GPIO/espera.
}
} // namespace jwplc_w5500_candidate

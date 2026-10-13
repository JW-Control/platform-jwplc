#pragma once
// Alpha13 G10-P1G-C: prototipo aislado, NO incluido en el package activo.
// Coordina ownership de bus compartido y leases de sockets W5500.
#include <stdint.h>

namespace jwplc_w5500_candidate {

bool enter(uint32_t timeoutMs);
void leave();
bool ownedByThisTask();

class BusScope final {
public:
    explicit BusScope(uint32_t timeoutMs) : valid_(enter(timeoutMs)) {}
    ~BusScope() { if (valid_) leave(); }
    BusScope(const BusScope&) = delete;
    BusScope& operator=(const BusScope&) = delete;
    explicit operator bool() const { return valid_; }
private:
    bool valid_;
};

struct SocketToken final {
    uint32_t chipEpoch = 0;
    uint32_t allocation = 0;
    uint32_t ownerCookie = 0;
    uint8_t socketIndex = 255;
    explicit operator bool() const {
        return chipEpoch != 0 && allocation != 0 &&
               ownerCookie != 0 && socketIndex < 8;
    }
};

enum class CloseOutcome : uint8_t {
    Closed = 1,
    BusBusy = 2,
    Stale = 3
};

// Todas las funciones salvo closeIfCurrent/resetAndInvalidate exigen
// estar DENTRO del scope de bus. La operación física y el check se
// realizan juntos; la API pública Arduino aún no está conectada aquí.
SocketToken allocateSocketLocked(uint8_t socket, uint32_t ownerCookie);
bool matchesLocked(const SocketToken& token);
void mapListenerLocked(uint8_t socket, uint16_t port, const SocketToken& token);
bool listenerMatchesLocked(uint8_t socket, uint16_t port,
                           const SocketToken& token);
uint32_t currentEpochLocked();

using SocketCloseFn = void (*)(uint8_t socket, void* context);
CloseOutcome closeIfCurrent(const SocketToken& token, SocketCloseFn close,
                            void* context, uint32_t timeoutMs);

// Variante B de prototipo: exclusión SPI completa durante reset.
// Mantiene el mutex mientras se ejecuta el callback; no promover a
// producto hasta medir fairness SD/FRAM/Display durante ~90ms.
using ResetFn = bool (*)(void* context);
bool resetAndInvalidate(ResetFn reset, void* context, uint32_t timeoutMs);

} // namespace jwplc_w5500_candidate

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

// G10-I: lectura sin ambigüedad. NO convertir contención a SnSR::CLOSED.
// Esta API es solo de la candidata, no modifica interfaces Arduino.
enum class StatusReadOutcome : uint8_t {
    Read = 1,
    BusBusy = 2,
    Stale = 3
};

using SocketStatusReadFn = uint8_t (*)(uint8_t socket, void* context);

// La operación de status y la validación de owner suceden en un mismo
// ámbito de SPI, incluso si el callback usa el scope W5500 anidable.
// statusOut permanece intacto si no hubo lectura real.
StatusReadOutcome readStatusIfCurrent(
    const SocketToken& token, SocketStatusReadFn read,
    void* context, uint32_t timeoutMs, uint8_t* statusOut);

// Todas las funciones salvo closeIfCurrent/resetAndInvalidate exigen
// estar DENTRO del scope de bus. La operación física y el check se
// realizan juntos; la API pública Arduino aún no está conectada aquí.
SocketToken allocateSocketLocked(uint8_t socket, uint32_t ownerCookie);
// S2A: ticket único al abrir un socket nuevo; no se entrega a usuarios todavía.
SocketToken allocateSocketLockedAuto(uint8_t socket);
// Captura válida SOLO con el bus W5500 adquirido, para asociar handles futuros.
SocketToken currentSocketTokenLocked(uint8_t socket);
bool matchesLocked(const SocketToken& token);
// El propietario invalida su token tras CLOSE bajo la MISMA exclusión SPI.
bool invalidateSocketLocked(const SocketToken& token);
void mapListenerLocked(uint8_t socket, uint16_t port, const SocketToken& token);
bool listenerMatchesLocked(uint8_t socket, uint16_t port,
                           const SocketToken& token);
uint32_t currentEpochLocked();

using SocketCloseFn = void (*)(uint8_t socket, void* context);
CloseOutcome closeIfCurrent(const SocketToken& token, SocketCloseFn close,
                            void* context, uint32_t timeoutMs);

// A stop that cannot acquire SPI publishes its allocation without keeping the
// UDP object alive. Allocation numbers never repeat; a late stale publisher
// cannot replace a newer owner's request. The caller retains its own token
// until it observes CLOSED or STALE.
bool requestDeferredClose(const SocketToken& token);
// One bounded cooperative pass. May be called after the UDP/DNS/DHCP object
// has gone away. The callback executes only under the bus and current lease.
uint8_t serviceDeferredClose(SocketCloseFn close, void* context,
                             uint32_t timeoutMs);

// Variante B de prototipo: exclusión SPI completa durante reset.
// Mantiene el mutex mientras se ejecuta el callback; no promover a
// producto hasta medir fairness SD/FRAM/Display durante ~90ms.
using ResetFn = bool (*)(void* context);
bool resetAndInvalidate(ResetFn reset, void* context, uint32_t timeoutMs);

} // namespace jwplc_w5500_candidate

// Candidato P1G-C en host: ejecuta el C++ EXACTO de SafeAccess.cpp.
// NO prueba integración real con EthernetClient/Server ni latencias físicas.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace jwplc_w5500_candidate;
static int g_task = 1;
static int g_spi_owner = 0;
static int g_acquires = 0;
static int g_releases = 0;
static int g_deselects = 0;
static int g_hw_owners[8] = {};
static int g_closes = 0;
static int g_wrong_owner = 0;
static void require(bool yes, const char* msg) {
    if (!yes) throw std::runtime_error(msg);
}

// Stub host de la interfaz pública REAL de jwplc_spi_bus.
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if (g_spi_owner != 0) return false; // mutex NO recursivo.
    g_spi_owner = g_task;
    ++g_acquires;
    return true;
}
extern "C" void jwplcSPI_release(void) {
    require(g_spi_owner == g_task, "RELEASE_BY_OTHER_TASK");
    g_spi_owner = 0;
    ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) { ++g_deselects; }
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}

struct CloseContext { int expected; };
static void closeHardware(uint8_t idx, void* ctx) {
    auto* c = static_cast<CloseContext*>(ctx);
    ++g_closes;
    if (g_hw_owners[idx] != c->expected) ++g_wrong_owner;
    g_hw_owners[idx] = 0;
}
static bool resetHardware(void*) {
    require(g_spi_owner == g_task, "RESET_OUTSIDE_SPI_EXCLUSION");
    g_task = 2;
    require(!enter(0), "SECOND_TASK_ENTER_DURING_RESET");
    g_task = 1;
    return true;
}

static void testBusNestedAndContention() {
    require(enter(20), "INITIAL_BUS_LOCK");
    require(enter(20), "NESTED_BUS_LOCK");
    require(g_acquires == 1 && g_deselects == 1, "NESTED_LOCK_DOUBLE_ACQUIRE");
    g_task = 2;
    require(!enter(0), "ANOTHER_TASK_CAN_ENTER_WHILE_HELD");
    g_task = 1;
    leave();
    require(g_spi_owner == 1 && g_releases == 0, "EARLY_RELEASE_NESTED");
    leave();
    require(g_spi_owner == 0 && g_releases == 1, "LAST_RELEASE_MISSING");

    // Un dueño SD/FRAM no se debe tratar como dueño W5500.
    require(jwplcSPI_acquire(0), "FOREIGN_SHARED_BUS_OWNER_SETUP");
    require(!enter(0), "FOREIGN_BUS_OWNER_MISTAKEN_FOR_W5500");
    require(g_spi_owner == 1, "FOREIGN_SPI_LOCK_RELEASED");
    jwplcSPI_release();

    g_task = 0;
    require(!enter(0), "NULL_TASK_ACCEPTED");
    g_task = 1;
    std::cout << "C1_BUS_NESTING_CONTENTION=PASS\n";
}

static void testOwnershipAndReset() {
    SocketToken old, current;
    {
        BusScope guard(20);
        require(bool(guard), "OWNER_SETUP_LOCK");
        old = allocateSocketLocked(2, 101);
        require(bool(old) && matchesLocked(old), "OLD_LEASE_NOT_VALID");
        mapListenerLocked(2, 502, old);
        require(listenerMatchesLocked(2, 502, old), "LISTENER_MAP_NOT_SET");
        current = allocateSocketLocked(2, 202); // índice reciclado sin reset
        require(bool(current) && matchesLocked(current), "NEW_LEASE_INVALID");
        require(!matchesLocked(old), "STALE_SAME_EPOCH_ACCEPTED");
        require(!listenerMatchesLocked(2, 502, old), "OLD_LISTENER_ACCEPTED");
    }
    g_hw_owners[2] = 202;
    CloseContext oldOwner{101}, newOwner{202};
    require(closeIfCurrent(old, closeHardware, &oldOwner, 20) ==
                CloseOutcome::Stale, "STALE_CLOSE_NOT_REJECTED");
    require(g_hw_owners[2] == 202 && g_closes == 0,
            "STALE_CLOSE_REACHED_HARDWARE");
    // Mutex temporalmente ocupado por otro task: no cerrar ni borrar lease.
    g_task = 2;
    require(jwplcSPI_acquire(0), "BUS_OCCUPY_PRECONDITION");
    g_task = 1;
    require(closeIfCurrent(current, closeHardware, &newOwner, 0) ==
                CloseOutcome::BusBusy, "SPI_DENIAL_NOT_RETRIABLE");
    require(g_hw_owners[2] == 202 && g_closes == 0,
            "BUS_DENIAL_CLOSED_HARDWARE");
    g_task = 2;
    jwplcSPI_release();
    g_task = 1;
    require(closeIfCurrent(current, closeHardware, &newOwner, 20) ==
                CloseOutcome::Closed, "CURRENT_LEASE_NOT_CLOSED");
    require(g_closes == 1 && g_wrong_owner == 0,
            "WRONG_OWNER_CLOSE_AFTER_RECOVERY");

    uint32_t beforeEpoch = 0;
    {
        BusScope guard(20);
        require(bool(guard), "RESET_BASELINE_LOCK");
        current = allocateSocketLocked(3, 301);
        mapListenerLocked(3, 1502, current);
        beforeEpoch = currentEpochLocked();
    }
    g_hw_owners[3] = 301;
    // LINK_OFF / !isReady() sin reset: ningún cambio de generación.
    {
        BusScope guard(20);
        require(bool(guard) && currentEpochLocked() == beforeEpoch &&
                matchesLocked(current), "LINK_OFF_NOT_A_REAL_RESET");
    }
    require(resetAndInvalidate(resetHardware, nullptr, 20),
            "PROTECTED_RESET_FAILED");
    {
        BusScope guard(20);
        require(bool(guard) && currentEpochLocked() != beforeEpoch,
                "RESET_DID_NOT_INCREASE_EPOCH");
        require(!matchesLocked(current), "PRE_RESET_SOCKET_STILL_VALID");
        require(!listenerMatchesLocked(3, 1502, current),
                "PRE_RESET_LISTENER_STILL_VALID");
    }
    require(closeIfCurrent(current, closeHardware, &newOwner, 20) ==
                CloseOutcome::Stale, "PRE_RESET_DEFERRED_CLOSE_ALLOWED");
    require(g_wrong_owner == 0 && g_closes == 1,
            "WRONG_OWNER_CLOSE_AFTER_RESET");
    std::cout << "C1_SOCKET_EPOCH_OWNER_LISTENER=PASS\n";
    std::cout << "C1_RESET_INTERLOCK_AND_LINKOFF=PASS\n";
}

int main() {
    try {
        testBusNestedAndContention();
        testOwnershipAndReset();
        std::cout << "C1_NATIVE_RESULT=PASS_PROTOTYPE_ONLY_NO_PRODUCT_ADOPTION\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "C1_NATIVE_FAILURE=" << e.what() << "\n";
        return 3;
    }
}

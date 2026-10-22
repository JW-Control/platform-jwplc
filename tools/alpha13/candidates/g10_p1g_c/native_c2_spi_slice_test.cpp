// G10-P1G-C2: harness nativo que enlaza los cuerpos C++ extraidos de
// FUENTES PRODUCTIVAS convertidas en overlay, no una reescritura semantica.
// Alcance: recorrido runtime/Modbus -> Client.connected -> socketStatus.
// No demuestra seguridad general de todos los metodos TCP/UDP ni ownership.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

#define JWPLC_HAS_ETHERNET 1
#define JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS 0
#define MAX_SOCK_NUM 8
static void check(bool valid, const char* message) {
    if (!valid) throw std::runtime_error(message);
}
static int g_task=1;
static int g_spi_owner=0;
static int g_acquires=0;
static int g_releases=0;
static int g_spi_transactions=0;
static int g_bad_access=0;
static uint8_t g_status=0x17; // ESTABLISHED

extern "C" bool jwplcSPI_acquire(uint32_t) {
    if (g_spi_owner != 0) return false; // xSemaphoreCreateMutex NO recursivo
    g_spi_owner=g_task;
    ++g_acquires;
    return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_spi_owner==g_task,"MUTEX_RELEASE_BY_NONOWNER");
    g_spi_owner=0;
    ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_spi_owner==g_task,"DESELECT_OUTSIDE_SPI_MUTEX");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}

struct SnSR {
    static constexpr uint8_t CLOSED=0x00, LISTEN=0x14;
    static constexpr uint8_t ESTABLISHED=0x17, FIN_WAIT=0x18;
    static constexpr uint8_t CLOSE_WAIT=0x1C;
};
constexpr int SPI_ETHERNET_SETTINGS=26;
struct MockSPI {
    void beginTransaction(int) {
        if (g_spi_owner!=g_task) {
            ++g_bad_access;
            throw std::runtime_error("SOCKET_STATUS_OUTSIDE_MUTEX");
        }
        ++g_spi_transactions;
    }
    void endTransaction() {
        check(g_spi_owner==g_task,"SPI_END_OUTSIDE_MUTEX");
    }
};
static MockSPI SPI;
struct MockW5100 {
    uint8_t readSnSR(uint8_t) { return g_status; }
};
static MockW5100 W5100;

class EthernetClass {
public:
    uint8_t socketStatus(uint8_t);
};
static EthernetClass Ethernet;
class EthernetClient {
public:
    uint8_t _sockindex=2;
    int available() {
        if (g_spi_owner!=g_task) {
            ++g_bad_access;
            throw std::runtime_error("AVAILABLE_OUTSIDE_MUTEX");
        }
        return 0;
    }
    uint8_t connected();
};
struct Stats { int busLockTimeouts=0; };
constexpr int JWPLC_MODBUS_TCP_BUS_LOCK_TIMEOUT=11;
constexpr int JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT=12;
class JWPLC_ModbusTCPClientClass {
public:
    Stats _stats;
    int _result=0;
    bool acquireBus(uint32_t);
    void releaseBus();
};
class JWPLC_ModbusTCPClass {
public:
    Stats _stats;
    int _lastError=0;
    void setError(int error) { _lastError=error; }
    bool acquireBus(uint32_t);
    void releaseBus();
};
class JWPLC_EthernetClass {
public:
    bool acquireBus(uint32_t);
    void releaseBus();
};

// Compilar .cpp generado mediante extraccion EXACTA de firmas originales.
#include "C2_REAL_PATCHED_METHODS.inc"

static void testDirectAndNested() {
    EthernetClient raw;
    g_status=SnSR::ESTABLISHED;
    const int before=g_acquires;
    check(raw.connected()==1,"RAW_CLIENT_CONNECTED_FAILED");
    check(g_acquires==before+1 && g_spi_owner==0,
          "RAW_CLIENT_MUST_ACQUIRE_AND_RELEASE_ONCE");

    g_status=SnSR::CLOSE_WAIT;
    check(raw.connected()==0,"CLOSE_WAIT_SEMANTICS_CHANGED");
    check(g_bad_access==0 && g_spi_owner==0,
          "CLOSE_WAIT_AVAILABLE_NOT_PROTECTED");

    JWPLC_ModbusTCPClientClass mbc;
    g_status=SnSR::ESTABLISHED;
    int acq=g_acquires, rel=g_releases;
    check(mbc.acquireBus(20),"MB_CLIENT_LOCK_FAILED");
    check(raw.connected()==1,"NESTED_CLIENT_DEADLOCK_OR_FALSE_DISCONNECT");
    mbc.releaseBus();
    check(g_acquires==acq+1 && g_releases==rel+1 &&
          !jwplc_w5500_candidate::ownedByThisTask(),
          "MB_CLIENT_DOUBLE_TAKE_OR_OWNER_LEAK");

    JWPLC_ModbusTCPClass mbs;
    acq=g_acquires; rel=g_releases;
    check(mbs.acquireBus(20),"MB_SERVER_LOCK_FAILED");
    check(raw.connected()==1,"MB_SERVER_NESTED_CLIENT_FAILED");
    mbs.releaseBus();
    check(g_acquires==acq+1 && g_releases==rel+1,
          "MB_SERVER_DOUBLE_TAKE_OR_OWNER_LEAK");

    JWPLC_EthernetClass runtime;
    acq=g_acquires; rel=g_releases;
    check(runtime.acquireBus(20),"RUNTIME_LOCK_FAILED");
    check(raw.connected()==1,"RUNTIME_NESTED_CLIENT_FAILED");
    runtime.releaseBus();
    check(g_acquires==acq+1 && g_releases==rel+1,
          "RUNTIME_DOUBLE_TAKE_OR_OWNER_LEAK");

    acq=g_acquires;
    check(Ethernet.socketStatus(2)==SnSR::ESTABLISHED,
          "DIRECT_SOCKET_STATUS_FAILED");
    check(g_acquires==acq+1,"DIRECT_SOCKET_STATUS_NO_LOCK");

    std::cout<<"C2_REAL_SOURCE_DIRECT_AND_NESTED_SPI=PASS\n";
}

static void testBusyDoesNotTransfer() {
    // Test limitado: no se acepta false disconnected como semantica final.
    // Confirma solo que lock BUSY no genera transferencia W5500.
    g_task=2;
    check(jwplcSPI_acquire(0),"BUSY_SETUP_FAILED");
    g_task=1;
    int before=g_spi_transactions;
    EthernetClient raw;
    check(raw.connected()==0,"BUSY_SHOULD_NOT_ACCESS_HARDWARE");
    check(g_spi_transactions==before && g_spi_owner==2,
          "BUSY_PERFORMED_UNPROTECTED_SPI");
    g_task=2;
    jwplcSPI_release();
    g_task=1;
    std::cout<<"C2_SPI_BUSY_NO_W5500_ACCESS=PASS_WITH_SEMANTIC_GAP\n";
}

int main() {
    try {
        testDirectAndNested();
        testBusyDoesNotTransfer();
        check(g_bad_access==0 && g_spi_owner==0,"END_STATE_DIRTY");
        std::cout<<"C2_NATIVE_RESULT=PASS_LIMITED_SLICE_ADOPTION_BLOCKED\n";
        return 0;
    } catch (const std::exception& err) {
        std::cerr<<"C2_NATIVE_FAILURE="<<err.what()<<"\n";
        return 3;
    }
}

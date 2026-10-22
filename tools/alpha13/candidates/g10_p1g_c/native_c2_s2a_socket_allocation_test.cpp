// G10-P1G-C2-S2A: usa el cuerpo REAL de socketBegin extraido de overlay.
// No integra ni acredita todavia Client/Server/UDP, DHCP, ownership al cerrar.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;

#define MAX_SOCK_NUM 8
#define JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS 0
static int g_task = 1;
static int g_busOwner = 0;
static int g_acquires = 0;
static int g_releases = 0;
static int g_spiTransactions = 0;
static int g_hardwareOpens = 0;
static int g_wrongCloses = 0;
static bool g_chipPresent = true;
static uint8_t g_socketStatus[MAX_SOCK_NUM] = {};
static void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if (g_busOwner) return false;
    g_busOwner = g_task;
    ++g_acquires;
    return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_busOwner == g_task, "SPI_RELEASE_NOT_OWNER");
    g_busOwner = 0;
    ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_busOwner == g_task, "DESELECT_WITHOUT_SPI_LOCK");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
struct SnSR {
    static constexpr uint8_t CLOSED = 0x00;
    static constexpr uint8_t ESTABLISHED = 0x17;
    static constexpr uint8_t LAST_ACK = 0x1D;
    static constexpr uint8_t TIME_WAIT = 0x1B;
    static constexpr uint8_t FIN_WAIT = 0x18;
    static constexpr uint8_t CLOSING = 0x1A;
};
struct SnMR { static constexpr uint8_t TCP = 0x01; };
static constexpr int Sock_CLOSE=0x10;
static constexpr int Sock_OPEN=0x01;
static constexpr int SPI_ETHERNET_SETTINGS=26;
static void delayMicroseconds(int) {}
static uint16_t local_port=49152;
struct socketstate_t {
    uint16_t RX_RSR=0,RX_RD=0,TX_FSR=0,RX_inc=0;
};
static socketstate_t state[MAX_SOCK_NUM];

struct MockSPI {
    void beginTransaction(int) {
        check(g_busOwner==g_task, "SPI_TRANSACTION_WITHOUT_LOCK");
        ++g_spiTransactions;
    }
    void endTransaction() { check(g_busOwner==g_task,"SPI_END_WITHOUT_LOCK"); }
};
static MockSPI SPI;
struct MockW5100 {
    int getChip() { return g_chipPresent ? 55 : 0; }
    uint8_t readSnSR(uint8_t s) { return g_socketStatus[s]; }
    void execCmdSn(uint8_t s, int command) {
        check(g_busOwner==g_task, "W5500_COMMAND_WITHOUT_LOCK");
        if (command==Sock_OPEN) {
            ++g_hardwareOpens;
            g_socketStatus[s]=SnSR::ESTABLISHED;
        }
        if (command==Sock_CLOSE) g_socketStatus[s]=SnSR::CLOSED;
    }
    void writeSnMR(uint8_t, uint8_t) { check(g_busOwner==g_task,"WRITE_MR_UNLOCKED"); }
    void writeSnIR(uint8_t, uint8_t) { check(g_busOwner==g_task,"WRITE_IR_UNLOCKED"); }
    void writeSnPORT(uint8_t, uint16_t) { check(g_busOwner==g_task,"WRITE_PORT_UNLOCKED"); }
    uint16_t readSnRX_RD(uint8_t) { check(g_busOwner==g_task,"READ_PTR_UNLOCKED"); return 0; }
};
static MockW5100 W5100;
struct EthernetServer { static uint16_t server_port[MAX_SOCK_NUM]; };
uint16_t EthernetServer::server_port[MAX_SOCK_NUM]={};
struct EthernetClass { uint8_t socketBegin(uint8_t protocol,uint16_t port); };
static EthernetClass Ethernet;

#include "C2_S2A_PATCHED_SOCKET_BEGIN.inc"

struct CloseContext { int* counter; };
static void hardwareClose(uint8_t,void* ctx) {
    ++*static_cast<CloseContext*>(ctx)->counter;
    ++g_wrongCloses;
}
static bool doReset(void*) {
    check(g_busOwner==g_task,"RESET_WITHOUT_EXCLUSION");
    return true;
}
int main() {
    try {
        const int base = g_acquires;
        const uint8_t first = Ethernet.socketBegin(SnMR::TCP,502);
        check(first==0 && g_acquires==base+1 && g_releases==g_acquires,
              "DIRECT_SOCKET_BEGIN_MUTEX_FAILURE");
        check(g_hardwareOpens==1,"SOCKET_OPEN_MISSING");
        SocketToken t1;
        {
            BusScope bus(20);
            t1=currentSocketTokenLocked(first);
            check(bool(t1) && matchesLocked(t1),"LEASE_NOT_ALLOCATED");
        }
        EthernetServer::server_port[first]=502;
        g_socketStatus[first]=SnSR::CLOSED;
        int originalAcq=g_acquires;
        uint8_t again=MAX_SOCK_NUM;
        {
            BusScope bus(20);
            again=Ethernet.socketBegin(SnMR::TCP,1502);
            check(g_acquires==originalAcq+1,
                  "NESTED_SOCKET_BEGIN_DOUBLE_SPI_ACQUIRE");
        }
        check(again==first && g_releases==g_acquires,
              "REUSE_OR_RELEASE_FAILED");
        check(EthernetServer::server_port[first]==0,
              "SERVER_MAP_NOT_INVALIDATED");
        SocketToken t2;
        {
            BusScope bus(20);
            t2=currentSocketTokenLocked(first);
            check(bool(t2) && matchesLocked(t2),
                  "REUSED_LEASE_NOT_ALLOCATED");
            check(!matchesLocked(t1),"STALE_SAME_EPOCH_LEASE_ACCEPTED");
            check(t2.allocation!=t1.allocation,"ALLOCATION_SERIAL_REUSED");
        }
        int wrong=0; CloseContext oldContext{&wrong};
        check(closeIfCurrent(t1,hardwareClose,&oldContext,20)==CloseOutcome::Stale &&
              wrong==0 && g_wrongCloses==0,"STALE_SOCKET_CLOSED_WRONG_OWNER");
        g_task=2;
        check(jwplcSPI_acquire(0),"BUS_CONTENTION_SETUP");
        g_task=1;
        int spiBefore=g_spiTransactions;
        check(Ethernet.socketBegin(SnMR::TCP,2000)==MAX_SOCK_NUM &&
              g_spiTransactions==spiBefore,
              "CONTENDED_SOCKET_BEGIN_TOUCHED_HARDWARE");
        g_task=2;
        jwplcSPI_release();
        g_task=1;
        check(resetAndInvalidate(doReset,nullptr,20),"RESET_NOT_EXCLUDED");
        {
            BusScope bus(20);
            check(!matchesLocked(t2) && !bool(currentSocketTokenLocked(first)),
                  "PRE_RESET_LEASE_NOT_STALE");
        }
        check(closeIfCurrent(t2,hardwareClose,&oldContext,20)==CloseOutcome::Stale &&
              wrong==0,"DEFERRED_CLOSE_AFTER_RESET_WRONG_OWNER");
        std::cout<<"C2_S2A_REAL_SOCKET_BEGIN_MUTEX_LEASE=PASS\n";
        std::cout<<"C2_S2A_SAME_INDEX_AND_RESET_STALE=PASS\n";
        std::cout<<"C2_S2A_BUSY_SKIP_NO_HARDWARE=PASS\n";
        std::cout<<"C2_S2A_NATIVE_RESULT=PASS_LIMITED_SOCKET_ALLOCATION_ADOPTION_BLOCKED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"C2_S2A_NATIVE_FAILURE="<<e.what()<<"\n";
        return 3;
    }
}

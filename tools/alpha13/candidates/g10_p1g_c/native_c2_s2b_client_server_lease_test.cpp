// C2-S2B1 native witness: REAL patched ctor, cancelStopAsync,
// EthernetServer::begin/available/accept; C1 lease core compiled, SPI mocked.
// No claim of full TCP transport/Arduino integration.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task = 1, g_busOwner = 0, g_acquires = 0, g_releases = 0;
static int g_close = 0, g_disconnect = 0, g_wrongClose = 0, g_cancels = 0;
static uint8_t g_status[MAX_SOCK_NUM] = {};
static int g_rx[MAX_SOCK_NUM] = {};
static int g_real_owner[MAX_SOCK_NUM] = {};
static uint32_t g_millis = 100;
static void check(bool condition,const char* why) {
    if (!condition) throw std::runtime_error(why);
}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if (g_busOwner) return false;
    g_busOwner = g_task;
    ++g_acquires;
    return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_busOwner == g_task, "SPI_RELEASE_WRONG_TASK");
    g_busOwner = 0;
    ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_busOwner == g_task,"SPI_DESELECT_UNLOCKED");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static uint32_t millis() { return g_millis; }
struct SnMR { static constexpr uint8_t TCP=1; };
struct SnSR {
    static constexpr uint8_t CLOSED=0x00, INIT=0x13, LISTEN=0x14;
    static constexpr uint8_t ESTABLISHED=0x17, CLOSE_WAIT=0x1C;
};
struct MockW5100 {
    uint8_t getChip() { return 55; }
};
static MockW5100 W5100;

class EthernetClient {
public:
    // Body extracted without re-authoring it from the overlay header.
    __REAL_CLIENT_CTOR__
    EthernetClient() : _sockindex(MAX_SOCK_NUM), _timeout(1000) {}
    void cancelStopAsync();
    int getSocketNumber() const { return _sockindex; }
    void cancelFlushAsync() { ++g_cancels; }
    void cancelWriteAsync() { ++g_cancels; }
    uint8_t _sockindex = MAX_SOCK_NUM;
    uint16_t _timeout = 1000;
    bool _stopPending = false;
    uint32_t _stopStartedAtMs = 0;
    uint32_t _startMillis = 0;
    SocketToken _jwplcSocketToken{};
};
class EthernetServer {
public:
    explicit EthernetServer(uint16_t port):_port(port){}
    void begin();
    EthernetClient available();
    EthernetClient accept();
    static uint16_t server_port[MAX_SOCK_NUM];
    uint16_t _port = 0;
};
uint16_t EthernetServer::server_port[MAX_SOCK_NUM] = {};

class EthernetClass {
public:
    // Mock only socketBegin; this function is independently source-backed
    // by C2-S2A. Ownership allocation is the same C1 code.
    uint8_t socketBegin(uint8_t, uint16_t) {
        check(ownedByThisTask(), "SOCKET_BEGIN_UNLOCKED");
        for (uint8_t i=0;i<MAX_SOCK_NUM;i++) if (g_status[i]==SnSR::CLOSED) {
            g_status[i]=SnSR::INIT;
            serverCleanup(i);
            auto token=allocateSocketLockedAuto(i);
            check(bool(token),"SOCKET_ALLOCATE_FAILED");
            g_real_owner[i]=int(token.ownerCookie);
            return i;
        }
        return MAX_SOCK_NUM;
    }
    static void serverCleanup(uint8_t idx) {
        EthernetServer::server_port[idx]=0;
    }
    int socketListen(uint8_t idx) {
        check(ownedByThisTask(),"LISTEN_UNLOCKED");
        if (g_status[idx]!=SnSR::INIT) return 0;
        g_status[idx]=SnSR::LISTEN;
        return 1;
    }
    uint8_t socketStatus(uint8_t idx) {
        check(ownedByThisTask(),"STATUS_UNLOCKED");
        return g_status[idx];
    }
    int socketRecvAvailable(uint8_t idx) {
        check(ownedByThisTask(),"RECV_AVAILABLE_UNLOCKED");
        return g_rx[idx];
    }
    void socketDisconnect(uint8_t idx) {
        check(ownedByThisTask(),"DISCONNECT_UNLOCKED");
        ++g_disconnect;
        g_status[idx]=SnSR::CLOSED;
    }
    void socketClose(uint8_t idx) {
        check(ownedByThisTask(),"CLOSE_UNLOCKED");
        if (g_real_owner[idx]!=0 &&
            !bool(currentSocketTokenLocked(idx))) ++g_wrongClose;
        ++g_close;
        g_status[idx]=SnSR::CLOSED;
    }
};
static EthernetClass Ethernet;

#include "C2_S2B1_REAL_PATCHED_METHODS.inc"

static void testServerAvailableAndCopies() {
    EthernetServer server(502);
    server.begin();
    check(EthernetServer::server_port[0]==502,"LISTENER_START_FAILED");
    g_status[0]=SnSR::ESTABLISHED;
    g_rx[0]=12;
    EthernetClient original=server.available();
    check(original.getSocketNumber()==0,"AVAILABLE_CLIENT_NOT_RETURNED");
    EthernetClient copy=original;
    check(copy.getSocketNumber()==0,"CLIENT_COPY_NOT_PRESERVED");
    g_status[0]=SnSR::CLOSED;
    {
        BusScope bus(20);
        check(bool(bus),"REUSE_BUS_FAILED");
        check(Ethernet.socketBegin(SnMR::TCP,2000)==0,"SOCKET_REUSE_FAILED");
    }
    const int oldCloses=g_close;
    original.cancelStopAsync();
    copy.cancelStopAsync();
    check(g_close==oldCloses && g_wrongClose==0,
          "STALE_COPY_CLOSED_RECYCLED_SOCKET");
    check(original.getSocketNumber()==MAX_SOCK_NUM &&
          copy.getSocketNumber()==MAX_SOCK_NUM,
          "STALE_COPIES_NOT_DISCARDED_SAFELY");
    std::cout<<"C2_S2B1_SERVER_AVAILABLE_COPY_STALE=PASS\n";
}

static void testServerAcceptAndContention() {
    EthernetServer server(1502);
    server.begin();
    uint8_t listening=MAX_SOCK_NUM;
    for (uint8_t i=0;i<MAX_SOCK_NUM;i++)
        if (EthernetServer::server_port[i]==1502) listening=i;
    check(listening<MAX_SOCK_NUM,"SECOND_LISTENER_FAILED");
    g_status[listening]=SnSR::ESTABLISHED;
    EthernetClient accepted=server.accept();
    check(accepted.getSocketNumber()==listening,"ACCEPT_DID_NOT_RETURN_CLIENT");
    check(EthernetServer::server_port[listening]==0,"ACCEPT_LISTENER_NOT_RELEASED");
    EthernetClient alias=accepted;

    // La contención SPI debe conservar el handle y NO cerrar el socket.
    g_task=2;
    check(jwplcSPI_acquire(0),"CONTENTION_SETUP_FAILED");
    g_task=1;
    const int before=g_close;
    alias.cancelStopAsync();
    check(alias.getSocketNumber()==listening && g_close==before,
          "BUS_BUSY_DISCARDED_HANDLE_OR_CLOSED");
    g_task=2;
    jwplcSPI_release();
    g_task=1;
    alias.cancelStopAsync();
    check(alias.getSocketNumber()==MAX_SOCK_NUM && g_close==before+1,
          "OWNER_CLOSE_AFTER_CONTENTION_FAILED");
    accepted.cancelStopAsync();
    check(g_close==before+1,"COPIED_HANDLE_CLOSED_TWICE");
    std::cout<<"C2_S2B1_ACCEPT_COPY_AND_SPI_BUSY=PASS\n";
}
static bool hwReset(void*) {
    check(ownedByThisTask(),"RESET_WITHOUT_SPI_LOCK");
    return true;
}
static void testResetStaleHandle() {
    EthernetServer server(8080);
    server.begin();
    uint8_t idx=MAX_SOCK_NUM;
    for (uint8_t i=0;i<MAX_SOCK_NUM;i++)
        if (EthernetServer::server_port[i]==8080) idx=i;
    check(idx<MAX_SOCK_NUM,"RESET_LISTENER_FAILED");
    g_status[idx]=SnSR::ESTABLISHED;
    EthernetClient before=server.accept();
    check(before.getSocketNumber()==idx,"RESET_BASELINE_ACCEPT_FAILED");
    const int closes=g_close;
    check(resetAndInvalidate(hwReset,nullptr,20),"RESET_FAILED");
    before.cancelStopAsync();
    check(g_close==closes && before.getSocketNumber()==MAX_SOCK_NUM,
          "STALE_RESET_HANDLE_REACHED_HARDWARE");
    std::cout<<"C2_S2B1_RESET_EPOCH_REJECTED_CLOSE=PASS\n";
}
int main() {
    try {
        testServerAvailableAndCopies();
        testServerAcceptAndContention();
        testResetStaleHandle();
        check(g_wrongClose==0 && g_busOwner==0 && g_acquires==g_releases,
              "END_STATE_LOCK_OR_OWNER_ERROR");
        std::cout<<"C2_S2B1_NATIVE_RESULT=PASS_LIMITED_CLIENT_SERVER_CLOSE_ADOPTION_BLOCKED\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<"C2_S2B1_NATIVE_FAILURE="<<e.what()<<"\n";
        return 3;
    }
}

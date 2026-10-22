// Alpha13 G10-I B04: source-backed ModbusTCP CLOSE session and retry.
// Harness compiles actual overlay method bodies against SafeAccess.cpp.
// No board, no autoload, no on-target timing, no product adoption.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_spiOwner=0,g_takes=0,g_releases=0,g_closed=0;
static uint8_t g_hwState[8]={};
static void check(bool c,const char* why){if(!c)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if (g_spiOwner!=0) return false;
    g_spiOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_spiOwner==g_task,"WRONG_TASK_RELEASE");
    g_spiOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_spiOwner==g_task,"DESELECT_NO_LOCK");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
struct EthernetClient {
    uint8_t _sockindex=MAX_SOCK_NUM;
    SocketToken _jwplcSocketToken{};
    explicit operator bool() const{return _sockindex<MAX_SOCK_NUM;}
    void cancelStopAsync() {
        if (!bool(*this)) return;
        auto fn=[](uint8_t socket,void*) {
            check(ownedByThisTask(),"CLOSE_NOT_GUARDED");
            check(g_hwState[socket]==1,"CLOSE_UNEXPECTED_HARDWARE_STATE");
            g_hwState[socket]=0;++g_closed;
        };
        auto state=closeIfCurrent(_jwplcSocketToken,fn,nullptr,20);
        if (state!=CloseOutcome::BusBusy) {
            _sockindex=MAX_SOCK_NUM;
            _jwplcSocketToken={};
        }
    }
};
struct MockTx {int resets=0;void reset(){++resets;}};
enum {
    JWPLC_MODBUS_TCP_CLIENT_IDLE=0,
    JWPLC_MODBUS_TCP_CLIENT_WAIT_ETHERNET=1,
    JWPLC_MODBUS_TCP_CLIENT_CONNECTING=2,
    JWPLC_MODBUS_TCP_CLIENT_SENDING=3,
    JWPLC_MODBUS_TCP_CLIENT_WAIT_RESPONSE=4
};
struct JWPLC_ModbusTCPClientClass {
    MockTx _asyncTx;
    EthernetClient _tcpClient;
    bool _jwplcClosePending=false;
    bool _sessionConnected=false,_connectStarted=false,_sendStarted=false;
    int _state=JWPLC_MODBUS_TCP_CLIENT_IDLE;
    int _rxLength=4;
    bool acquireBus(uint32_t ms){return enter(ms);}
    void releaseBus(){leave();}
    void resetRx(){_rxLength=0;}
    bool busy() const;
    void closeSession();
    bool serviceDeferredClose();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static SocketToken fresh(uint8_t slot) {
    BusScope bus(20);
    check(bool(bus),"ALLOCATE_GUARD");
    auto token=allocateSocketLockedAuto(slot);
    check(bool(token),"TOKEN_NOT_ISSUED");
    g_hwState[slot]=1;
    return token;
}
static void assign(JWPLC_ModbusTCPClientClass& x,uint8_t slot) {
    x._tcpClient._sockindex=slot;
    x._tcpClient._jwplcSocketToken=fresh(slot);
    x._sessionConnected=true;
    x._state=JWPLC_MODBUS_TCP_CLIENT_SENDING;
}
static void lockByOtherTask() {
    g_task=2;check(jwplcSPI_acquire(0),"BUSY_SETUP_FAILED");g_task=1;
}
static void releaseOtherTask(){
    g_task=2;jwplcSPI_release();g_task=1;
}
static void testDeferAndRetry() {
    JWPLC_ModbusTCPClientClass mb;
    assign(mb,1);
    auto token=mb._tcpClient._jwplcSocketToken;
    // Igual que after end()/fail(): el engine ya no está en SENDING.
    // busy() sólo debe mantenerse true por la limpieza pendiente.
    mb._state=JWPLC_MODBUS_TCP_CLIENT_IDLE;
    lockByOtherTask();
    mb.closeSession();
    check(mb._jwplcClosePending&&mb.busy() &&
          mb._tcpClient._sockindex==1 && g_hwState[1]==1 &&
          g_closed==0 && !mb._sessionConnected &&
          mb._asyncTx.resets==1 &&
          g_spiOwner==2,"SPI_BUSY_DROPPED_MODBUS_HANDLE");
    check(!mb.serviceDeferredClose() && mb._jwplcClosePending,
          "RETRY_BUSY_DROPPED_HANDLE");
    releaseOtherTask();
    check(mb.serviceDeferredClose() && !mb._jwplcClosePending &&
          !mb.busy() && !bool(mb._tcpClient) &&
          g_closed==1 && g_hwState[1]==0,"DEFERRED_CLOSE_DID_NOT_DRAIN");
    {BusScope bus(20);check(bool(bus) && !matchesLocked(token),
          "CLOSED_SOCKET_LEASE_STILL_LIVE");}
    std::cout<<"G10_I_MB_DEFERRED_CLOSE_BUSY_RETRY=PASS\n";
}
static void testStaleDoesNotCloseDifferentOwner() {
    JWPLC_ModbusTCPClientClass mb;
    assign(mb,2);
    auto old=mb._tcpClient._jwplcSocketToken;
    lockByOtherTask();
    mb.closeSession();
    releaseOtherTask();
    {BusScope bus(20);
      check(bool(bus),"STALE_GUARD");
      auto next=allocateSocketLockedAuto(2);
      check(bool(next) && next.allocation!=old.allocation,
            "REUSED_SOCKET_NO_NEW_LEASE");}
    const int before=g_closed;
    check(mb.serviceDeferredClose() && mb._tcpClient._sockindex==MAX_SOCK_NUM &&
          !mb._jwplcClosePending && g_closed==before && g_hwState[2]==1,
          "STALE_OWNER_CLOSED_NEW_SOCKET");
    std::cout<<"G10_I_MB_STALE_REUSE_PROTECTED=PASS\n";
}
static void testAlreadyClosedIdempotent() {
    JWPLC_ModbusTCPClientClass mb;
    check(mb.serviceDeferredClose() && !mb.busy(),"EMPTY_SERVICE_NOT_IDEMPOTENT");
    mb.closeSession();
    check(!mb._jwplcClosePending && !mb.busy() &&
          mb._tcpClient._sockindex==MAX_SOCK_NUM,
          "EMPTY_CLOSE_LEFT_PENDING");
    std::cout<<"G10_I_MB_NO_HANDLE_IDEMPOTENT=PASS\n";
}
int main(){
    try {
        testDeferAndRetry();
        testStaleDoesNotCloseDifferentOwner();
        testAlreadyClosedIdempotent();
        check(g_spiOwner==0 && g_takes==g_releases,"BUS_MUTEX_LEAK");
        std::cout<<"G10_I_MB_CLEANUP_NATIVE=PASS_SOURCE_METHODS_NO_PRODUCT_ADOPTION\n";
        return 0;
    }catch(const std::exception& ex){
        std::cerr<<"G10_I_MB_CLEANUP_NATIVE_ERROR="<<ex.what()<<"\n";
        return 3;
    }
}

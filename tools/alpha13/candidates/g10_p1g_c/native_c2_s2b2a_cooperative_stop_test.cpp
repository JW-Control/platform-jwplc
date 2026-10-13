// C2-S2B2A: native C++ harness of REAL patched beginStopAsync,
// pollStopAsync, stop methods, after S2B1 overlay. Only a safety slice.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_lockOwner=0,g_takes=0,g_releases=0;
static int g_discon=0,g_closes=0,g_flushCancel=0,g_writeCancel=0;
static uint32_t g_ms=100;
static uint8_t g_s[8] = {};
static void check(bool ok,const char* why) {
    if(!ok)throw std::runtime_error(why);
}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if(g_lockOwner)return false;
    g_lockOwner=g_task;
    ++g_takes;return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_lockOwner==g_task,"SPI_RELEASE_OTHER_TASK");
    g_lockOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_lockOwner==g_task,"DESELECT_WITHOUT_LOCK");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static uint32_t millis(){return g_ms;}
static void delay(int t){g_ms+=t;}
struct SnSR {
    static constexpr uint8_t CLOSED=0,ESTABLISHED=0x17,FIN_WAIT=0x18;
};
struct MockEthernet {
    uint8_t socketStatus(uint8_t i) {
        check(ownedByThisTask(),"STATUS_WITHOUT_GUARD");
        return g_s[i];
    }
    void socketDisconnect(uint8_t i) {
        check(ownedByThisTask(),"DISCONNECT_WITHOUT_GUARD");
        ++g_discon;g_s[i]=SnSR::FIN_WAIT;
    }
    void socketClose(uint8_t i) {
        check(ownedByThisTask(),"CLOSE_WITHOUT_GUARD");
        ++g_closes;g_s[i]=SnSR::CLOSED;
    }
};
static MockEthernet Ethernet;
class EthernetClient {
public:
    uint8_t _sockindex=MAX_SOCK_NUM;
    uint16_t _timeout=1000;
    bool _stopPending=false;
    uint32_t _stopStartedAtMs=0;
    SocketToken _jwplcSocketToken{};
    void cancelFlushAsync(){++g_flushCancel;}
    void cancelWriteAsync(){++g_writeCancel;}
    int beginStopAsync();
    int pollStopAsync();
    void stop();
    void setTimeout(uint16_t t){_timeout=t;}
};

#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static EthernetClient makeClient(uint8_t idx,uint32_t owner) {
    EthernetClient c;
    c._sockindex=idx;
    BusScope guard(20);
    check(bool(guard),"INITIAL_SETUP_LOCK");
    c._jwplcSocketToken=allocateSocketLocked(idx,owner);
    check(bool(c._jwplcSocketToken),"INITIAL_LEASE_FAILED");
    g_s[idx]=SnSR::ESTABLISHED;
    return c;
}
static void testStartAndPoll() {
    auto c=makeClient(0,1001);
    check(c.beginStopAsync()==0 && c._stopPending,"NO_PENDING_AFTER_FIN");
    check(g_discon==1 && g_flushCancel==1 && g_writeCancel==1,
          "DISCONNECT_NOT_STARTED_EXACTLY_ONCE");
    check(c.beginStopAsync()==0 && g_discon==1,"DUPLICATE_DISCONNECT");
    g_s[0]=SnSR::CLOSED;
    check(c.pollStopAsync()==1 && c._sockindex==MAX_SOCK_NUM,
          "POLL_CLOSED_NOT_FINISHED");
    check(g_closes==0,"NORMAL_FIN_FORCED_CLOSE");
    {
        BusScope bus(20);
        check(bool(bus) && !matchesLocked(c._jwplcSocketToken),
              "FINISHED_LEASE_NOT_INVALIDATED");
    }
    std::cout<<"C2_S2B2A_GRACEFUL_STOP=PASS\n";
}
static void testBusyStartAndRetry() {
    auto c=makeClient(1,1002);
    const int before=g_discon, flush=g_flushCancel;
    g_task=2;
    check(jwplcSPI_acquire(0),"OTHER_TASK_BUSY_SETUP");
    g_task=1;
    check(c.beginStopAsync()==0 && c._sockindex==1 &&
          !c._stopPending && g_discon==before && g_flushCancel==flush,
          "BUS_BUSY_START_MUTATED_HANDLE_OR_HARDWARE");
    check(c.pollStopAsync()==0 && c._sockindex==1,
          "BUS_BUSY_POLL_DROPPED_HANDLE");
    g_task=2;jwplcSPI_release();g_task=1;
    check(c.pollStopAsync()==0 && c._stopPending,
          "RETRY_DID_NOT_START_DISCONNECT");
    check(g_discon==before+1,"RETRY_DOUBLE_DISCONNECT");
    g_ms+=1500;
    check(c.pollStopAsync()==-1 && c._sockindex==MAX_SOCK_NUM,
          "FORCED_CLOSE_AFTER_TIMEOUT_FAILED");
    check(g_closes==1,"TIMEOUT_DID_NOT_SEND_CLOSE");
    std::cout<<"C2_S2B2A_BUSY_PRESERVES_HANDLE_AND_RETRIES=PASS\n";
}
static void testStaleOwner() {
    auto old=makeClient(2,1003);
    auto copy=old;
    {
        BusScope bus(20);
        check(bool(bus),"RECYCLING_LOCK");
        auto fresh=allocateSocketLocked(2,2003);
        check(bool(fresh) && !matchesLocked(old._jwplcSocketToken),
              "RECYCLING_NOT_INVALIDATED");
    }
    const int before=g_discon,closeBefore=g_closes;
    check(old.beginStopAsync()==1 && old._sockindex==MAX_SOCK_NUM,
          "STALE_BEGIN_DID_NOT_DETACH");
    copy._stopPending=true;
    check(copy.pollStopAsync()==1 && copy._sockindex==MAX_SOCK_NUM,
          "STALE_POLL_DID_NOT_DETACH");
    check(g_discon==before && g_closes==closeBefore,
          "STALE_HANDLE_TOUCHES_NEW_OWNER");
    std::cout<<"C2_S2B2A_STALE_OWNER_NO_CLOSE=PASS\n";
}
static void testBoundedLegacyStop() {
    auto c=makeClient(3,1004);
    c.setTimeout(5);
    g_task=2;check(jwplcSPI_acquire(0),"STOP_CONTENTION_SETUP");g_task=1;
    const int closes=g_closes;
    const uint32_t start=g_ms;
    c.stop();
    check(g_ms-start>=5 && g_ms-start<=6,
          "LEGACY_STOP_NOT_BOUNDED");
    check(c._sockindex==3 && !c._stopPending && g_closes==closes,
          "LEGACY_STOP_BUSY_DISCARDED_HANDLE");
    g_task=2;jwplcSPI_release();g_task=1;
    check(c.beginStopAsync()==0,"LEGACY_RETRY_AFTER_CONTENTION_FAILED");
    g_s[3]=SnSR::CLOSED;
    check(c.pollStopAsync()==1,"LEGACY_RETRY_POLL_FAILED");
    std::cout<<"C2_S2B2A_LEGACY_STOP_BOUNDED_RETRYABLE=PASS\n";
}
int main(){
    try{
        testStartAndPoll();
        testBusyStartAndRetry();
        testStaleOwner();
        testBoundedLegacyStop();
        check(g_lockOwner==0 && g_takes==g_releases,
              "END_LOCK_LEAK_OR_DOUBLE_RELEASE");
        std::cout<<"C2_S2B2A_NATIVE_RESULT=PASS_STOP_SLICE_NO_PRODUCT_ADOPTION\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"C2_S2B2A_NATIVE_FAILURE="<<e.what()<<"\n";
        return 3;
    }
}

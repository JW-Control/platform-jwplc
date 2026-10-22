// G10-I B08: extract six real wrapper bodies from the composed overlay.
// Real JWPLC_W5500_SafeAccess.cpp; host reproduces the core's NON-recursive SPI mutex.
// Neither successful compilation nor a mock-only BusScope qualifies adoption.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
using namespace jwplc_w5500_candidate;

#define JWPLC_HAS_ETHERNET 1
static int g_task=1, g_rawOwner=0, g_takes=0, g_releases=0, g_attempts=0;
static int g_closes=0, g_reads=0;
static void check(bool ok,const char* why) { if (!ok) throw std::runtime_error(why); }

extern "C" bool jwplcSPI_acquire(uint32_t) {
    ++g_attempts;
    // xSemaphoreCreateMutex() is NOT recursive, including for the same task.
    if (g_rawOwner!=0) return false;
    g_rawOwner=g_task;
    ++g_takes;
    return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_rawOwner==g_task,"B08_FOREIGN_MUTEX_RELEASE");
    g_rawOwner=0;
    ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_rawOwner==g_task,"B08_CS_WITHOUT_MUTEX");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}

enum { JWPLC_MODBUS_TCP_CLIENT_BUS_LOCK_TIMEOUT=7,
       JWPLC_MODBUS_TCP_BUS_LOCK_TIMEOUT=8 };
struct JWPLC_EthernetClass {
    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
};
struct JWPLC_ModbusTCPClientClass {
    struct { int busLockTimeouts=0; } _stats;
    int _result=0;
    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
};
struct JWPLC_ModbusTCPClass {
    struct { int busLockTimeouts=0; } _stats;
    int _lastError=0;
    void setError(int error) { _lastError=error; }
    bool acquireBus(uint32_t timeoutMs);
    void releaseBus();
};

// The gate writes original composed source bodies to this include.
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static uint8_t readHardware(uint8_t socket,void*) {
    check(ownedByThisTask(),"B08_READ_OUTSIDE_W5500_SCOPE");
    ++g_reads;
    return static_cast<uint8_t>(0x10 + socket);
}
static void closeHardware(uint8_t,void*) {
    check(ownedByThisTask(),"B08_CLOSE_OUTSIDE_W5500_SCOPE");
    ++g_closes;
}
template<typename Wrapper>
static void verifyWrapper(const char* label,Wrapper& wrapper,uint8_t index) {
    const int before=g_takes;
    check(wrapper.acquireBus(0),"B08_OUTER_ACQUIRE_FAILED");
    check(g_takes==before+1 && ownedByThisTask() && g_rawOwner==g_task,
          "B08_WRAPPER_NOT_TRACKED_AS_W5500_OWNER");
    const SocketToken token=allocateSocketLockedAuto(index);
    check(bool(token),"B08_TOKEN_NO_OUTER_SCOPE");

    // A direct second lock MUST fail (non-recursive), but the W5500 nested
    // scope must not re-acquire that mutex and must still permit SPI work.
    const int previousAttempts=g_attempts;
    check(!jwplcSPI_acquire(0),"B08_NONRECURSIVE_STUB_INCORRECT");
    check(g_attempts==previousAttempts+1,"B08_RAW_MUTEX_NOT_INSTRUMENTED");
    uint8_t observed=0xa5;
    const auto probe=readStatusIfCurrent(token,readHardware,nullptr,0,&observed);
    check(probe==StatusReadOutcome::Read && observed==0x10+index,
          "B08_NONRECURSIVE_SELF_CONTENTION");
    {
        BusScope nested(0);
        check(bool(nested),"B08_NESTED_SCOPE_BUSY");
        check(matchesLocked(token),"B08_NESTED_SCOPE_WRONG_OWNER");
    }
    check(closeIfCurrent(token,closeHardware,nullptr,0)==CloseOutcome::Closed,
          "B08_CLOSE_SELF_CONTENTION");
    wrapper.releaseBus();
    check(!ownedByThisTask() && g_rawOwner==0 && g_takes==g_releases,
          "B08_WRAPPER_RELEASE_LEAK");
    std::printf("G10_I_B08_%s_NESTED=PASS\n",label);
}
int main() { try {
    JWPLC_ModbusTCPClientClass client;
    JWPLC_ModbusTCPClass server;
    JWPLC_EthernetClass ethernet;
    verifyWrapper("MODBUS_CLIENT",client,1);
    verifyWrapper("MODBUS_SERVER",server,2);
    verifyWrapper("ETHERNET_SERVICE",ethernet,3);
    // The W5500 helper must never inherit a raw SPI lock held by a different
    // peripheral, even when it is the same FreeRTOS task.
    check(jwplcSPI_acquire(0),"B08_OTHER_PERIPHERAL_PREP");
    check(!ethernet.acquireBus(0),"B08_INHERITED_OTHER_PERIPHERAL_LOCK");
    check(!ownedByThisTask(),"B08_FOREIGN_SAME_TASK_SCOPE_LEAK");
    jwplcSPI_release();
    // A second task holding SPI likewise blocks without corrupting owner.
    g_task=2;
    check(jwplcSPI_acquire(0),"B08_SECOND_TASK_PREP");
    g_task=1;
    check(!client.acquireBus(0),"B08_FOREIGN_TASK_LOCK_ACCEPTED");
    check(!ownedByThisTask(),"B08_FOREIGN_TASK_SCOPE_LEAK");
    g_task=2;
    jwplcSPI_release();
    g_task=1;
    check(g_reads==3 && g_closes==3,"B08_SOURCE_PATH_NOT_EXECUTED");
    check(g_rawOwner==0 && g_takes==g_releases,"B08_FINAL_MUTEX_LEAK");
    std::puts("G10_I_B08_MUTEX_NATIVE=PASS_REAL_WRAPPERS_NONRECURSIVE");
    return 0;
} catch(const std::exception& e) {
    std::fprintf(stderr,"B08_ERROR=%s\n",e.what());
    return 3;
} }

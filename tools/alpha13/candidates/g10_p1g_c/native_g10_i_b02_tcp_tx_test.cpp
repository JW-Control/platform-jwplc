// B02 TCP TX: execute source-derived Client methods with real SafeAccess.
// W5500 SEND calls are instrumented at the boundary; yield must be outside SPI.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_lockOwner=0,g_takes=0,g_releases=0;
static int g_begins=0,g_polls=0,g_closes=0,g_yields=0;
static uint32_t g_time=100;
static SocketToken g_expectedToken{};
enum class SendMode { Success, BeginFailure, PollFailure, PollTimeout };
enum class YieldAction { None, ReleaseOther, HoldOther, Recycle };
static SendMode g_sendMode=SendMode::Success;
static YieldAction g_yieldAction=YieldAction::None;
static int g_noSpace=0;
static uint8_t g_recycleSocket=0;
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){
    if(g_lockOwner)return false;
    g_lockOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void){
    check(g_lockOwner==g_task,"B02_TX_WRONG_RELEASE");
    g_lockOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static uint32_t millis(){return g_time;}
static void holdOther(){
    g_task=2;check(jwplcSPI_acquire(0),"B02_TX_SETUP_LOCK");g_task=1;
}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static void yield(){
    check(!ownedByThisTask(),"B02_TX_YIELD_WITH_SPI_MUTEX");
    ++g_yields;++g_time;
    if(g_yieldAction==YieldAction::ReleaseOther && g_yields==2){
        releaseOther();g_yieldAction=YieldAction::None;
    } else if(g_yieldAction==YieldAction::HoldOther && g_yields==1){
        holdOther();g_yieldAction=YieldAction::None;
    } else if(g_yieldAction==YieldAction::Recycle && g_yields==1){
        g_task=2;
        {BusScope bus(0);check(bool(bus),"B02_TX_RECYCLE_BUS");
            (void)allocateSocketLockedAuto(g_recycleSocket);}
        g_task=1;g_yieldAction=YieldAction::None;
    }
}
struct {size_t SSIZE=4;} W5100;
static void verifySendAccess(){
    check(ownedByThisTask(),"B02_TX_IO_WITHOUT_BUS");
    check(matchesLocked(g_expectedToken),"B02_TX_STALE_SENT_TO_NEW_OWNER");
}
struct MockEthernet {
    int socketBeginSendTCP(uint8_t,const uint8_t*,uint16_t){
        verifySendAccess();++g_begins;
        if(g_sendMode==SendMode::BeginFailure)return -1;
        if(g_noSpace>0){--g_noSpace;return 0;}
        return 1;
    }
    int socketPollSendTCP(uint8_t){
        verifySendAccess();++g_polls;
        if(g_sendMode==SendMode::PollFailure)return -1;
        if(g_sendMode==SendMode::PollTimeout)return 0;
        return 1;
    }
    void socketClose(uint8_t){verifySendAccess();++g_closes;}
} Ethernet;
struct EthernetClient {
    uint8_t _sockindex=MAX_SOCK_NUM;
    SocketToken _jwplcSocketToken{};
    uint32_t _timeout=20,_writeAsyncStartedAtMs=0;
    bool _stopPending=false,_flushPending=false;
    uint8_t _writeAsyncState=0;
    const uint8_t* _writeAsyncBuffer=nullptr;
    uint16_t _writeAsyncLength=0;
    int _writeErrors=0;
    void setWriteError(){++_writeErrors;}
    size_t write(const uint8_t*,size_t);
    int beginWriteAsync(const uint8_t*,size_t);
    int pollWriteAsync();
    bool writeAsyncInProgress() const;
    void cancelWriteAsync();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static void bind(EthernetClient& client,uint8_t socket){
    BusScope bus(0);check(bool(bus),"B02_TX_BIND_BUS");
    client._sockindex=socket;
    client._jwplcSocketToken=allocateSocketLockedAuto(socket);
    g_expectedToken=client._jwplcSocketToken;
}
static void resetScenario(){
    g_sendMode=SendMode::Success;g_yieldAction=YieldAction::None;
    g_noSpace=0;g_begins=0;g_polls=0;g_yields=0;
}
static void testCurrentOwner(){
    resetScenario();EthernetClient client;bind(client,0);
    const uint8_t bytes[6]={1,2,3,4,5,6};
    check(client.write(bytes,6)==6 && g_begins==2 && g_polls==2 &&
          g_yields>=2 && !client.writeAsyncInProgress() &&
          client._writeErrors==0,"B02_TX_VALID_OWNER_FAILED");
    std::cout<<"G10_I_B02_TCP_TX_CURRENT_OWNER=PASS\n";
}
static void testBusyRecovery(){
    resetScenario();EthernetClient client;bind(client,1);
    holdOther();g_yieldAction=YieldAction::ReleaseOther;
    const uint8_t byte=7;
    check(client.write(&byte,1)==1 && g_begins==1 && g_polls==1 &&
          g_yields>=3 && client._writeErrors==0,
          "B02_TX_BUSY_DID_NOT_RECOVER");
    std::cout<<"G10_I_B02_TCP_TX_BUSY_RETRY=PASS\n";
}
static void testRecycledSocket(){
    resetScenario();EthernetClient client;bind(client,2);
    g_recycleSocket=2;g_yieldAction=YieldAction::Recycle;
    const uint8_t byte=8;
    check(client.write(&byte,1)==0 && g_begins==1 && g_polls==0 &&
          client._sockindex==MAX_SOCK_NUM &&
          !client.writeAsyncInProgress() && client._writeErrors==1,
          "B02_TX_RECYCLED_OWNER_USED");
    std::cout<<"G10_I_B02_TCP_TX_RECYCLED=PASS\n";
}
static void testTimeoutBeforeSend(){
    resetScenario();EthernetClient client;bind(client,3);
    client._timeout=3;
    holdOther();
    const uint8_t byte=9;
    check(client.write(&byte,1)==0 && g_begins==0 &&
          !client.writeAsyncInProgress() && client._writeAsyncBuffer==nullptr &&
          client._writeErrors==1 && client._sockindex==3,
          "B02_TX_TIMEOUT_BORROWED_BUFFER_RETAINED");
    releaseOther();
    std::cout<<"G10_I_B02_TCP_TX_TIMEOUT_BEFORE_SEND=PASS\n";
}
static void testTimeoutAfterSend(){
    resetScenario();EthernetClient client;bind(client,4);
    client._timeout=3;g_sendMode=SendMode::PollTimeout;
    g_yieldAction=YieldAction::HoldOther;
    const uint8_t byte=10;
    const int closesBefore=g_closes;
    check(client.write(&byte,1)==0 && g_begins==1 && g_polls==0 &&
          client._writeAsyncState==2 && client._writeAsyncBuffer==nullptr &&
          client._sockindex==4 && g_closes==closesBefore &&
          client._writeErrors==1,
          "B02_TX_TIMEOUT_AFTER_SEND_LOST_OWNER");
    releaseOther();client.cancelWriteAsync();
    check(g_closes==closesBefore+1 && client._sockindex==MAX_SOCK_NUM &&
          !client.writeAsyncInProgress(),"B02_TX_TIMEOUT_CLOSE_NOT_RETRIED");
    std::cout<<"G10_I_B02_TCP_TX_TIMEOUT_AFTER_SEND=PASS\n";
}
static void testTransmissionFailure(){
    const uint8_t byte=11;
    resetScenario();EthernetClient beginFail;bind(beginFail,5);
    g_sendMode=SendMode::BeginFailure;
    check(beginFail.write(&byte,1)==0 && beginFail._writeErrors==1 &&
          !beginFail.writeAsyncInProgress(),"B02_TX_BEGIN_FAILURE_NOT_REPORTED");
    resetScenario();EthernetClient pollFail;bind(pollFail,6);
    g_sendMode=SendMode::PollFailure;
    check(pollFail.write(&byte,1)==0 && pollFail._writeErrors==1 &&
          !pollFail.writeAsyncInProgress(),"B02_TX_POLL_FAILURE_NOT_REPORTED");
    std::cout<<"G10_I_B02_TCP_TX_TRANSMISSION_FAILURE=PASS\n";
}
int main(){try{
    testCurrentOwner();testBusyRecovery();testTransmissionFailure();
    testRecycledSocket();testTimeoutBeforeSend();testTimeoutAfterSend();
    check(g_lockOwner==0 && g_takes==g_releases,"B02_TX_MUTEX_LEAK");
    return 0;
}catch(const std::exception& e){
    std::cerr<<"G10_I_B02_TCP_TX_NATIVE_ERROR="<<e.what()<<"\n";return 3;
}}

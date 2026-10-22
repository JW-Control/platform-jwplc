// Execute real Client capacity/flush/endpoint bodies with SafeAccess real.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
#define SPI_ETHERNET_SETTINGS 1
namespace SnSR { constexpr uint8_t CLOSED=0, ESTABLISHED=23, CLOSE_WAIT=28; }
static int g_task=1,g_lockOwner=0,g_takes=0,g_releases=0;
static int g_reads=0,g_polls=0,g_closes=0,g_delays=0;
static uint32_t g_now=100;
static SocketToken g_expected{};
static uint8_t g_status=SnSR::ESTABLISHED;
static uint16_t g_space=4;
static bool g_pollFails=false;
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){if(g_lockOwner)return false;g_lockOwner=g_task;++g_takes;return true;}
extern "C" void jwplcSPI_release(void){check(g_lockOwner==g_task,"B02_REMAIN_BAD_RELEASE");g_lockOwner=0;++g_releases;}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));}
static uint32_t millis(){return g_now;}
static void delay(int){check(!ownedByThisTask(),"B02_REMAIN_WAIT_WITH_BUS");++g_delays;++g_now;}
static void holdOther(){g_task=2;check(jwplcSPI_acquire(0),"B02_REMAIN_HOLD_SETUP");g_task=1;}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static void access(){
    check(ownedByThisTask(),"B02_REMAIN_IO_WITHOUT_BUS");
    check(matchesLocked(g_expected),"B02_REMAIN_STALE_SOCKET_ACCESS");
    ++g_reads;
}
struct IPAddress {
    uint32_t value=0;
    IPAddress(uint32_t v):value(v){}
    IPAddress(const uint8_t* p):value((uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]){}
};
struct MockSPI {
    void beginTransaction(int){access();}
    void endTransaction(){access();}
} SPI;
struct MockW5100 {
    size_t SSIZE=4;
    uint16_t readSnPORT(uint8_t){access();return 2100;}
    void readSnDIPR(uint8_t,uint8_t* p){access();p[0]=10;p[1]=20;p[2]=30;p[3]=40;}
    uint16_t readSnDPORT(uint8_t){access();return 502;}
} W5100;
struct MockEthernet {
    uint16_t socketSendAvailable(uint8_t){access();return g_space;}
    uint8_t socketStatus(uint8_t){access();return g_status;}
    int socketBeginSendTCP(uint8_t,const uint8_t*,uint16_t){access();return 1;}
    int socketPollSendTCP(uint8_t){access();++g_polls;return g_pollFails?-1:1;}
    void socketClose(uint8_t){access();++g_closes;}
} Ethernet;
struct EthernetClient {
    uint8_t _sockindex=MAX_SOCK_NUM,_writeAsyncState=0;
    SocketToken _jwplcSocketToken{};
    uint32_t _timeout=20,_flushStartedAtMs=0,_writeAsyncStartedAtMs=0;
    bool _flushPending=false,_stopPending=false;
    const uint8_t* _writeAsyncBuffer=nullptr;
    uint16_t _writeAsyncLength=0;
    int availableForWrite(void);
    int beginFlushAsync();
    int pollFlushAsync();
    bool flushAsyncInProgress() const;
    void cancelFlushAsync();
    void flush();
    uint16_t localPort();
    IPAddress remoteIP();
    uint16_t remotePort();
    bool writeAsyncInProgress() const;
    int pollWriteAsync();
    void cancelWriteAsync();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static void bind(EthernetClient& c,uint8_t slot){
    BusScope bus(0);check(bool(bus),"B02_REMAIN_BIND");
    c._sockindex=slot;c._jwplcSocketToken=allocateSocketLockedAuto(slot);
    g_expected=c._jwplcSocketToken;
}
static void reset(){g_status=SnSR::ESTABLISHED;g_space=4;g_pollFails=false;g_reads=0;g_polls=0;}
static void current(){
    reset();EthernetClient c;bind(c,0);
    check(c.availableForWrite()==4 && c.localPort()==2100 &&
          c.remoteIP().value==0x0A141E28 && c.remotePort()==502,
          "B02_REMAIN_CURRENT_ENDPOINT_FAILED");
    check(c.beginFlushAsync()==1 && !c.flushAsyncInProgress(),"B02_REMAIN_CURRENT_FLUSH_FAILED");
    std::cout<<"G10_I_B02_REMAIN_CURRENT=PASS\n";
}
static void busy(){
    reset();EthernetClient c;bind(c,1);holdOther();
    const int before=g_reads;
    check(c.availableForWrite()==0 && c.localPort()==0 &&
          c.remoteIP().value==0 && c.remotePort()==0 && g_reads==before,
          "B02_REMAIN_BUSY_ENDPOINT_ACCESS");
    check(c.beginFlushAsync()==0 && c.flushAsyncInProgress() && g_reads==before,
          "B02_REMAIN_BUSY_FLUSH_ACCESS");
    releaseOther();
    check(c.pollFlushAsync()==1 && !c.flushAsyncInProgress(),"B02_REMAIN_BUSY_RECOVERY_FAILED");
    std::cout<<"G10_I_B02_REMAIN_BUSY=PASS\n";
}
static void recycled(){
    reset();EthernetClient c;bind(c,2);
    {BusScope bus(0);g_expected=allocateSocketLockedAuto(2);}
    const int before=g_reads;
    check(c.availableForWrite()==0 && c.localPort()==0 &&
          c.remoteIP().value==0 && c.remotePort()==0 && g_reads==before,
          "B02_REMAIN_STALE_SOCKET_ACCESS");
    c._flushPending=true;c._flushStartedAtMs=g_now;
    check(c.pollFlushAsync()==-1 && c._sockindex==MAX_SOCK_NUM &&
          !c.flushAsyncInProgress() && g_reads==before,
          "B02_REMAIN_STALE_FLUSH_ACCESS");
    std::cout<<"G10_I_B02_REMAIN_RECYCLED=PASS\n";
}
static void timeoutAndCancel(){
    reset();EthernetClient c;bind(c,3);c._timeout=2;
    c._flushPending=true;c._flushStartedAtMs=g_now;
    holdOther();g_now+=3;
    const int before=g_reads;
    check(c.pollFlushAsync()==-1 && !c.flushAsyncInProgress() && g_reads==before,
          "B02_REMAIN_BUSY_TIMEOUT_NOT_HONORED");
    c._flushPending=true;c.cancelFlushAsync();
    check(!c.flushAsyncInProgress() && g_reads==before,"B02_REMAIN_CANCEL_TOUCHED_BUS");
    releaseOther();
    std::cout<<"G10_I_B02_REMAIN_TIMEOUT_CANCEL=PASS\n";
}
static void sendFailure(){
    reset();EthernetClient c;bind(c,4);
    c._flushPending=true;c._flushStartedAtMs=g_now;
    c._writeAsyncState=2;c._writeAsyncStartedAtMs=g_now;
    g_pollFails=true;
    check(c.pollFlushAsync()==-1 && !c.flushAsyncInProgress() &&
          !c.writeAsyncInProgress() && g_polls==1,
          "B02_REMAIN_SEND_FAILURE_MASKED");
    std::cout<<"G10_I_B02_REMAIN_SEND_FAILURE=PASS\n";
}
static void cooperativeWait(){
    reset();EthernetClient c;bind(c,5);
    c._timeout=3;g_space=0;g_delays=0;
    c.flush();
    check(g_delays>=3 && !c.flushAsyncInProgress(),
          "B02_REMAIN_FLUSH_TIMEOUT_FAILED");
    std::cout<<"G10_I_B02_REMAIN_COOPERATIVE_WAIT=PASS\n";
}
int main(){try{
    current();busy();recycled();timeoutAndCancel();sendFailure();cooperativeWait();
    check(g_lockOwner==0 && g_takes==g_releases,"B02_REMAIN_MUTEX_LEAK");
    return 0;
}catch(const std::exception& e){std::cerr<<"G10_I_B02_REMAIN_ERROR="<<e.what()<<"\n";return 3;}}

// Source-derived Server begin/available/bool/write and real Client constructor.
// The W5500 and the already source-tested Client TX path are boundaries.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
struct SnMR {static constexpr uint8_t TCP=1;};
struct SnSR {static constexpr uint8_t CLOSED=0,INIT=0x13,LISTEN=0x14,
    ESTABLISHED=0x17,CLOSE_WAIT=0x1c;};
static int g_task=1,g_owner=0,g_takes=0,g_releases=0,g_hwReads=0;
static int g_writes=0,g_wrongWrites=0,g_disconnects=0,g_recycleAtWrite=-1;
static uint8_t g_status[MAX_SOCK_NUM]={};
static int g_rx[MAX_SOCK_NUM]={};
static size_t g_sendResult[MAX_SOCK_NUM]={};
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){if(g_owner)return false;g_owner=g_task;++g_takes;return true;}
extern "C" void jwplcSPI_release(void){check(g_owner==g_task,"B05_BAD_RELEASE");g_owner=0;++g_releases;}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));}
static void requireBus(){check(ownedByThisTask(),"B05_W5500_OUTSIDE_BUS");++g_hwReads;}
static void holdOther(){g_task=2;check(jwplcSPI_acquire(0),"B05_HOLD_FAILED");g_task=1;}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
class EthernetServer;
class EthernetClient {
public:
    EthernetClient():_sockindex(MAX_SOCK_NUM),_timeout(1000){}
    __REAL_CLIENT_CTOR__
    uint8_t getSocketNumber() const{return _sockindex;}
    size_t write(const uint8_t*,size_t);
private:
    uint8_t _sockindex=MAX_SOCK_NUM;
    uint16_t _timeout=1000;
    uint32_t _startMillis=0;
    SocketToken _jwplcSocketToken{};
    friend class EthernetServer;
};
class EthernetServer {
public:
    explicit EthernetServer(uint16_t port):_port(port){}
    void begin();EthernetClient available();operator bool();
    size_t write(const uint8_t*,size_t);
    static uint16_t server_port[MAX_SOCK_NUM];
private:
    uint16_t _port;
};
uint16_t EthernetServer::server_port[MAX_SOCK_NUM]={};
struct MockW5100 {uint8_t getChip(){requireBus();return 55;}} W5100;
struct MockEthernet {
    uint8_t socketBegin(uint8_t,uint16_t){
        requireBus();
        for(uint8_t i=0;i<MAX_SOCK_NUM;i++)if(g_status[i]==SnSR::CLOSED){
            EthernetServer::server_port[i]=0;
            check(bool(allocateSocketLockedAuto(i)),"B05_ALLOCATE_FAILED");
            g_status[i]=SnSR::INIT;return i;
        }
        return MAX_SOCK_NUM;
    }
    int socketListen(uint8_t i){requireBus();g_status[i]=SnSR::LISTEN;return 1;}
    void socketDisconnect(uint8_t i){requireBus();++g_disconnects;g_status[i]=SnSR::CLOSED;}
    uint8_t socketStatus(uint8_t i){
        requireBus();
        check(listenerMatchesLocked(i,EthernetServer::server_port[i],
              currentSocketTokenLocked(i)),"B05_STALE_LISTENER_STATUS_READ");
        return g_status[i];
    }
    int socketRecvAvailable(uint8_t i){requireBus();return g_rx[i];}
} Ethernet;
size_t EthernetClient::write(const uint8_t*,size_t n){
    check(!ownedByThisTask(),"B05_BROADCAST_WRITE_WITH_BUS");
    if(g_recycleAtWrite==_sockindex){
        g_recycleAtWrite=-1;g_task=2;
        {BusScope bus(0);check(bool(bus),"B05_RECYCLE_LOCK");
         (void)allocateSocketLockedAuto(_sockindex);}
        g_task=1;
    }
    BusScope bus(20);check(bool(bus),"B05_CLIENT_WRITE_BUS");
    if(!matchesLocked(_jwplcSocketToken))return 0;
    ++g_writes;
    if(g_status[_sockindex]!=SnSR::ESTABLISHED){++g_wrongWrites;return 0;}
    return g_sendResult[_sockindex]<n?g_sendResult[_sockindex]:n;
}
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static bool resetChip(void*){
    check(ownedByThisTask(),"B05_RESET_WITHOUT_BUS");
    for(auto& status:g_status)status=SnSR::CLOSED;
    return true;
}
static void resetScenario(){
    check(resetAndInvalidate(resetChip,nullptr,20),"B05_RESET_SETUP_FAILED");
    for(auto& port:EthernetServer::server_port)port=0;
    for(auto& rx:g_rx)rx=0;
    for(auto& result:g_sendResult)result=0;
    g_hwReads=0;g_writes=0;g_wrongWrites=0;g_recycleAtWrite=-1;
}
static void currentAndReopen(){
    resetScenario();
    EthernetServer server(502);server.begin();
    check(bool(server) && EthernetServer::server_port[0]==502,
          "B05_CURRENT_LISTENER_MISSING");
    g_status[0]=SnSR::CLOSED;
    check(!bool(server),"B05_CLOSED_LISTENER_REPORTED_ACTIVE");
    server.begin();
    check(bool(server) && EthernetServer::server_port[0]==502,
          "B05_REOPEN_LISTENER_FAILED");
    std::cout<<"G10_I_B05_LISTENER_REOPEN=PASS\n";
}
static void busy(){
    resetScenario();EthernetServer server(502);server.begin();
    const int before=g_hwReads;
    holdOther();
    check(!bool(server) && g_hwReads==before,"B05_BUSY_BOOL_TOUCHED_W5500");
    const uint8_t byte=1;
    check(server.write(&byte,1)==0 && g_hwReads==before && g_writes==0,
          "B05_BUSY_WRITE_TOUCHED_W5500");
    releaseOther();
    check(bool(server),"B05_BUSY_RECOVERY_FAILED");
    std::cout<<"G10_I_B05_BUSY=PASS\n";
}
static void staleAndReset(){
    resetScenario();EthernetServer server(502);server.begin();
    {BusScope bus(0);const auto old=currentSocketTokenLocked(0);
     check(invalidateSocketLocked(old),"B05_INVALIDATE_SETUP");}
    check(!bool(server),"B05_INVALID_OWNER_LISTENER_ACCEPTED");
    {BusScope bus(0);(void)allocateSocketLockedAuto(0);}
    // server_port remains stale, while the SafeAccess listener map was cleared.
    check(!bool(server),"B05_RECYCLED_LISTENER_ACCEPTED");
    g_status[0]=SnSR::ESTABLISHED;g_sendResult[0]=1;
    const uint8_t byte=1;
    check(server.write(&byte,1)==0 && g_writes==0,
          "B05_STALE_LISTENER_BROADCAST");
    check(resetAndInvalidate(resetChip,nullptr,20),"B05_RESET_FAILED");
    check(!bool(server),"B05_OLD_EPOCH_LISTENER_ACCEPTED");
    server.begin();
    check(bool(server),"B05_LISTENER_AFTER_RESET_FAILED");
    std::cout<<"G10_I_B05_RECYCLED_RESET=PASS\n";
}
static void broadcast(){
    resetScenario();EthernetServer server(502);server.begin();
    g_status[0]=SnSR::ESTABLISHED;g_sendResult[0]=4;
    server.begin();
    g_status[1]=SnSR::ESTABLISHED;g_sendResult[1]=2;
    const uint8_t bytes[4]={1,2,3,4};
    const int before=g_writes;
    check(server.write(bytes,4)==2 && g_writes==before+2,
          "B05_BROADCAST_PARTIAL_RESULT_WRONG");
    // Snapshot must retain the old token even when another task recycles slot 0.
    g_recycleAtWrite=0;g_sendResult[1]=4;
    const int after=g_writes;
    check(server.write(bytes,4)==0 && g_writes==after+1 && g_wrongWrites==0,
          "B05_RECYCLED_BROADCAST_SENT_TO_NEW_OWNER");
    std::cout<<"G10_I_B05_BROADCAST_PARTIAL_RECYCLED=PASS\n";
}
static void noPeerAndFailure(){
    resetScenario();EthernetServer server(9000);server.begin();
    const uint8_t byte=1;
    check(server.write(&byte,1)==0,"B05_NO_PEER_NONZERO");
    EthernetServer first(502);first.begin();
    g_status[1]=SnSR::ESTABLISHED;g_sendResult[1]=0;
    check(first.write(&byte,1)==0,"B05_TX_TIMEOUT_NOT_REPORTED");
    std::cout<<"G10_I_B05_NO_PEER_TIMEOUT=PASS\n";
}
int main(){try{
    currentAndReopen();busy();staleAndReset();broadcast();noPeerAndFailure();
    check(g_owner==0 && g_takes==g_releases,"B05_MUTEX_LEAK");
    return 0;
}catch(const std::exception& e){std::cerr<<"G10_I_B05_ERROR="<<e.what()<<"\n";return 3;}}

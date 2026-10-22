// Real EthernetClient::cancelConnectAsync against the SafeAccess close queue.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
struct SnSR {static constexpr uint8_t CLOSED=0,ESTABLISHED=0x17,
    CLOSE_WAIT=0x1c,SYN_SENT=0x15,FIN_WAIT=0x18;};
static int g_task=1,g_owner=0,g_takes=0,g_releases=0,g_reads=0;
static int g_closes=0,g_disconnects=0,g_wrongClose=0;
static uint8_t g_status[MAX_SOCK_NUM]={};
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
static uint32_t g_now=0;
static uint32_t millis(){return g_now;}
static void delay(int ms){check(!ownedByThisTask(),"B04_WAIT_HOLDS_SPI_MUTEX");g_now+=uint32_t(ms);}
extern "C" bool jwplcSPI_acquire(uint32_t){if(g_owner)return false;g_owner=g_task;++g_takes;return true;}
extern "C" void jwplcSPI_release(void){check(g_owner==g_task,"B04_BAD_RELEASE");g_owner=0;++g_releases;}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));}
static void holdOther(){g_task=2;check(jwplcSPI_acquire(0),"B04_HOLD_FAILED");g_task=1;}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
struct MockEthernet{
  uint8_t socketStatus(uint8_t index){check(ownedByThisTask(),"B04_STATUS_NO_MUTEX");
    ++g_reads;return g_status[index];}
  void socketClose(uint8_t index){check(ownedByThisTask(),"B04_CLOSE_NO_MUTEX");
    if(!bool(currentSocketTokenLocked(index)))++g_wrongClose;
    ++g_closes;g_status[index]=SnSR::CLOSED;}
  void socketDisconnect(uint8_t index){check(ownedByThisTask(),"B04_DISCONNECT_NO_MUTEX");
    ++g_disconnects;g_status[index]=SnSR::FIN_WAIT;}
} Ethernet;
struct EthernetClient {
  uint8_t _sockindex=MAX_SOCK_NUM;
  SocketToken _jwplcSocketToken{};
  bool _jwplcDeferredConnect=false;
  bool _jwplcCancelConnectPending=false;
  bool _stopPending=false;
  uint32_t _stopStartedAtMs=0,_timeout=3;
  void cancelConnectAsync();
  void stop();
  int beginStopAsync();
  int pollStopAsync();
  void cancelStopAsync();
  void cancelFlushAsync(){}
  void cancelWriteAsync(){}
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static SocketToken assign(EthernetClient& client,uint8_t index,uint8_t status){
  BusScope bus(20);check(bool(bus),"B04_SETUP_MUTEX");
  client._sockindex=index;
  client._jwplcSocketToken=allocateSocketLockedAuto(index);
  check(bool(client._jwplcSocketToken),"B04_SETUP_TOKEN");
  g_status[index]=status;
  return client._jwplcSocketToken;
}
static uint8_t drain(){return serviceDeferredClose(
  [](uint8_t index,void*){Ethernet.socketClose(index);},nullptr,20);}
int main(){try{
  int before=g_closes;
  {
    EthernetClient temporary;assign(temporary,1,SnSR::SYN_SENT);
    holdOther();temporary.cancelConnectAsync();
    check(temporary._sockindex==1 && g_closes==before && g_reads==0,
          "B04_BUSY_CANCEL_TOUCHED_SOCKET");
    releaseOther();
  }
  check(drain()==1 && g_closes==before+1 && g_status[1]==SnSR::CLOSED,
        "B04_BUSY_CANCEL_LOST_AFTER_OBJECT_DESTROYED");
  std::cout<<"G10_I_B04_CANCEL_BUSY_DURABLE=PASS\n";
  EthernetClient stale;auto old=assign(stale,2,SnSR::SYN_SENT);
  holdOther();stale.cancelConnectAsync();releaseOther();
  {BusScope bus(20);check(bool(bus),"B04_RECYCLE_BUS");
    auto newer=allocateSocketLockedAuto(2);
    check(bool(newer) && newer.allocation!=old.allocation,"B04_RECYCLE_TOKEN");}
  before=g_closes;
  check(drain()==0 && g_closes==before && g_status[2]==SnSR::SYN_SENT,
        "B04_CANCEL_CLOSED_NEW_OWNER");
  stale.cancelConnectAsync();
  check(stale._sockindex==MAX_SOCK_NUM && g_closes==before,
        "B04_STALE_HANDLE_NOT_RELEASED");
  std::cout<<"G10_I_B04_CANCEL_RECYCLED=PASS\n";
  EthernetClient current;assign(current,3,SnSR::ESTABLISHED);
  current.cancelConnectAsync();
  check(current._sockindex==MAX_SOCK_NUM && g_disconnects==1 &&
        drain()==0,"B04_CURRENT_OWNER_CANCEL_FAILED");
  std::cout<<"G10_I_B04_CANCEL_CURRENT=PASS\n";
  {
    EthernetClient temporary;assign(temporary,4,SnSR::ESTABLISHED);
    holdOther();temporary.stop();
    check(temporary._sockindex==4 && g_status[4]==SnSR::ESTABLISHED,
          "B04_BUSY_STOP_TOUCHED_SOCKET");
    releaseOther();
  }
  before=g_closes;
  check(drain()==1 && g_closes==before+1 && g_status[4]==SnSR::CLOSED,
        "B04_STOP_TIMEOUT_LOST_AFTER_OBJECT_DESTROYED");
  std::cout<<"G10_I_B04_STOP_TIMEOUT_DURABLE=PASS\n";
  {
    EthernetClient temporary;assign(temporary,5,SnSR::ESTABLISHED);
    holdOther();temporary.cancelStopAsync();releaseOther();
  }
  before=g_closes;
  check(drain()==1 && g_closes==before+1 && g_status[5]==SnSR::CLOSED,
        "B04_CANCEL_STOP_LOST_AFTER_OBJECT_DESTROYED");
  std::cout<<"G10_I_B04_CANCEL_STOP_DURABLE=PASS\n";
  EthernetClient waiting;assign(waiting,6,SnSR::ESTABLISHED);
  before=g_closes;
  waiting.stop();
  check(waiting._sockindex==MAX_SOCK_NUM && g_closes==before+1 &&
        g_status[6]==SnSR::CLOSED,"B04_CURRENT_STOP_TIMEOUT_FAILED");
  std::cout<<"G10_I_B04_STOP_CURRENT_TIMEOUT=PASS\n";
  check(g_wrongClose==0 && g_owner==0 && g_takes==g_releases,
        "B04_MUTEX_OR_OWNER_LEAK");
  return 0;
}catch(const std::exception& error){
  std::cerr<<"G10_I_B04_CANCEL_ERROR="<<error.what()<<"\n";return 3;
}}

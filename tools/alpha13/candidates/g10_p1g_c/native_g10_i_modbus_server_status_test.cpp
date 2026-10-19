// Execute the composed Modbus TCP server service body with real SafeAccess.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
#define JWPLC_MODBUS_TCP_INT_GUIDED_RX 0
#define JWPLC_MODBUS_TCP_RX_BUDGET 64
#define JWPLC_MODBUS_TCP_MAX_ADU 260
static int g_task=1,g_owner=0,g_takes=0,g_releases=0;
static int g_statusReads=0,g_ack=0,g_disable=0,g_available=0,g_drops=0;
static uint8_t g_status[8]={};
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){if(g_owner)return false;g_owner=g_task;++g_takes;return true;}
extern "C" void jwplcSPI_release(void){check(g_owner==g_task,"SERVER_BAD_RELEASE");g_owner=0;++g_releases;}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));}
static uint32_t millis(){return 1000;}
static uint16_t readU16BE(const uint8_t*){return 0;}
enum {JWPLC_MODBUS_TCP_OK=0,JWPLC_MODBUS_TCP_TIMEOUT=1,
      JWPLC_MODBUS_TCP_INVALID_MBAP=2,JWPLC_MODBUS_TCP_INVALID_LENGTH=3};
using JWPLCModbusTCPError=int;
struct EthernetClient {
  uint8_t sock=MAX_SOCK_NUM;
  SocketToken token{};
  explicit operator bool()const{return sock<MAX_SOCK_NUM;}
  uint8_t getSocketNumber()const{return sock;}
  StatusReadOutcome jwplcProbeSocketStatus(uint8_t& value){
    if(sock>=MAX_SOCK_NUM)return StatusReadOutcome::Stale;
    return readStatusIfCurrent(token,
      [](uint8_t s,void*)->uint8_t{check(ownedByThisTask(),"SERVER_STATUS_NO_MUTEX");
        ++g_statusReads;return g_status[s];},nullptr,20,&value);
  }
  int available(){++g_available;return 0;}
  int connected(){check(ownedByThisTask(),"SERVER_CONNECTED_NO_MUTEX");
    ++g_statusReads;return g_status[sock]!=0;}
  int read(uint8_t*,uint16_t){return 0;}
};
struct MockServer{EthernetClient available(){return {};}};
struct Stats{int frameTimeouts=0,clientConnections=0,protocolErrors=0,rxFrames=0;};
struct JWPLC_ModbusTCPClass {
  EthernetClient _client;
  MockServer _server;
  Stats _stats;
  bool _jwplcDropPending=false,_clientActive=true;
  uint16_t _rxLength=0,_expectedLength=0;
  uint8_t _rxBuffer[260]={};
  uint32_t _lastRxMs=1000,_frameTimeoutMs=1000;
  int _serverState=0;
  bool acquireBus(uint32_t ms){return enter(ms);}
  void releaseBus(){leave();}
  bool serviceDeferredDrop(){return true;}
  void dropClient(){++g_drops;_clientActive=false;_client={};}
  void resetRx(){_rxLength=0;}
  void setError(int){}
  bool configureRxIntLocked(uint8_t){return true;}
  void resetRxIntSoftware(){}
  bool shouldServiceRxInt(uint32_t){return true;}
  void ackRxIntLocked(){check(ownedByThisTask(),"SERVER_ACK_NO_MUTEX");++g_ack;}
  void disableRxIntLocked(){check(ownedByThisTask(),"SERVER_DISABLE_NO_MUTEX");++g_disable;}
  void finishRxIntService(bool){}
  void noteRxIntChunkActivity(){}
  void noteRxIntFrameActivity(uint32_t){}
  bool processRequest(uint16_t,uint16_t&){return false;}
  bool sendResponse(uint16_t){return false;}
  void serviceServer();
};
#define JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE 3
#define JWPLC_MODBUS_TCP_SERVER_LISTENING 2
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static SocketToken assign(JWPLC_ModbusTCPClass& server,uint8_t index){
  BusScope bus(20);check(bool(bus),"SERVER_SETUP_BUS");
  server._client.sock=index;
  server._client.token=allocateSocketLockedAuto(index);
  check(bool(server._client.token),"SERVER_SETUP_TOKEN");
  g_status[index]=0x17;
  return server._client.token;
}
static void holdOther(){g_task=2;check(jwplcSPI_acquire(0),"SERVER_BUSY_SETUP");g_task=1;}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
int main(){try{
  JWPLC_ModbusTCPClass server;
  const auto old=assign(server,1);
  const int before=g_statusReads;
  holdOther();server.serviceServer();
  check(g_statusReads==before && g_ack==0 && g_drops==0,
        "SERVER_BUSY_CHANGED_SESSION");
  releaseOther();
  {BusScope bus(20);check(bool(bus),"SERVER_RECYCLE_BUS");
    const auto newer=allocateSocketLockedAuto(1);
    check(bool(newer) && newer.allocation!=old.allocation,"SERVER_RECYCLE_FAILED");}
  server.serviceServer();
  check(g_statusReads==before && g_ack==0 && g_available==0,
        "SERVER_STALE_TOUCHED_RECYCLED_SOCKET");
  check(g_drops==1 && !server._clientActive,"SERVER_STALE_HANDLE_NOT_RELEASED");
  std::cout<<"G10_I_B01_SERVER_BUSY_STALE=PASS\n";
  JWPLC_ModbusTCPClass current;
  assign(current,2);
  current.serviceServer();
  check(g_statusReads==before+2 && g_ack==1 && g_available==1 &&
        g_disable==0 && current._clientActive,"SERVER_CURRENT_OWNER_NOT_SERVICED");
  std::cout<<"G10_I_B01_SERVER_CURRENT=PASS\n";
  check(g_owner==0 && g_takes==g_releases,"SERVER_MUTEX_LEAK");
  return 0;
}catch(const std::exception& error){
  std::cerr<<"G10_I_B01_SERVER_ERROR="<<error.what()<<"\n";return 3;
}}

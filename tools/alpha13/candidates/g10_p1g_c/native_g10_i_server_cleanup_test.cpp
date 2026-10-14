// G10-I: two REAL ModbusTCP Server method bodies + SafeAccess.cpp candidate.
// Host SPI/socket only. These passes do not validate full Arduino hardware.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_spi=0,g_takes=0,g_releases=0,g_close=0,g_intDisabled=0,g_intReset=0,g_hwRead=0;
static int g_listen=0,g_service=0,g_error=0;
static uint8_t g_status[8]={};
static void check(bool c,const char* why){if(!c) throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t) {
  if(g_spi!=0)return false;
  g_spi=g_task; ++g_takes; return true;
}
extern "C" void jwplcSPI_release(void){
  check(g_spi==g_task,"MUTEX_RELEASE_WITHOUT_OWNER");
  g_spi=0; ++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
  check(g_spi==g_task,"MUTEX_DESELECT_WITHOUT_OWNER");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
  return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
struct EthernetClient {
  uint8_t sock=MAX_SOCK_NUM;
  SocketToken token{};
  explicit operator bool()const {return sock<MAX_SOCK_NUM;}
  uint8_t getSocketNumber()const {return sock;}
  StatusReadOutcome jwplcProbeSocketStatus(uint8_t& result){
    if(sock>=MAX_SOCK_NUM)return StatusReadOutcome::Stale;
    return readStatusIfCurrent(token,
      [](uint8_t s,void*)->uint8_t {
        check(ownedByThisTask(),"SERVER_READ_NO_MUTEX");
        ++g_hwRead; return g_status[s];},nullptr,20,&result);
  }
  void cancelStopAsync(){
    if(sock>=MAX_SOCK_NUM) return;
    auto state=closeIfCurrent(token,
      [](uint8_t s,void*) {
        check(ownedByThisTask(),"SERVER_CLOSE_NO_MUTEX");
        ++g_close; g_status[s]=0;
      },nullptr,20);
    if(state!=CloseOutcome::BusBusy){sock=MAX_SOCK_NUM;token={};}
  }
};
static bool g_ready=true;
struct MockRuntime { bool isReady()const {return g_ready;} };
static MockRuntime JWPLC_Ethernet;
enum {
  JWPLC_MODBUS_TCP_SERVER_STOPPED=0,
  JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET=1,
  JWPLC_MODBUS_TCP_SERVER_LISTENING=2,
  JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE=3,
  JWPLC_MODBUS_TCP_ETHERNET_NOT_READY=7
};
struct JWPLC_ModbusTCPClass {
  bool _serverEnabled=true,_clientActive=false,_jwplcDropPending=false,_serverListening=true;
  bool _rxIntConfigured=false;
  uint8_t _rxIntSocket=MAX_SOCK_NUM;
  int _serverState=JWPLC_MODBUS_TCP_SERVER_CLIENT_ACTIVE;
  int _rxLength=12;
  EthernetClient _client;
  bool acquireBus(uint32_t ms){return enter(ms);}
  void releaseBus(){leave();}
  void resetRx(){_rxLength=0;}
  void disableRxIntLocked(){
    check(ownedByThisTask(),"INT_DISABLE_NO_MUTEX");
    ++g_intDisabled; _rxIntConfigured=false;
  }
  void resetRxIntSoftware(){++g_intReset;_rxIntConfigured=false;}
  void dropClient();
  bool serviceDeferredDrop();
  void task();
  void setError(int){++g_error;}
  void ensureServerListening(){++g_listen;_serverListening=true;}
  void serviceServer(){++g_service;}
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static SocketToken assign(JWPLC_ModbusTCPClass& s,uint8_t sock){
  BusScope g(20);check(bool(g),"SETUP_BUS");
  s._client.sock=sock;
  s._client.token=allocateSocketLockedAuto(sock);
  check(bool(s._client.token),"TOKEN_ALLOCATION_FAILED");
  s._clientActive=true;
  s._rxIntConfigured=true;
  s._rxIntSocket=sock;
  g_status[sock]=0x17;
  return s._client.token;
}
static void enterBusy(){g_task=2;check(jwplcSPI_acquire(0),"CONTENTION_LOCK_FAILED");g_task=1;}
static void leaveBusy(){g_task=2;jwplcSPI_release();g_task=1;}
static void testDeferAndRetry(){
  JWPLC_ModbusTCPClass s;
  auto old=assign(s,1);
  enterBusy();
  s.dropClient();
  check(s._jwplcDropPending && s._client.sock==1 &&
        !s._clientActive && g_close==0 && g_spi==2,
        "SERVER_BUSY_DISCARDED_LEASE");
  check(!s.serviceDeferredDrop() && s._jwplcDropPending,
        "SERVER_BUSY_RETRY_DISCARDED_LEASE");
  leaveBusy();
  check(s.serviceDeferredDrop() && !s._jwplcDropPending &&
        !bool(s._client) && g_close==1 && g_intDisabled==1 &&
        s._rxLength==0,
        "SERVER_DEFERRED_RETRY_DID_NOT_CLOSE");
  {
    BusScope g(20);
    check(bool(g) && !matchesLocked(old),
          "SERVER_OLD_TOKEN_SURVIVES_CLOSE");
  }
  std::cout<<"G10_I_SERVER_DEFERRED_CLOSE_BUSY_RETRY=PASS\n";
}
static void testReuseNeverCloseAnotherOwner(){
  JWPLC_ModbusTCPClass s;
  auto old=assign(s,2);
  enterBusy();s.dropClient();leaveBusy();
  {BusScope g(20);check(bool(g),"TOKEN_REASSIGN_GUARD");
    auto newer=allocateSocketLockedAuto(2);
    check(bool(newer)&&newer.allocation!=old.allocation,
          "SERVER_REUSE_IDENTITY_UNCHANGED");}
  int before=g_close,disabled=g_intDisabled;
  check(s.serviceDeferredDrop() && !s._jwplcDropPending &&
        g_close==before && g_intDisabled==disabled &&
        !bool(s._client) && g_status[2]==0x17,
        "SERVER_STALE_CLOSED_OTHER_OWNER");
  std::cout<<"G10_I_SERVER_STALE_OWNER_NOT_CLOSED=PASS\n";
}
static void testNoClientAndLinkNotReady(){
  JWPLC_ModbusTCPClass s;
  g_ready=false;s.dropClient();
  check(!s._jwplcDropPending &&
        s._serverState==JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET &&
        !bool(s._client),"SERVER_EMPTY_DROP_INVALID");
  g_ready=true;
  check(s.serviceDeferredDrop(),"SERVER_EMPTY_RETRY_NOT_IDEMPOTENT");
  std::cout<<"G10_I_SERVER_EMPTY_AND_LINK_DOWN=PASS\n";
}
static void testTaskDrainsPendingEvenWhenIdle(){
  JWPLC_ModbusTCPClass s;
  const auto old=assign(s,3);
  enterBusy();
  s.dropClient();
  const int listen=g_listen,service=g_service;
  s.task();
  check(s._jwplcDropPending && s._client.sock==3 &&
        g_listen==listen && g_service==service,
        "SERVER_TASK_ENTERED_WHILE_DROP_BUSY");
  leaveBusy();
  const int before=g_close;
  s.task();
  check(!s._jwplcDropPending && !bool(s._client) &&
        g_close==before+1 && g_listen==listen+1 &&
        g_service==service+1,"SERVER_TASK_DID_NOT_DRAIN_PENDING");
  {BusScope b(20);check(bool(b)&&!matchesLocked(old),
        "SERVER_TASK_DRAIN_OLD_OWNER_SURVIVED");}
  std::cout<<"G10_I_SERVER_TASK_PENDING_DRAIN=PASS\n";
}
static void testLinkDownBusyMustNotAbandonHandle(){
  JWPLC_ModbusTCPClass s;
  auto old=assign(s,4);
  g_ready=false;
  enterBusy();
  s.task();
  check(s._jwplcDropPending && s._client.sock==4 &&
        g_status[4]==0x17 && g_spi==2,
        "SERVER_LINK_DOWN_BUSY_ABANDONED_CLIENT");
  leaveBusy();
  const int before=g_close;
  s.task();
  check(!s._jwplcDropPending && !bool(s._client) &&
        g_close==before+1 &&
        s._serverState==JWPLC_MODBUS_TCP_SERVER_WAIT_ETHERNET &&
        !s._serverListening,
        "SERVER_LINK_DOWN_DRAIN_FAILED");
  g_ready=true;
  {BusScope b(20);check(bool(b)&&!matchesLocked(old),
        "SERVER_LINK_DOWN_SOCKET_LEASE_RETAINED");}
  std::cout<<"G10_I_SERVER_LINK_DOWN_BUSY_DRAIN=PASS\n";
}
static void testDisabledServerStillDrains(){
  JWPLC_ModbusTCPClass s;
  assign(s,5);
  s._serverEnabled=false;
  enterBusy();
  s.dropClient();
  leaveBusy();
  const int before=g_close;
  s.task();
  check(!s._jwplcDropPending && !bool(s._client) &&
        g_close==before+1 && !s._clientActive,
        "SERVER_DISABLED_TASK_DID_NOT_DRAIN");
  std::cout<<"G10_I_SERVER_DISABLED_DRAIN=PASS\n";
}
int main(){
  try{
    testDeferAndRetry();
    testReuseNeverCloseAnotherOwner();
    testNoClientAndLinkNotReady();
    testTaskDrainsPendingEvenWhenIdle();
    testLinkDownBusyMustNotAbandonHandle();
    testDisabledServerStillDrains();
    check(g_spi==0&&g_takes==g_releases,"SERVER_MUTEX_LEAK");
    std::cout<<"G10_I_SERVER_CLEANUP_NATIVE=PASS_THREE_REAL_SOURCE_METHODS\n";
    return 0;
  }catch(const std::exception& e){
    std::cerr<<"G10_I_SERVER_CLEANUP_ERROR="<<e.what()<<"\n";
    return 3;
  }
}

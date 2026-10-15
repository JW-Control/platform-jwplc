// Source-backed B01 public status: real candidate C++ bodies, host W5500 boundary.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <cstdio>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
#define JWPLC_ETHERNET_ENABLE_PROFILE_HOOKS 0
#define SPI_ETHERNET_SETTINGS 0
namespace SnSR { constexpr uint8_t CLOSED=0, LISTEN=0x14, ESTABLISHED=0x17, FIN_WAIT=0x18, CLOSE_WAIT=0x1c; }
static int task=1, owner=0, reads=0, takes=0, releases=0, attempts=0;
static uint8_t hardware[8]={};
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){++attempts;if(owner)return false;owner=task;++takes;return true;}
extern "C" void jwplcSPI_release(void){check(owner==task,"BAD_RELEASE");owner=0;++releases;}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(task));}
struct MockSPI { void beginTransaction(int){check(ownedByThisTask(),"SPI_OUTSIDE_BUS");} void endTransaction(){check(ownedByThisTask(),"SPI_END_OUTSIDE_BUS");} } SPI;
struct MockW5100 {int getChip(){check(ownedByThisTask(),"CHIP_OUTSIDE_BUS");return 55;} uint8_t readSnSR(uint8_t i){check(ownedByThisTask(),"STATUS_OUTSIDE_BUS");++reads;return hardware[i];}} W5100;
struct EthernetClass {uint8_t socketStatus(uint8_t);} Ethernet;
struct EthernetClient {uint8_t _sockindex=MAX_SOCK_NUM; SocketToken _jwplcSocketToken{};int available(){return 0;}uint8_t connected();uint8_t status();StatusReadOutcome jwplcProbeSocketStatus(uint8_t&);};
static uint16_t server_port[8]={};
struct EthernetServer {uint16_t _port=502;explicit operator bool();};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static void hold(){task=2;check(jwplcSPI_acquire(0),"LOCK_SETUP");task=1;}
static void unhold(){task=2;jwplcSPI_release();task=1;}
int main(){try{
 EthernetClient client; EthernetServer server;
 {BusScope bus(0);check(bool(bus),"SETUP_BUS");client._sockindex=2;client._jwplcSocketToken=allocateSocketLockedAuto(2);hardware[2]=SnSR::ESTABLISHED;
  auto listener=allocateSocketLockedAuto(3);mapListenerLocked(3,server._port,listener);server_port[3]=server._port;hardware[3]=SnSR::LISTEN;}
 uint8_t observed=0xa5;
 check(client.connected()==1 && client.status()==SnSR::ESTABLISHED && bool(server),"REAL_CONNECTED_MISSING");
 check(client.jwplcProbeSocketStatus(observed)==StatusReadOutcome::Read && observed==SnSR::ESTABLISHED,"READ_WRONG");
 hardware[2]=SnSR::CLOSED;observed=0xa5;
 check(client.connected()==0 && client.status()==SnSR::CLOSED,"REAL_CLOSED_MISSING");
 check(client.jwplcProbeSocketStatus(observed)==StatusReadOutcome::Read && observed==SnSR::CLOSED,"CLOSED_READ_WRONG");
 hardware[2]=SnSR::ESTABLISHED;
 std::puts("B01_REAL_STATUS_CONNECTED_CLOSED=PASS");
 hold();const int before=reads;observed=0xa5;
 check(client.connected()==0 && client.status()==SnSR::CLOSED && !bool(server),"BUSY_SCALAR_CHANGED");
 check(client.jwplcProbeSocketStatus(observed)==StatusReadOutcome::BusBusy && observed==0xa5 && reads==before,"BUSY_PROBE_WRONG");
 unhold();
 check(client.connected()==1 && client.status()==SnSR::ESTABLISHED && bool(server),"RECOVERY_FAILED");
 std::puts("B01_REAL_BUSY_COLLAPSES_TO_DISCONNECTED_AND_RECOVERS=REPRODUCED");
 SocketToken newer;
 {BusScope bus(0);check(bool(bus),"RECYCLE_BUS");newer=allocateSocketLockedAuto(2);check(newer.allocation!=client._jwplcSocketToken.allocation,"RECYCLE_FAILED");hardware[2]=SnSR::ESTABLISHED;}
 observed=0xa5;const int staleReads=reads;
 check(client.connected()==0 && client.status()==SnSR::CLOSED && reads==staleReads,"STALE_SCALAR_TOUCHED_NEW_OWNER");
 check(client.jwplcProbeSocketStatus(observed)==StatusReadOutcome::Stale && observed==0xa5,"STALE_PROBE_WRONG");
 std::puts("B01_REAL_STALE_SCALAR_REJECTED=PASS");
 EthernetClient current;current._sockindex=2;current._jwplcSocketToken=newer;
 observed=0xa5;
 check(current.connected()==1 && current.status()==SnSR::ESTABLISHED &&
       current.jwplcProbeSocketStatus(observed)==StatusReadOutcome::Read &&
       observed==SnSR::ESTABLISHED,"NEW_OWNER_NOT_OBSERVED");
 EthernetClient invalid;invalid._sockindex=4;hardware[4]=SnSR::ESTABLISHED;
 observed=0xa5;const int invalidReads=reads;
 check(invalid.connected()==0 && invalid.status()==SnSR::CLOSED &&
       invalid.jwplcProbeSocketStatus(observed)==StatusReadOutcome::Stale &&
       observed==0xa5 && reads==invalidReads,"INVALID_LEASE_READ_HARDWARE");
 std::puts("B01_REAL_CURRENT_AND_INVALID_LEASE=PASS");
 EthernetClient empty;const int emptyAttempts=attempts;
 check(empty.status()==SnSR::CLOSED && attempts==emptyAttempts,
       "EMPTY_STATUS_ACQUIRED_SPI");
 std::puts("B01_REAL_EMPTY_STATUS_FAST_PATH=PASS");
 check(takes==releases&&owner==0,"LOCK_LEAK");
 return 0;
}catch(const std::exception& e){std::fprintf(stderr,"B01_ERROR=%s\n",e.what());return 3;}}

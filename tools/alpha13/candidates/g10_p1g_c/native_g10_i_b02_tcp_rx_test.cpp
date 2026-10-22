// B02: source-derived TCP Client RX methods plus real SafeAccess ownership.
// W5500 transfers are instrumented at their call boundary.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_lockOwner=0,g_calls=0,g_takes=0,g_releases=0;
static SocketToken g_expectedToken{};
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){
    if(g_lockOwner)return false;
    g_lockOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void){
    check(g_lockOwner==g_task,"B02_TCP_WRONG_RELEASE");g_lockOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static void verifyHardwareAccess(){
    check(ownedByThisTask(),"B02_TCP_IO_WITHOUT_BUS");
    check(matchesLocked(g_expectedToken),"B02_TCP_IO_STALE_OWNER");
    ++g_calls;
}
struct MockEthernet {
    int socketRecvAvailable(uint8_t){verifyHardwareAccess();return 2;}
    int socketRecv(uint8_t,uint8_t* buffer,int16_t size){
        verifyHardwareAccess();
        if(buffer && size>0)buffer[0]=0x5A;
        return size>0 ? 1 : 0;
    }
    int socketRecvTCPFastDeferred(uint8_t,uint8_t* buffer,uint16_t size){
        verifyHardwareAccess();
        if(buffer && size>0)buffer[0]=0x5A;
        return size>0 ? 1 : 0;
    }
    bool socketCommitTCPFast(uint8_t){verifyHardwareAccess();return true;}
    int socketPeek(uint8_t){verifyHardwareAccess();return 0x55;}
} Ethernet;
struct EthernetClient {
    uint8_t _sockindex=MAX_SOCK_NUM;
    SocketToken _jwplcSocketToken{};
    int available();
    int read(uint8_t*,size_t);
    int read();
    int peek();
    int jwplcReadTcpFastDeferred(uint8_t*,size_t);
    bool jwplcCommitRxFast();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static void holdOther(){
    g_task=2;check(jwplcSPI_acquire(0),"B02_TCP_SETUP_LOCK");g_task=1;
}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static void checkRejected(EthernetClient& client,int fastResult){
    const int callsBefore=g_calls;
    uint8_t byte=0;
    check(client.available()==0 && client.read(&byte,1)==0 &&
          client.read()==-1 && client.peek()==-1 &&
          client.jwplcReadTcpFastDeferred(&byte,1)==fastResult &&
          !client.jwplcCommitRxFast() && g_calls==callsBefore,
          "B02_TCP_REJECTED_PATH_TOUCHED_HARDWARE");
}
static void testTcpRx(){
    EthernetClient client;
    {
        BusScope bus(0);check(bool(bus),"B02_TCP_SETUP_BUS");
        client._sockindex=1;
        client._jwplcSocketToken=allocateSocketLockedAuto(1);
        g_expectedToken=client._jwplcSocketToken;
    }
    uint8_t byte=0;
    check(client.available()==2 && client.read(&byte,1)==1 && byte==0x5A &&
          client.read()==0x5A && client.peek()==0x55 &&
          client.jwplcReadTcpFastDeferred(&byte,1)==1 &&
          client.jwplcCommitRxFast(),"B02_TCP_CURRENT_OWNER_FAILED");
    std::cout<<"G10_I_B02_TCP_CURRENT_OWNER=PASS\n";

    holdOther();checkRejected(client,0);releaseOther();
    std::cout<<"G10_I_B02_TCP_BUSY=PASS\n";

    {
        BusScope bus(0);check(bool(bus),"B02_TCP_RECYCLE_BUS");
        (void)allocateSocketLockedAuto(1);
    }
    checkRejected(client,-1);
    std::cout<<"G10_I_B02_TCP_RECYCLED=PASS\n";

    EthernetClient invalid;
    invalid._sockindex=2;
    checkRejected(invalid,-1);
    check(g_lockOwner==0 && g_takes==g_releases,"B02_TCP_BUS_LEAK");
    std::cout<<"G10_I_B02_TCP_INVALID_LEASE=PASS\n";
}
int main(){try{testTcpRx();return 0;}
    catch(const std::exception& e){
        std::cerr<<"G10_I_B02_TCP_NATIVE_ERROR="<<e.what()<<"\n";return 3;
    }
}

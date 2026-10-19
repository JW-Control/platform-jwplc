// B02: execute source-derived UDP data methods against the real lease layer.
// W5500 byte transfers are instrumented; bus ownership and socket identity
// are checked at the point where the real methods issue hardware access.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_lockOwner=0,g_calls=0,g_takes=0,g_releases=0;
static SocketToken g_expectedToken{};
static uint8_t g_rx[]={10,20,30,40,0x12,0x34,0,2,0xAA,0xBB};
static size_t g_rxOffset=0;
static void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t){
    if(g_lockOwner)return false;
    g_lockOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void){
    check(g_lockOwner==g_task,"B02_WRONG_RELEASE");g_lockOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void){}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
struct IPAddress {
    uint8_t bytes[4]={};
    IPAddress()=default;
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d}{}
    IPAddress& operator=(const uint8_t* source){
        std::memcpy(bytes,source,4);return *this;
    }
};
struct {size_t SSIZE=2048;} W5100;
static void verifyHardwareAccess(){
    check(ownedByThisTask(),"B02_UDP_IO_WITHOUT_BUS");
    check(matchesLocked(g_expectedToken),"B02_UDP_IO_STALE_OWNER");
    ++g_calls;
}
struct MockEthernet {
    uint16_t socketBufferData(uint8_t,uint16_t,const uint8_t*,uint16_t n){
        verifyHardwareAccess();return n;
    }
    uint16_t socketRecvAvailable(uint8_t){
        verifyHardwareAccess();return static_cast<uint16_t>(sizeof(g_rx)-g_rxOffset);
    }
    int socketRecv(uint8_t,uint8_t* buffer,int16_t n){
        verifyHardwareAccess();
        if(n<=0 || g_rxOffset+static_cast<size_t>(n)>sizeof(g_rx))return 0;
        if(buffer)std::memcpy(buffer,g_rx+g_rxOffset,n);
        g_rxOffset+=n;return n;
    }
    int socketPeek(uint8_t){verifyHardwareAccess();return 0x55;}
    int socketRecvUDPFastDeferred(uint8_t,uint8_t* header,uint8_t* buffer,uint16_t){
        verifyHardwareAccess();
        std::memcpy(header,g_rx,8);buffer[0]=0xAA;buffer[1]=0xBB;return 2;
    }
    bool socketCommitUDPFast(uint8_t){verifyHardwareAccess();return true;}
} Ethernet;
struct EthernetUDP {
    uint8_t sockindex=MAX_SOCK_NUM;
    uint16_t _offset=0,_remaining=0,_remotePort=0;
    bool _sendPending=false;
    IPAddress _remoteIP;
    SocketToken _jwplcSocketToken{};
    size_t write(const uint8_t*,size_t);
    int parsePacket();
    int read();
    int read(unsigned char*,size_t);
    int peek();
    int jwplcReadPacketFastDeferred(uint8_t*,size_t);
    bool jwplcCommitRxFast();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"
static void holdOther(){
    g_task=2;check(jwplcSPI_acquire(0),"B02_SETUP_LOCK");g_task=1;
}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static void testCurrentOwner(){
    EthernetUDP udp;
    {
        BusScope bus(0);check(bool(bus),"B02_SETUP_BUS");
        udp.sockindex=1;
        udp._jwplcSocketToken=allocateSocketLockedAuto(1);
        g_expectedToken=udp._jwplcSocketToken;
    }
    const uint8_t bytes[2]={0xAA,0xBB};
    check(udp.write(bytes,2)==2 && udp._offset==2,"B02_UDP_WRITE_FAILED");
    check(udp.parsePacket()==2 && udp._remotePort==0x1234,
          "B02_UDP_PARSE_FAILED");
    uint8_t readBuffer[2]={};
    check(udp.read(readBuffer,2)==2 && readBuffer[0]==0xAA &&
          readBuffer[1]==0xBB && udp._remaining==0,"B02_UDP_READ_FAILED");
    udp._remaining=1;
    check(udp.peek()==0x55,"B02_UDP_PEEK_FAILED");
    uint8_t fast[2]={};
    check(udp.jwplcReadPacketFastDeferred(fast,2)==2 && fast[1]==0xBB,
          "B02_UDP_FAST_READ_FAILED");
    check(udp.jwplcCommitRxFast(),"B02_UDP_COMMIT_FAILED");
    std::cout<<"G10_I_B02_UDP_CURRENT_OWNER=PASS\n";

    const int callsBeforeBusy=g_calls;
    const uint16_t offsetBeforeBusy=udp._offset;
    udp._remaining=2;
    holdOther();
    check(udp.write(bytes,2)==0 && udp.parsePacket()==0 &&
          udp.read(readBuffer,2)==-1 && udp.read()==-1 &&
          udp.peek()==-1 && udp.jwplcReadPacketFastDeferred(fast,2)==0 &&
          !udp.jwplcCommitRxFast() && g_calls==callsBeforeBusy &&
          udp._offset==offsetBeforeBusy && udp._remaining==2,
          "B02_UDP_BUSY_TOUCHED_HARDWARE_OR_STATE");
    releaseOther();
    std::cout<<"G10_I_B02_UDP_BUSY=PASS\n";

    {
        BusScope bus(0);check(bool(bus),"B02_RECYCLE_BUS");
        (void)allocateSocketLockedAuto(1);
    }
    const int callsBeforeStale=g_calls;
    udp._remaining=0;
    check(udp.write(bytes,2)==0 && udp.parsePacket()==0,
          "B02_UDP_STALE_PARSE_OR_WRITE");
    udp._remaining=2;
    check(udp.read(readBuffer,2)==-1 && udp.read()==-1 &&
          udp.peek()==-1 && udp.jwplcReadPacketFastDeferred(fast,2)==-1 &&
          !udp.jwplcCommitRxFast() && g_calls==callsBeforeStale,
          "B02_UDP_STALE_TOUCHED_HARDWARE");
    check(g_lockOwner==0 && g_takes==g_releases,"B02_BUS_LEAK");
    std::cout<<"G10_I_B02_UDP_STALE=PASS\n";
}
int main(){try{testCurrentOwner();return 0;}
    catch(const std::exception& e){
        std::cerr<<"G10_I_B02_UDP_NATIVE_ERROR="<<e.what()<<"\n";return 3;
    }
}

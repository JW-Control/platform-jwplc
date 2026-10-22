// Alpha13 G10-P1G-C2-S2C2B1: REAL EthernetUDP method bodies from overlay.
// Host-only W5500 simulation. No production changes and NO DNS semantics claim.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_spiOwner=0,g_takes=0,g_releases=0;
static int g_closes=0,g_starts=0,g_sendStarts=0,g_sendPolls=0,g_wrongOwnerClose=0;
static uint8_t g_status[MAX_SOCK_NUM]={};
static void check(bool ok,const char* why) {
    if (!ok) throw std::runtime_error(why);
}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if(g_spiOwner!=0)return false;
    g_spiOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_spiOwner==g_task,"RELEASE_BY_NONOWNER");
    g_spiOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_spiOwner==g_task,"DESELECT_WITHOUT_GUARD");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
struct IPAddress {
    uint32_t id=0;
    IPAddress()=default;
    explicit IPAddress(uint32_t x):id(x){}
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d)
       :id((uint32_t(a)<<24)|(uint32_t(b)<<16)|(uint32_t(c)<<8)|d){}
};
static uint8_t g_ipraw[4]={10,0,0,1};
static uint8_t* rawIPAddress(IPAddress) { return g_ipraw; }
struct SnMR {static constexpr uint8_t UDP=2;};
struct MockEthernet {
    uint8_t socketBegin(uint8_t,uint16_t) {
        check(ownedByThisTask(),"UDP_BEGIN_WITHOUT_GUARD");
        for(uint8_t s=0;s<MAX_SOCK_NUM;s++) if(g_status[s]==0){
            g_status[s]=1;
            auto t=allocateSocketLockedAuto(s);
            check(bool(t),"UDP_ALLOCATE_LEASE_FAILED");
            return s;
        }
        return MAX_SOCK_NUM;
    }
    void socketClose(uint8_t s) {
        check(ownedByThisTask(),"UDP_CLOSE_WITHOUT_GUARD");
        if(!bool(currentSocketTokenLocked(s)))++g_wrongOwnerClose;
        ++g_closes;
        g_status[s]=0;
    }
    int socketStartUDP(uint8_t s,const uint8_t*,uint16_t) {
        check(ownedByThisTask(),"UDP_START_PACKET_WITHOUT_GUARD");
        check(bool(currentSocketTokenLocked(s)),"UDP_START_WITHOUT_TOKEN");
        ++g_starts;
        return 1;
    }
    int socketBeginSendUDP(uint8_t s) {
        check(ownedByThisTask(),"UDP_SEND_BEGIN_WITHOUT_GUARD");
        check(bool(currentSocketTokenLocked(s)),"UDP_SEND_WITHOUT_TOKEN");
        ++g_sendStarts;
        return 0; // real backend: SEND launched, await poll
    }
    int socketPollSendUDP(uint8_t s) {
        check(ownedByThisTask(),"UDP_SEND_POLL_WITHOUT_GUARD");
        check(bool(currentSocketTokenLocked(s)),"UDP_POLL_WITHOUT_TOKEN");
        ++g_sendPolls;
        return 1;
    }
};
static MockEthernet Ethernet;
class EthernetUDP {
public:
    uint8_t begin(uint16_t port);
    void stop();
    int beginPacket(IPAddress ip,uint16_t port);
    int beginEndPacketAsync();
    int pollEndPacketAsync();
    void cancelEndPacketAsync();
    uint8_t sockindex=MAX_SOCK_NUM;
    uint16_t _port=0,_remaining=0,_offset=0,_remotePort=0;
    bool _sendPending=false;
    bool _jwplcDeferredSendStart=false;
    IPAddress _remoteIP{};
    SocketToken _jwplcSocketToken{};
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static void checkNoLockLeak(){
    check(g_spiOwner==0&&g_takes==g_releases,"LOCK_LEAK");
}
static void testOpenStartSendPoll(){
    EthernetUDP udp;
    check(udp.begin(53)==1 && udp.sockindex==0,"UDP_BEGIN_FAILED");
    {BusScope guard(20);
     check(bool(guard)&&matchesLocked(udp._jwplcSocketToken),
           "UDP_BEGIN_NO_TOKEN");
    }
    check(udp.beginPacket(IPAddress(30),53)==1,"UDP_PACKET_START_FAILED");
    check(udp.beginEndPacketAsync()==0&&udp._sendPending&&g_sendStarts==1,
          "UDP_SEND_NOT_QUEUED");
    check(udp.pollEndPacketAsync()==1&&!udp._sendPending&&g_sendPolls==1,
          "UDP_SEND_NOT_POLLED");
    udp.stop();
    check(g_closes==1 && udp.sockindex==MAX_SOCK_NUM,"UDP_STOP_MISSING_CLOSE");
    {BusScope guard(20);
     check(bool(guard)&&!matchesLocked(udp._jwplcSocketToken),
           "UDP_STOP_NO_INVALIDATION");
    }
    checkNoLockLeak();
    std::cout<<"C2_S2C2B1_UDP_BEGIN_SEND_POLL_STOP=PASS\n";
}
static void testBusyBeginAndStop(){
    EthernetUDP udp;
    check(udp.begin(1068)==1,"UDP_BUSY_STOP_SETUP");
    const auto idx=udp.sockindex;
    const int closes=g_closes;
    g_task=2;check(jwplcSPI_acquire(0),"BUSY_SETUP");g_task=1;
    udp.stop();
    check(udp.sockindex==idx && g_closes==closes && udp._port==1068,
          "UDP_BUSY_STOP_LOST_HANDLE");
    check(udp.begin(1069)==0 && udp.sockindex==idx && g_closes==closes,
          "UDP_BUSY_BEGIN_LOST_HANDLE");
    g_task=2;jwplcSPI_release();g_task=1;
    udp.stop();
    check(udp.sockindex==MAX_SOCK_NUM&&g_closes==closes+1,
          "UDP_STOP_RETRY_FAILED");
    checkNoLockLeak();
    std::cout<<"C2_S2C2B1_UDP_BUSY_NO_HANDLE_LOSS=PASS\n";
}
static void testStaleUdpCopy(){
    EthernetUDP udp;
    check(udp.begin(1053)==1,"UDP_STALE_SETUP");
    EthernetUDP alias=udp;
    const auto old=udp._jwplcSocketToken;
    const auto idx=udp.sockindex;
    int closed=g_closes;
    {BusScope guard(20);
     check(bool(guard),"UDP_STALE_REUSE_GUARD");
     auto newer=allocateSocketLocked(idx,0xBAD);
     check(bool(newer)&&!matchesLocked(old),"UDP_STALE_REUSE_BAD_LEASE");
    }
    alias.stop();
    check(g_closes==closed&&alias.sockindex==MAX_SOCK_NUM,
          "UDP_STALE_CLOSED_NEW_OWNER");
    check(udp.beginPacket(IPAddress(31),53)==0 &&
          g_starts==1,"UDP_STALE_PACKET_TOUCHED_OTHER_OWNER");
    checkNoLockLeak();
    std::cout<<"C2_S2C2B1_UDP_STALE_OWNER_REJECTED=PASS\n";
}
static void testDeferredUdpSend(){
    // Hardware model reset between tests; no GPIO4 exercised.
    {BusScope guard(20);
     check(bool(guard),"UDP_RESET_SETUP");
     check(resetAndInvalidate([](void*){return true;},nullptr,20),
           "UDP_LEASE_RESET_FAILED");
    }
    for(auto& x:g_status)x=0;
    EthernetUDP udp;
    check(udp.begin(666)==1,"UDP_DEFERRED_OPEN");
    check(udp.beginPacket(IPAddress(40),53)==1,"UDP_DEFERRED_PACKET");
    const auto sock=udp.sockindex;
    int before=g_sendStarts;
    g_task=2;check(jwplcSPI_acquire(0),"UDP_SEND_BUSY_SETUP");g_task=1;
    check(udp.beginEndPacketAsync()==0 && udp._sendPending &&
          udp._jwplcDeferredSendStart && g_sendStarts==before,
          "UDP_BUSY_SEND_WAS_NOT_DEFERRED");
    check(udp.pollEndPacketAsync()==0 && udp.sockindex==sock &&
          udp._jwplcDeferredSendStart && g_sendStarts==before,
          "UDP_DEFERRED_POLL_TOUCHES_BUSY_BUS");
    g_task=2;jwplcSPI_release();g_task=1;
    check(udp.pollEndPacketAsync()==0 &&
          udp._sendPending && !udp._jwplcDeferredSendStart &&
          g_sendStarts==before+1,
          "UDP_DEFERRED_SEND_NOT_STARTED");
    check(udp.pollEndPacketAsync()==1 && !udp._sendPending,
          "UDP_DEFERRED_SEND_NOT_FINISHED");
    checkNoLockLeak();
    std::cout<<"C2_S2C2B1_UDP_BUSY_SEND_DEFERRED=PASS\n";
}
static void testBusyStartPacketAndResetStaleSend(){
    EthernetUDP udp;
    check(udp.begin(703)==1,"UDP_RESET_SETUP_OPEN");
    const int before=g_starts;
    g_task=2;check(jwplcSPI_acquire(0),"UDP_PACKET_BUSY_SETUP");g_task=1;
    check(udp.beginPacket(IPAddress(50),53)==0 && g_starts==before,
          "UDP_BEGIN_PACKET_BUSY_SENT_TO_W5500");
    g_task=2;jwplcSPI_release();g_task=1;
    check(udp.beginPacket(IPAddress(50),53)==1,"UDP_PACKET_RETRY_FAILED");
    check(udp.beginEndPacketAsync()==0,"UDP_PENDING_SETUP_FAILED");
    const auto closes=g_closes,polls=g_sendPolls;
    check(resetAndInvalidate([](void*){return true;},nullptr,20),
          "UDP_RESET_INVALIDATE_FAILED");
    check(udp.pollEndPacketAsync()==-1 && g_sendPolls==polls &&
          udp.sockindex==MAX_SOCK_NUM,
          "UDP_STALE_SEND_AFTER_RESET_TOUCHED_HARDWARE");
    udp.stop();
    check(g_closes==closes,"UDP_STOP_STALE_AFTER_RESET_CLOSED");
    checkNoLockLeak();
    std::cout<<"C2_S2C2B1_UDP_PACKET_BUSY_AND_RESET_STALE=PASS\n";
}
int main(){
    try{
        testOpenStartSendPoll();
        testBusyBeginAndStop();
        testStaleUdpCopy();
        testDeferredUdpSend();
        testBusyStartPacketAndResetStaleSend();
        check(g_wrongOwnerClose==0,"WRONG_OWNER_CLOSED");
        std::cout<<"C2_S2C2B1_NATIVE_RESULT=PASS_UDP_LIFECYCLE_SLICE_ADOPTION_BLOCKED\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"C2_S2C2B1_NATIVE_FAILURE="<<e.what()<<"\n";
        return 3;
    }
}

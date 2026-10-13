// G10-P1G-C2-S2B2B: C++ bodies extracted from patched real EthernetClient.cpp
// Tested against C1 actual BusScope / SocketToken; network and RTOS mocked.
// No Arduino integration, no product firmware changes, no physical GPIO4.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <cstring>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
#define ESP32 1

static int g_task=1,g_spiOwner=0,g_takes=0,g_releases=0;
static int g_connects=0,g_disconnects=0,g_closes=0,g_wrongClose=0;
static uint8_t g_sockStatus[8]={};
static uint8_t g_target[8]={};
static uint32_t g_hwOwner[8]={};
static uint32_t g_connectPort[8]={};
static uint32_t g_now=0;
static int g_dnsCalls=0;
static bool g_autoConnect=false;
static void check(bool b,const char* msg) {
    if(!b) throw std::runtime_error(msg);
}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if(g_spiOwner)return false;
    g_spiOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void) {
    check(g_spiOwner==g_task,"WRONG_SPI_RELEASE");
    g_spiOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_spiOwner==g_task,"DESELECT_WITHOUT_SPI_GUARD");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
class IPAddress {
public:
    explicit IPAddress(uint32_t value=0):value_(value){}
    bool operator==(const IPAddress& o)const{return value_==o.value_;}
    uint32_t value()const{return value_;}
private:
    uint32_t value_;
};
static uint8_t g_ipbuf[4]={10,0,0,2};
static uint32_t millis(){return g_now;}
static void delay(int ms){ g_now+=uint32_t(ms); if(g_autoConnect){ for(int i=0;i<8;i++) if(g_sockStatus[i]==0x15) g_sockStatus[i]=0x17; } }
static const uint8_t* rawIPAddress(IPAddress ip) {
    g_ipbuf[3]=uint8_t(ip.value()&0xff);
    return g_ipbuf;
}
struct SnSR {
    static constexpr uint8_t CLOSED=0, INIT=0x13, SYN_SENT=0x15;
    static constexpr uint8_t ESTABLISHED=0x17, CLOSE_WAIT=0x1C;
    static constexpr uint8_t FIN_WAIT=0x18;
};
struct SnMR {static constexpr uint8_t TCP=1;};
class MockEthernet {
public:
    IPAddress dnsServerIP(){check(!ownedByThisTask(),"DNS_LOOKUP_HOLDING_SPI_MUTEX");return IPAddress(1);}

    uint8_t socketStatus(uint8_t s) {
        check(ownedByThisTask(),"SOCKET_STATUS_UNLOCKED");
        return g_sockStatus[s];
    }
    void socketDisconnect(uint8_t s) {
        check(ownedByThisTask(),"SOCKET_DISCONNECT_UNLOCKED");
        check(bool(currentSocketTokenLocked(s)),"DISCONNECT_WITHOUT_LEASE");
        ++g_disconnects;g_sockStatus[s]=SnSR::FIN_WAIT;
    }
    void socketClose(uint8_t s) {
        check(ownedByThisTask(),"SOCKET_CLOSE_UNLOCKED");
        if(!bool(currentSocketTokenLocked(s))) ++g_wrongClose;
        ++g_closes;g_sockStatus[s]=SnSR::CLOSED;
    }
    uint8_t socketBegin(uint8_t,uint16_t) {
        check(ownedByThisTask(),"SOCKET_BEGIN_UNLOCKED");
        for(uint8_t i=0;i<MAX_SOCK_NUM;i++)if(g_sockStatus[i]==SnSR::CLOSED){
            g_sockStatus[i]=SnSR::INIT;
            auto t=allocateSocketLockedAuto(i);
            check(bool(t),"REAL_LEASE_ALLOCATION_FAILED");
            g_hwOwner[i]=t.ownerCookie;
            return i;
        }
        return MAX_SOCK_NUM;
    }
    void socketConnect(uint8_t s,const uint8_t*,uint16_t port) {
        check(ownedByThisTask(),"SOCKET_CONNECT_UNLOCKED");
        check(bool(currentSocketTokenLocked(s)),"CONNECT_WITHOUT_LEASE");
        ++g_connects;g_sockStatus[s]=SnSR::SYN_SENT;
        g_connectPort[s]=port;
    }
};
static MockEthernet Ethernet;
class DNSClient {
public:
    void begin(IPAddress){check(!ownedByThisTask(),"DNS_BEGIN_HOLDING_SPI_MUTEX");}
    int getHostByName(const char* h,IPAddress& out){
        check(!ownedByThisTask(),"DNS_LOOKUP_HOLDING_SPI_MUTEX");
        ++g_dnsCalls;
        if(h && std::strcmp(h,"ok.local")==0){out=IPAddress(21);return 1;}
        return 0;
    }
};
class EthernetClient{
public:
    int connect(IPAddress,uint16_t);
    int connect(const char*,uint16_t);
    int beginConnectAsync(IPAddress,uint16_t);
    int pollConnectAsync();
    bool connectAsyncInProgress();
    void cancelConnectAsync();
    void cancelWriteAsync();
    uint8_t _sockindex=MAX_SOCK_NUM;
    uint32_t _timeout=1000;
    bool _stopPending=false, _flushPending=false;
    uint32_t _stopStartedAtMs=0, _flushStartedAtMs=0;
    bool _jwplcDeferredConnect=false;
    IPAddress _jwplcDeferredIp;
    uint16_t _jwplcDeferredPort=0;
    SocketToken _jwplcSocketToken{};
    uint8_t _writeAsyncState=0;
    const uint8_t* _writeAsyncBuffer=nullptr;
    uint16_t _writeAsyncLength=0;
    uint32_t _writeAsyncStartedAtMs=0;
};

// El runner nativo reutiliza el compilador C2-S2B2A y su archivo .inc generado.
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static void checkNoLeak(){check(g_spiOwner==0&&g_takes==g_releases,"SPI_OWNER_LEAK");}
static void testDirectConnect(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(2),502)==0,"BEGIN_CONNECT_MUST_BE_PENDING");
    check(g_connects==1&&c._sockindex==0,"OUTBOUND_CONNECT_NOT_STARTED");
    check(bool(c._jwplcSocketToken),"NEW_CONNECT_HAS_NO_VALID_TOKEN");
    check(c.connectAsyncInProgress(),"CONNECT_SHOULD_BE_PENDING");
    // Contention with an ALLOCATED socket is different from deferred start:
    // poll must report pending, not a false link failure.
    g_task=2;check(jwplcSPI_acquire(0),"DIRECT_POLL_BUSY_SETUP");g_task=1;
    check(c.pollConnectAsync()==0&&c._sockindex==0,
          "BUS_BUSY_POLL_MISREPORTED_DISCONNECTED");
    g_task=2;jwplcSPI_release();g_task=1;
    g_sockStatus[0]=SnSR::ESTABLISHED;
    check(c.pollConnectAsync()==1,"POLL_CONNECTED_FAILED");
    check(!c.connectAsyncInProgress(),"CONNECTED_STILL_IN_PROGRESS");
    checkNoLeak();
    std::cout<<"C2_S2B2B_OUTBOUND_CONNECT_POLL=PASS\n";
}
static void testContentionStartAndPoll(){
    EthernetClient c;
    g_task=2;check(jwplcSPI_acquire(0),"BUSY_SETUP");g_task=1;
    int connects=g_connects;
    check(c.beginConnectAsync(IPAddress(3),1502)==0 &&
          c._jwplcDeferredConnect&&c._sockindex==MAX_SOCK_NUM &&
          g_connects==connects,"BUS_BUSY_DEFERRED_START_LOST");
    check(c.connectAsyncInProgress(),"DEFERRED_CONNECT_NOT_IN_PROGRESS");
    check(c.pollConnectAsync()==0&&c._jwplcDeferredConnect&&
          g_connects==connects,"BUS_BUSY_POLL_MISREPORTED_DISCONNECTED");
    g_task=2;jwplcSPI_release();g_task=1;
    check(c.pollConnectAsync()==0 && !c._jwplcDeferredConnect &&
          bool(c._jwplcSocketToken)&&g_connects==connects+1,
          "DEFERRED_RETRY_NOT_STARTED");
    g_sockStatus[c._sockindex]=SnSR::ESTABLISHED;
    check(c.pollConnectAsync()==1,"DEFERRED_CONNECTED_NOT_RECOGNIZED");
    checkNoLeak();
    std::cout<<"C2_S2B2B_SPI_BUSY_START_AND_POLL=PASS\n";
}
static void testRetargetBusyAndStaleCopy(){
    EthernetClient old;
    check(old.beginConnectAsync(IPAddress(4),3000)==0,"OLD_CONNECT_SETUP");
    g_sockStatus[old._sockindex]=SnSR::ESTABLISHED;
    EthernetClient alias=old;
    const uint8_t first=old._sockindex;
    g_task=2;check(jwplcSPI_acquire(0),"BUSY_RETARGET_SETUP");g_task=1;
    int disc=g_disconnects;
    check(old.beginConnectAsync(IPAddress(5),3001)==0 &&
          old._jwplcDeferredConnect &&
          g_disconnects==disc,"BUSY_RETARGET_DROPPED_ACTIVE_OWNER");
    g_task=2;jwplcSPI_release();g_task=1;
    check(old.pollConnectAsync()==0 && !old._jwplcDeferredConnect &&
          old._sockindex!=first && g_disconnects==disc+1,
          "RETARGET_NOT_NEW_SOCKET");
    const int prevClose=g_closes,prevDisc=g_disconnects;
    alias.cancelConnectAsync();
    check(alias._sockindex==MAX_SOCK_NUM&&g_disconnects==prevDisc&&
          g_closes==prevClose,"STALE_CANCEL_CLOSED_NEW_OWNER");
    checkNoLeak();
    std::cout<<"C2_S2B2B_RETARGET_AND_STALE_ALIAS=PASS\n";
}
static void testCancelBusyAndOwnerReuse(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(6),2000)==0,"CANCEL_SETUP");
    uint8_t idx=c._sockindex;
    g_task=2;check(jwplcSPI_acquire(0),"BUSY_CANCEL_SETUP");g_task=1;
    int closes=g_closes;
    c.cancelConnectAsync();
    check(c._sockindex==idx && g_closes==closes,"BUSY_CANCEL_LOST_HANDLE");
    g_task=2;jwplcSPI_release();g_task=1;
    c.cancelConnectAsync();
    check(c._sockindex==MAX_SOCK_NUM&&g_closes==closes+1,
          "CANCEL_AFTER_BUSY_NOT_CLOSED");
    EthernetClient stale;
    stale._sockindex=idx;
    stale._jwplcSocketToken=c._jwplcSocketToken;
    const int disc=g_disconnects;
    stale.cancelConnectAsync();
    check(g_closes==closes+1&&g_disconnects==disc,
          "STALE_CANCEL_CLOSED_NEW_OWNER");
    checkNoLeak();
    std::cout<<"C2_S2B2B_CANCEL_BUSY_RETRY_OWNER=PASS\n";
}
static bool resetPhysical(void*){check(ownedByThisTask(),"RESET_NOT_LOCKED");return true;}
static void testResetAndInvalidIp(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(7),502)==0,"RESET_SETUP_FAILED");
    const int before=g_closes,disc=g_disconnects;
    check(resetAndInvalidate(resetPhysical,nullptr,20),"RESET_INTERLOCK_FAILED");
    check(c.pollConnectAsync()==-1&&c._sockindex==MAX_SOCK_NUM,
          "RESET_LEASE_POLL_NOT_REJECTED");
    c.cancelConnectAsync();
    check(g_closes==before&&g_disconnects==disc,
          "RESET_STALE_SOCKET_TOUCHED");
    EthernetClient invalid;
    check(invalid.beginConnectAsync(IPAddress(0),502)==-1 &&
          invalid._sockindex==MAX_SOCK_NUM,"INVALID_IP_ACCEPTED");
    checkNoLeak();
    std::cout<<"C2_S2B2B_RESET_AND_INVALID_IP=PASS\n";
}
static void testTxCancelAfterSend() {
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(11),2701)==0,"TX_SEND_CONNECT_SETUP");
    const auto token=c._jwplcSocketToken;
    c._writeAsyncState=2;
    c._writeAsyncStartedAtMs=77;
    c._writeAsyncLength=5;
    const int before=g_closes;
    c.cancelWriteAsync();
    check(g_closes==before+1 && c._sockindex==MAX_SOCK_NUM &&
          c._writeAsyncState==0 && c._writeAsyncLength==0 &&
          c._writeAsyncStartedAtMs==0,
          "CLOSE_NOT_ISSUED_WHEN_SEND_PENDING");
    { BusScope bus(20);
      check(bool(bus)&&!matchesLocked(token),"LEASE_STILL_VALID_AFTER_CLOSE");
    }
    checkNoLeak();
    std::cout<<"C2_S2C1_CANCEL_AFTER_SEND_INVALIDATES_LEASE=PASS\n";
}
static void testBusyTxPreservesState(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(12),2702)==0,"TX_BUSY_SETUP");
    c._writeAsyncState=2;
    c._writeAsyncLength=9;
    const auto idx=c._sockindex;
    const int before=g_closes;
    g_task=2;check(jwplcSPI_acquire(0),"TX_SPI_BUSY_SETUP");g_task=1;
    c.cancelWriteAsync();
    check(c._writeAsyncState==2 && c._writeAsyncLength==9 &&
          c._sockindex==idx && g_closes==before,
          "BUS_BUSY_LOST_WRITE_STATE");
    g_task=2;jwplcSPI_release();g_task=1;
    c.cancelWriteAsync();
    check(c._writeAsyncState==0&&g_closes==before+1,
          "TX_RETRY_DID_NOT_CLOSE");
    checkNoLeak();
    std::cout<<"C2_S2C1_BUSY_TX_STATE_RETRY=PASS\n";
}
static void testStaleTxAliasAndUnsentCancel(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(13),2703)==0,"TX_STALE_SETUP");
    auto alias=c;
    alias._writeAsyncState=2;
    c._writeAsyncState=1;
    const auto slot=c._sockindex;
    c.cancelWriteAsync();
    check(c._writeAsyncState==0 &&
          c._sockindex==slot,"UNSENT_CANCEL_BROKE_SESSION");
    { BusScope bus(20);
      check(bool(bus),"TX_REUSE_LOCK");
      auto next=allocateSocketLocked(slot,900001);
      check(bool(next),"TX_REUSE_TOKEN");
    }
    const int before=g_closes;
    alias.cancelWriteAsync();
    check(g_closes==before && alias._sockindex==MAX_SOCK_NUM &&
          alias._writeAsyncState==0,
          "STALE_TX_CANCEL_CLOSED_NEW_OWNER");
    checkNoLeak();
    std::cout<<"C2_S2C1_STALE_TX_COPY_AND_UNSENT=PASS\n";
}
static void testRetargetWithActiveTx() {
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(14),2710)==0,"TX_RETARGET_SETUP");
    c._writeAsyncState=2;
    const auto previousToken=c._jwplcSocketToken;
    const int before=g_closes;
    check(c.beginConnectAsync(IPAddress(15),2711)==0,
          "RETARGET_WITH_SEND_PENDING_FAILED");
    // La nueva conexión puede legítimamente reutilizar el mismo índice:
    // lo importante es un lease DISTINTO y vigente bajo la misma generación.
    {
        BusScope bus(20);
        check(bool(bus)&&!matchesLocked(previousToken) &&
              matchesLocked(c._jwplcSocketToken) &&
              c._jwplcSocketToken.allocation!=previousToken.allocation &&
              g_closes==before+1 && c._writeAsyncState==0,
              "TX_RETARGET_LEAK_OR_DOUBLE_CLOSE");
    }
    checkNoLeak();
    std::cout<<"C2_S2C1_TX_RETARGET_NESTED_GUARD=PASS\n";
}
static void testBlockingIpSuccess(){
    // Separar las asignaciones de las pruebas outbound/TX precedentes:
    // en el mock reset físico limpia tanto tokens como hardware.
    check(resetAndInvalidate(resetPhysical,nullptr,20),"LEGACY_RESET_INTERLOCK");
    for(int i=0;i<8;i++)g_sockStatus[i]=SnSR::CLOSED;
    EthernetClient c;
    c._timeout=10;
    g_autoConnect=true;
    check(c.connect(IPAddress(23),502)==1,"BLOCKING_IP_CONNECT_SUCCESS_FAILED");
    g_autoConnect=false;
    check(c._sockindex<MAX_SOCK_NUM && bool(c._jwplcSocketToken),
          "BLOCKING_IP_SUCCESS_NO_TOKEN");
    checkNoLeak();
    std::cout<<"C2_S2C2A_IP_BLOCKING_SUCCESS=PASS\n";
}
static void testBlockingIpBusyTimeoutNoPhantomConnect(){
    EthernetClient c;
    c._timeout=3;
    const int before=g_connects;
    g_task=2;check(jwplcSPI_acquire(0),"BLOCK_IP_BUSY_SETUP");g_task=1;
    check(c.connect(IPAddress(31),502)==0,"BLOCK_IP_BUSY_TIMEOUT_SHOULD_FAIL");
    check(!c._jwplcDeferredConnect && c._sockindex==MAX_SOCK_NUM &&
          g_connects==before,"TIMED_OUT_DEFERRED_CONNECT_LEAKED");
    g_task=2;jwplcSPI_release();g_task=1;
    check(c.pollConnectAsync()==-1 && g_connects==before,
          "LATE_CONNECT_AFTER_BLOCKING_TIMEOUT");
    checkNoLeak();
    std::cout<<"C2_S2C2A_IP_BUSY_TIMEOUT_NO_PHANTOM=PASS\n";
}
static void testHostDnsAndSessionCleanup(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(41),1234)==0,"HOST_OLD_SETUP");
    g_sockStatus[c._sockindex]=SnSR::ESTABLISHED;
    const auto oldToken=c._jwplcSocketToken;
    const int startDisc=g_disconnects,startDns=g_dnsCalls;
    g_autoConnect=true;
    check(c.connect("ok.local",3210)==1,"HOST_CONNECT_DNS_FAILED");
    g_autoConnect=false;
    check(g_disconnects==startDisc+1 && g_dnsCalls==startDns+1,
          "HOST_PRE_DNS_CLEANUP_OR_DNS_MISSING");
    { BusScope bus(20);
      check(bool(bus) && !matchesLocked(oldToken) &&
            matchesLocked(c._jwplcSocketToken),
            "HOST_LEASE_AFTER_DNS_INVALID");
    }
    checkNoLeak();
    std::cout<<"C2_S2C2A_HOST_DNS_AND_OLD_OWNER=PASS\n";
}
static void testHostBusyPreservesSocket(){
    EthernetClient c;
    check(c.beginConnectAsync(IPAddress(42),1201)==0,"HOST_BUSY_SETUP");
    g_sockStatus[c._sockindex]=SnSR::ESTABLISHED;
    const int idx=c._sockindex,disc=g_disconnects,dns=g_dnsCalls;
    g_task=2;check(jwplcSPI_acquire(0),"HOST_BUSY_SPI_SETUP");g_task=1;
    check(c.connect("ok.local",1202)==0 &&
          c._sockindex==idx && g_disconnects==disc &&
          g_dnsCalls==dns,"HOST_BUSY_DROPPED_HANDLE_OR_RAN_DNS");
    g_task=2;jwplcSPI_release();g_task=1;
    checkNoLeak();
    std::cout<<"C2_S2C2A_HOST_BUSY_NO_DESTRUCTIVE_DNS=PASS\n";
}
static void testHostStaleAndDnsFailure(){
    EthernetClient owner;
    check(owner.beginConnectAsync(IPAddress(43),1203)==0,"HOST_STALE_SETUP");
    g_sockStatus[owner._sockindex]=SnSR::ESTABLISHED;
    EthernetClient alias=owner;
    {BusScope bus(20);check(bool(bus),"HOST_STALE_REUSE_LOCK");
     auto t=allocateSocketLocked(owner._sockindex,999999);
     check(bool(t),"HOST_STALE_REUSE_FAIL");}
    const int before=g_disconnects,dns=g_dnsCalls;
    check(alias.connect("nonexistent.local",502)==0,
          "HOST_BAD_DNS_DID_NOT_FAIL");
    check(alias._sockindex==MAX_SOCK_NUM &&
          g_disconnects==before && g_dnsCalls==dns+1,
          "HOST_STALE_PRE_DNS_TOUCHED_WRONG_OWNER");
    checkNoLeak();
    std::cout<<"C2_S2C2A_STALE_HOST_AND_DNS_FAILURE=PASS\n";
}
int main(){
    try{
        testDirectConnect();
        testContentionStartAndPoll();
        testRetargetBusyAndStaleCopy();
        testCancelBusyAndOwnerReuse();
        testResetAndInvalidIp();
        testTxCancelAfterSend();
        testBusyTxPreservesState();
        testStaleTxAliasAndUnsentCancel();
        testRetargetWithActiveTx();
        testBlockingIpSuccess();
        testBlockingIpBusyTimeoutNoPhantomConnect();
        testHostDnsAndSessionCleanup();
        testHostBusyPreservesSocket();
        testHostStaleAndDnsFailure();
        check(g_wrongClose==0,"WRONG_SOCKET_CLOSED");
        std::cout<<"C2_S2C2A_NATIVE_RESULT=PASS_IP_HOST_PRE_DNS_SLICE_ADOPTION_BLOCKED\n";
        return 0;
    }catch(const std::exception& error){
        std::cerr<<"C2_S2B2B_NATIVE_FAILURE="<<error.what()<<"\n";
        return 3;
    }
}

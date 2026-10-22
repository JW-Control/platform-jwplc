// B03: extracted C++ control paths plus the real SafeAccess implementation.
// Hardware/bus are stubs; assertions check the lease, not a mock DNS result.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
#define MAX_SOCK_NUM 8
static int g_task=1,g_spiOwner=0,g_takes=0,g_releases=0,g_closes=0;
static bool g_hwOpen[MAX_SOCK_NUM]={};
static uint32_t g_time=4123;
static uint8_t g_dhcpResponse=0;
static int g_dnsPacketSize=0;
static bool g_dnsFailBuildAndHold=false;
static void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
extern "C" bool jwplcSPI_acquire(uint32_t) {
    if(g_spiOwner!=0)return false;
    g_spiOwner=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void){
    check(g_spiOwner==g_task,"WRONG_TASK_RELEASE");
    g_spiOwner=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void){
    check(g_spiOwner==g_task,"DESELECT_WITHOUT_OWNER");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static uint32_t millis(){return g_time;}
static unsigned long random(unsigned long, unsigned long){return 42;}
struct IPAddress{
    uint8_t bytes[4]={0,0,0,0};
    IPAddress()=default;
    IPAddress(uint8_t a,uint8_t b,uint8_t c,uint8_t d):bytes{a,b,c,d}{}
    bool operator==(const IPAddress& other) const {
        for(int i=0;i<4;++i)if(bytes[i]!=other.bytes[i])return false;
        return true;
    }
};
static const IPAddress INADDR_NONE(255,255,255,255);
namespace SnMR {constexpr uint8_t UDP=2,MULTI=128;}
constexpr int INVALID_RESPONSE=-4,SUCCESS=1,INVALID_SERVER=-2,TIMED_OUT=-1;
constexpr uint16_t DNS_PORT=53,DHCP_CLIENT_PORT=68;
constexpr uint8_t STATE_DHCP_START=0,STATE_DHCP_DISCOVER=1,
                  STATE_DHCP_REQUEST=2,STATE_DHCP_LEASED=3,
                  STATE_DHCP_REREQUEST=4;
constexpr uint8_t DHCP_DISCOVER=1,DHCP_OFFER=2,DHCP_REQUEST=3,
                  DHCP_ACK=5,DHCP_NAK=6;
struct MockEthernet {
    void socketClose(uint8_t s){
        check(ownedByThisTask(),"B03_HW_CLOSE_UNGUARDED");
        check(s<MAX_SOCK_NUM&&g_hwOpen[s],"B03_DOUBLE_OR_UNKNOWN_CLOSE");
        g_hwOpen[s]=false;++g_closes;
    }
    uint8_t socketBegin(uint8_t,uint16_t){
        check(ownedByThisTask(),"B03_BEGIN_WITHOUT_BUS");
        for(uint8_t s=0;s<MAX_SOCK_NUM;++s)if(!g_hwOpen[s]){
            g_hwOpen[s]=true;
            (void)allocateSocketLockedAuto(s);
            return s;
        }
        return MAX_SOCK_NUM;
    }
    uint8_t socketBeginMulticast(uint8_t protocol,IPAddress,uint16_t port){
        return socketBegin(protocol,port);
    }
} Ethernet;
struct EthernetUDP{
    uint8_t sockindex=MAX_SOCK_NUM;
    uint16_t _port=0,_remaining=0,_offset=0,_remotePort=0;
    bool _sendPending=false,_jwplcDeferredSendStart=false;
    IPAddress _remoteIP;
    SocketToken _jwplcSocketToken{};
    uint8_t begin(uint16_t port);
    uint8_t beginMulticast(IPAddress ip,uint16_t port);
    void stop();
    int beginPacket(IPAddress,uint16_t){return 1;}
    int beginEndPacketAsync(){return 1;}
    int pollEndPacketAsync(){return 1;}
    int parsePacket(){return g_dnsPacketSize;}
};
struct DNSClient {
    EthernetUDP iUdp;
    IPAddress iDNSServer{8,8,8,8};
    IPAddress* iAsyncResult=nullptr;
    uint16_t iAsyncTimeout=0;
    uint32_t iAsyncWaitStartMs=0;
    uint8_t iAsyncWaitAttempt=0;
    int iAsyncStatus=0;
    bool iAsyncActive=false,iAsyncSendPending=false;
    int inet_aton(const char*,IPAddress&){return 0;}
    uint16_t BuildRequest(const char*){
        if (!g_dnsFailBuildAndHold) return 1;
        g_task=2;
        check(jwplcSPI_acquire(0),"DNS_BUILD_LOCK_FAILED");
        g_task=1;
        return 0;
    }
    int beginResolveAsync(const char*,IPAddress&,uint16_t);
    int pollResolveAsync();
    int ProcessResponsePacket(IPAddress&){return SUCCESS;}
    void finishResolveAsync(int result);
    void cancelResolveAsync();
};
struct DhcpClass{
    EthernetUDP _dhcpUdpSocket;
    uint32_t _dhcpTransactionId=0;
    uint32_t _dhcpInitialTransactionId=0,_dhcpLeaseTime=0,_dhcpT1=0,_dhcpT2=0;
    uint32_t _lastCheckLeaseMillis=0;
    uint8_t _dhcpMacAddr[6]={};
    uint8_t _dhcp_state=0;
    unsigned long _timeout=0,_responseTimeout=0,_asyncStartTime=0,_asyncStateStartTime=0;
    bool _asyncActive=false;
    enum : uint8_t {LEASE_MAINT_NONE=0,LEASE_MAINT_RENEW=1,LEASE_MAINT_REBIND=2};
    uint8_t _leaseMaintenanceMode=LEASE_MAINT_NONE;
    unsigned long _leaseRetryNotBeforeMs=0;
    void reset_DHCP_lease(){}
    void presend_DHCP(){}
    void send_DHCP_MESSAGE(uint8_t,uint16_t){}
    uint8_t parseDHCPResponseAvailable(uint32_t& id){
        id=77;const uint8_t response=g_dhcpResponse;g_dhcpResponse=0;
        return response;
    }
    uint8_t parseDHCPResponse(unsigned long,uint32_t& id){
        id=77;
        return _dhcp_state==STATE_DHCP_DISCOVER ? DHCP_OFFER : DHCP_ACK;
    }
    void finalizeLease(){}
    int pollDHCP();
    int request_DHCP_lease();
    int beginWithDHCP(uint8_t*,unsigned long,unsigned long);
    int beginWithDHCPAsync(uint8_t*,unsigned long,unsigned long);
    bool startLeaseMaintenance(uint8_t);
    void finishAsync();
    void cancelDHCP();
};
#include "C2_S2B2A_REAL_PATCHED_METHODS.inc"

static void closeTicket(uint8_t s,void*){Ethernet.socketClose(s);}
static void tick(){(void)serviceDeferredClose(closeTicket,nullptr,0);}
static void bind(EthernetUDP& udp,uint8_t s){
    BusScope bus(20);
    check(bool(bus),"SETUP_BUS_REQUIRED");
    udp.sockindex=s;
    udp._jwplcSocketToken=allocateSocketLockedAuto(s);
    check(bool(udp._jwplcSocketToken),"SETUP_TOKEN_REQUIRED");
    g_hwOpen[s]=true;
}
static void holdByOther(){
    g_task=2;
    check(jwplcSPI_acquire(0),"SETUP_LOCK_FAILED");
    g_task=1;
}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static void testFinishAndLifetime(){
    const int before=g_closes;
    {
        DNSClient dns;
        bind(dns.iUdp,1);
        dns.iAsyncActive=true;
        holdByOther();
        dns.finishResolveAsync(1);
        check(dns.iUdp.sockindex==1 && g_hwOpen[1] && !dns.iAsyncActive,
              "DNS_BUSY_LOST_OWNER");
        tick();
        check(g_closes==before,"DNS_CLOSED_DURING_SPI_BUSY");
    } // DNS/UDP lifetime ends before the next network tick.
    releaseOther();
    tick();
    check(!g_hwOpen[1]&&g_closes==before+1,"DNS_FINISH_TICKET_NOT_DRAINED");
    tick();
    check(g_closes==before+1,"DNS_DOUBLE_CLOSE");
    std::cout<<"G10_I_B03_DNS_FINISH_LIFETIME=PASS\n";
}
static void testCancelRepeatedBusy(){
    const int before=g_closes;
    DNSClient dns;
    bind(dns.iUdp,2);
    dns.iAsyncActive=true;
    holdByOther();
    dns.cancelResolveAsync();
    dns.iUdp.stop();
    tick();tick();
    check(dns.iUdp.sockindex==2 && g_hwOpen[2] &&
          !dns.iAsyncActive && dns.iAsyncStatus==INVALID_RESPONSE &&
          g_closes==before,"DNS_CANCEL_BUSY_LOST_OWNER");
    releaseOther();
    tick();
    check(!g_hwOpen[2]&&g_closes==before+1,"DNS_CANCEL_NO_IDLE_DRAIN");
    dns.iUdp.stop(); // stale local token cannot close twice.
    check(g_closes==before+1,"DNS_CANCEL_DOUBLE_CLOSE");
    std::cout<<"G10_I_B03_DNS_CANCEL_IDLE_RETRY=PASS\n";
}
static void testDhcpFinish(){
    const int before=g_closes;
    DhcpClass dhcp;
    bind(dhcp._dhcpUdpSocket,3);
    dhcp._asyncActive=true;
    holdByOther();
    dhcp.finishAsync();
    check(!dhcp._asyncActive && g_hwOpen[3] &&
          dhcp._lastCheckLeaseMillis==g_time,"DHCP_FINISH_BUSY_STATE");
    releaseOther();
    tick();
    check(!g_hwOpen[3]&&g_closes==before+1,"DHCP_FINISH_NO_IDLE_DRAIN");
    std::cout<<"G10_I_B03_DHCP_FINISH_IDLE_RETRY=PASS\n";
}
static void testDnsTimeout(){
    DNSClient dns;
    IPAddress result;
    bind(dns.iUdp,1);
    dns.iAsyncActive=true;
    dns.iAsyncResult=&result;
    dns.iAsyncTimeout=1;
    dns.iAsyncWaitStartMs=g_time-2;
    dns.iAsyncWaitAttempt=3;
    holdByOther();
    check(dns.pollResolveAsync()==TIMED_OUT && !dns.iAsyncActive &&
          dns.iUdp.sockindex==1 && g_hwOpen[1],"DNS_TIMEOUT_LOST_OWNER");
    releaseOther();
    tick();
    check(!g_hwOpen[1],"DNS_TIMEOUT_NOT_DRAINED");
    std::cout<<"G10_I_B03_DNS_TIMEOUT=PASS\n";
}
static void testDnsBuildError(){
    DNSClient dns;
    IPAddress result;
    g_dnsFailBuildAndHold=true;
    check(dns.beginResolveAsync("invalid.example",result,100)==-11,
          "DNS_BUILD_ERROR_NOT_RETURNED");
    g_dnsFailBuildAndHold=false;
    const uint8_t socket=dns.iUdp.sockindex;
    check(socket<MAX_SOCK_NUM && g_hwOpen[socket] && !dns.iAsyncActive,
          "DNS_BUILD_ERROR_LOST_OWNER");
    releaseOther();tick();
    check(!g_hwOpen[socket],"DNS_BUILD_ERROR_NOT_DRAINED");
    std::cout<<"G10_I_B03_DNS_BUILD_ERROR=PASS\n";
}
static void testDhcpTimeoutAckNakCancel(){
    DhcpClass timed;
    bind(timed._dhcpUdpSocket,1);
    timed._asyncActive=true;
    timed._asyncStartTime=g_time-3;
    timed._timeout=1;
    holdByOther();
    check(timed.pollDHCP()==-1 && !timed._asyncActive &&
          timed._dhcpUdpSocket.sockindex==1 && g_hwOpen[1],
          "DHCP_TIMEOUT_LOST_OWNER");
    releaseOther();tick();
    check(!g_hwOpen[1],"DHCP_TIMEOUT_NOT_DRAINED");

    DhcpClass ack;
    bind(ack._dhcpUdpSocket,2);
    ack._asyncActive=true;
    ack._asyncStartTime=g_time;
    ack._timeout=100;
    ack._dhcp_state=STATE_DHCP_REQUEST;
    g_dhcpResponse=DHCP_ACK;
    holdByOther();
    check(ack.pollDHCP()==1 && !ack._asyncActive &&
          ack._dhcp_state==STATE_DHCP_LEASED && g_hwOpen[2],
          "DHCP_ACK_LOST_OWNER");
    releaseOther();tick();
    check(!g_hwOpen[2],"DHCP_ACK_NOT_DRAINED");

    DhcpClass nak;
    bind(nak._dhcpUdpSocket,3);
    nak._asyncActive=true;
    nak._asyncStartTime=g_time;
    nak._timeout=100;
    nak._dhcp_state=STATE_DHCP_REQUEST;
    g_dhcpResponse=DHCP_NAK;
    holdByOther();
    check(nak.pollDHCP()==0 && nak._asyncActive &&
          nak._dhcp_state==STATE_DHCP_START && g_hwOpen[3],
          "DHCP_NAK_FLOW_INVALID");
    nak.cancelDHCP();
    check(!nak._asyncActive && nak._leaseMaintenanceMode==DhcpClass::LEASE_MAINT_NONE &&
          nak._dhcpUdpSocket.sockindex==3 && g_hwOpen[3],
          "DHCP_CANCEL_LOST_OWNER");
    releaseOther();tick();
    check(!g_hwOpen[3],"DHCP_CANCEL_NOT_DRAINED");
    std::cout<<"G10_I_B03_DHCP_TIMEOUT_ACK_NAK_CANCEL=PASS\n";
}
static void testSynchronousDhcpRebegin(){
    uint8_t mac[6]={1,2,3,4,5,6};
    DhcpClass dhcp;
    bind(dhcp._dhcpUdpSocket,1);
    holdByOther();
    check(dhcp.beginWithDHCP(mac,100,10)==0 &&
          dhcp._dhcpUdpSocket.sockindex==1 && g_hwOpen[1],
          "DHCP_SYNC_REBEGIN_REUSED_BUSY_SOCKET");
    releaseOther();tick();
    check(!g_hwOpen[1],"DHCP_SYNC_TICKET_NOT_DRAINED");
    check(dhcp.beginWithDHCP(mac,100,10)==1,
          "DHCP_SYNC_RETRY_FAILED");
    std::cout<<"G10_I_B03_DHCP_SYNC_REBEGIN=PASS\n";
}
static void testDnsAndDhcpRebegin(){
    DNSClient dns;
    bind(dns.iUdp,5);
    dns.iAsyncActive=true;
    IPAddress result;
    holdByOther();
    check(dns.beginResolveAsync("example.org",result,100)==-11 &&
          dns.iUdp.sockindex==5 && g_hwOpen[5],
          "DNS_REBEGIN_REUSED_BUSY_SOCKET");
    releaseOther();
    tick();
    check(!g_hwOpen[5],"DNS_REBEGIN_TICKET_NOT_DRAINED");
    check(dns.beginResolveAsync("example.org",result,100)==0 &&
          dns.iAsyncActive,"DNS_REBEGIN_AFTER_DRAIN_FAILED");
    dns.cancelResolveAsync();

    uint8_t mac[6]={1,2,3,4,5,6};
    DhcpClass dhcp;
    bind(dhcp._dhcpUdpSocket,6);
    holdByOther();
    check(dhcp.beginWithDHCPAsync(mac,200,50)==-1 &&
          dhcp._dhcpUdpSocket.sockindex==6 && g_hwOpen[6],
          "DHCP_REBEGIN_REUSED_BUSY_SOCKET");
    releaseOther();
    tick();
    check(!g_hwOpen[6],"DHCP_REBEGIN_TICKET_NOT_DRAINED");
    check(dhcp.beginWithDHCPAsync(mac,200,50)==0 &&
          dhcp._asyncActive,"DHCP_REBEGIN_AFTER_DRAIN_FAILED");
    dhcp.finishAsync();
    std::cout<<"G10_I_B03_DNS_DHCP_REBEGIN=PASS\n";
}
static void testRenewRebindBusy(){
    uint8_t modes[2]={DhcpClass::LEASE_MAINT_RENEW,DhcpClass::LEASE_MAINT_REBIND};
    for(uint8_t mode:modes){
        DhcpClass dhcp;
        bind(dhcp._dhcpUdpSocket,7);
        holdByOther();
        check(!dhcp.startLeaseMaintenance(mode) &&
              dhcp._dhcpUdpSocket.sockindex==7 && g_hwOpen[7],
              "DHCP_MAINT_REUSED_BUSY_SOCKET");
        releaseOther();
        tick();
        check(!g_hwOpen[7],"DHCP_MAINT_TICKET_NOT_DRAINED");
        check(dhcp.startLeaseMaintenance(mode) && dhcp._asyncActive &&
              dhcp._leaseMaintenanceMode==mode,
              "DHCP_MAINT_RETRY_FAILED");
        dhcp.finishAsync();
    }
    std::cout<<"G10_I_B03_DHCP_RENEW_REBIND=PASS\n";
}
static void testMulticastDoesNotReuseBusyOwner(){
    EthernetUDP udp;
    bind(udp,5);
    holdByOther();
    check(udp.beginMulticast(IPAddress(239,1,2,3),32001)==0 &&
          udp.sockindex==5 && g_hwOpen[5],"MULTICAST_BUSY_LOST_OWNER");
    udp.stop();
    releaseOther();
    tick();
    check(!g_hwOpen[5],"MULTICAST_PENDING_NOT_DRAINED");
    check(udp.beginMulticast(IPAddress(239,1,2,3),32001)==1 &&
          bool(udp._jwplcSocketToken),"MULTICAST_LEASE_NOT_ALLOCATED");
    udp.stop();
    std::cout<<"G10_I_B03_MULTICAST_BUSY_REUSE=PASS\n";
}
static void testRecycledAndReset(){
    const int before=g_closes;
    DNSClient dns;
    bind(dns.iUdp,4);
    holdByOther();
    dns.cancelResolveAsync();
    releaseOther();
    {
        BusScope bus(20);
        check(bool(bus),"RECYCLE_LOCK");
        // Simulate an intervening owner. A pending old allocation must not
        // close the new socket at the same hardware index.
        (void)allocateSocketLockedAuto(4);
    }
    tick();
    check(g_hwOpen[4]&&g_closes==before,"RECYCLED_OWNER_CLOSED");
    {
        BusScope bus(20);
        check(bool(bus),"RESET_LOCK");
        check(resetAndInvalidate([](void*){return true;},nullptr,0),
              "RESET_FAILED");
        (void)allocateSocketLockedAuto(4);
    }
    tick();
    check(g_hwOpen[4]&&g_closes==before,"RESET_OWNER_CLOSED");
    std::cout<<"G10_I_B03_STALE_RESET_OWNER=PASS\n";
}
int main(){
    try{
        testFinishAndLifetime();testCancelRepeatedBusy();
        testDhcpFinish();testDnsTimeout();
        testDhcpTimeoutAckNakCancel();testSynchronousDhcpRebegin();
        testDnsAndDhcpRebegin();
        testRenewRebindBusy();testMulticastDoesNotReuseBusyOwner();
        testRecycledAndReset();testDnsBuildError();
        check(g_spiOwner==0&&g_takes==g_releases,"B03_MUTEX_LEAK");
        std::cout<<"G10_I_B03_NATIVE=PASS_SOURCE_BACKED_DEFERRED_CLOSE\n";
        return 0;
    }catch(const std::exception& e){
        std::cerr<<"G10_I_B03_NATIVE_ERROR="<<e.what()<<"\n";
        return 3;
    }
}

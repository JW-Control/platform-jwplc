// Appended to the existing real-client host boundary by the integrated gate.
static void resetScenario(){
    check(resetAndInvalidate([](void*)->bool{
        check(ownedByThisTask(),"B04_RESET_NO_BUS");
        for(auto& state:g_sockStatus)state=SnSR::CLOSED;
        return true;
    },nullptr,20),"B04_RESET_FAILED");
    (void)serviceDeferredClose(
        [](uint8_t index,void*){Ethernet.socketClose(index);},nullptr,20);
    g_connects=0;g_disconnects=0;g_closes=0;g_wrongClose=0;
    g_now=0;
}
static void holdOther(){g_task=2;check(jwplcSPI_acquire(0),"B04_BUSY_SETUP");g_task=1;}
static void releaseOther(){g_task=2;jwplcSPI_release();g_task=1;}
static uint8_t drain(){return serviceDeferredClose(
    [](uint8_t index,void*){Ethernet.socketClose(index);},nullptr,20);}
static void cancelledRetargetMustNotRestart(){
    resetScenario();EthernetClient client;
    check(client.beginConnectAsync(IPAddress(11),5001)==0,"B04_FIRST_CONNECT");
    const uint8_t old=client._sockindex;
    g_sockStatus[old]=SnSR::ESTABLISHED;
    holdOther();
    const int before=g_connects;
    check(client.beginConnectAsync(IPAddress(12),5002)==0 &&
          client._jwplcDeferredConnect,"B04_BUSY_RETARGET_NOT_QUEUED");
    client.cancelConnectAsync();
    check(client.pollConnectAsync()==0 && g_connects==before &&
          client.connectAsyncInProgress(),"B04_CANCEL_BUSY_POLL_LOST");
    releaseOther();
    check(client.connectAsyncInProgress(),
          "B04_CANCEL_PENDING_NOT_IN_PROGRESS");
    check(client.pollConnectAsync()==-1 && g_connects==before &&
          client._sockindex==MAX_SOCK_NUM,
          "B04_CANCELLED_RETARGET_RESTARTED");
    check(drain()==0 && g_sockStatus[old]==SnSR::FIN_WAIT,
          "B04_CANCELLED_RETARGET_TICKET_REACHED_NEW_OWNER");
    std::cout<<"G10_I_B04_CANCELLED_RETARGET=PASS\n";
}
static void recycledOwnerMustSurviveCancelRetry(){
    resetScenario();EthernetClient client;
    check(client.beginConnectAsync(IPAddress(21),6001)==0,"B04_RECYCLE_CONNECT");
    const uint8_t index=client._sockindex;
    const auto old=client._jwplcSocketToken;
    holdOther();client.cancelConnectAsync();releaseOther();
    {BusScope bus(20);check(bool(bus),"B04_RECYCLE_MUTEX");
     const auto newer=allocateSocketLockedAuto(index);
     check(bool(newer) && newer.allocation!=old.allocation,"B04_RECYCLE_TOKEN");}
    const int closes=g_closes,disconnects=g_disconnects;
    check(client.pollConnectAsync()==-1 && client._sockindex==MAX_SOCK_NUM &&
          drain()==0 && g_closes==closes && g_disconnects==disconnects &&
          g_sockStatus[index]==SnSR::SYN_SENT,
          "B04_RECYCLED_OWNER_TOUCHED");
    std::cout<<"G10_I_B04_RECYCLED_CANCEL=PASS\n";
}
static void explicitNewTargetSupersedesCancelledOne(){
    resetScenario();EthernetClient client;
    check(client.beginConnectAsync(IPAddress(31),7001)==0,"B04_NEW_TARGET_FIRST");
    holdOther();client.cancelConnectAsync();
    check(client.beginConnectAsync(IPAddress(32),7002)==0 &&
          client._jwplcDeferredConnect,"B04_NEW_TARGET_NOT_QUEUED");
    releaseOther();
    check(client.pollConnectAsync()==0 && g_connects==2 &&
          g_connectPort[client._sockindex]==7002 &&
          bool(client._jwplcSocketToken),"B04_NEW_TARGET_WAS_CANCELLED");
    const auto latest=client._jwplcSocketToken;
    check(drain()==0 && client.pollConnectAsync()==0,
          "B04_OLD_TICKET_CLOSED_NEW_TARGET");
    {BusScope bus(20);check(bool(bus)&&matchesLocked(latest),
          "B04_NEW_TARGET_LEASE_LOST");}
    std::cout<<"G10_I_B04_NEW_TARGET_SUPERSEDES=PASS\n";
}
static void timeoutWithoutSocketMustNotRestart(){
    resetScenario();EthernetClient client;client._timeout=3;
    holdOther();
    check(client.connect(IPAddress(41),8001)==0 && g_connects==0,
          "B04_BUSY_TIMEOUT_CONNECTED");
    releaseOther();
    check(client.pollConnectAsync()==-1 && !client.connectAsyncInProgress() &&
          g_connects==0,"B04_TIMEOUT_PHANTOM_CONNECT");
    std::cout<<"G10_I_B04_TIMEOUT_NO_PHANTOM=PASS\n";
}
static void timeoutWithOldSocketMustLeaveTicket(){
    resetScenario();
    uint8_t oldIndex=MAX_SOCK_NUM;
    {
        EthernetClient client;client._timeout=3;
        check(client.beginConnectAsync(IPAddress(51),9001)==0,
              "B04_TIMEOUT_OLD_CONNECT");
        oldIndex=client._sockindex;
        const int before=g_connects;
        holdOther();
        check(client.connect(IPAddress(52),9002)==0 &&
              g_connects==before && client._jwplcCancelConnectPending,
              "B04_TIMEOUT_OLD_TICKET_MISSING");
        releaseOther();
    }
    check(drain()==1 && g_sockStatus[oldIndex]==SnSR::CLOSED &&
          g_connects==1,"B04_TIMEOUT_OLD_SOCKET_LEAKED");
    std::cout<<"G10_I_B04_TIMEOUT_EXISTING_TICKET=PASS\n";
}
int main(){try{
    cancelledRetargetMustNotRestart();
    recycledOwnerMustSurviveCancelRetry();
    explicitNewTargetSupersedesCancelledOne();
    timeoutWithoutSocketMustNotRestart();
    timeoutWithOldSocketMustLeaveTicket();
    check(g_spiOwner==0&&g_takes==g_releases&&g_wrongClose==0,
          "B04_LIFECYCLE_MUTEX_OR_OWNER_LEAK");
    return 0;
}catch(const std::exception& e){
    std::cerr<<"G10_I_B04_LIFECYCLE_ERROR="<<e.what()<<"\n";return 3;
}}

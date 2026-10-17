// Alpha13 G10-R: labCaptureSockets against REAL composed SafeAccess.cpp.
// HOST-SIMULATED non-recursive SPI mutex; not evidence of physical W5500.
// No reset, no application Q output, no flash.
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
static int taskId=1, busOwner=0, takes=0, releases=0, physicalClose=0;
static void check(bool value,const char* reason){
    if(!value)throw std::runtime_error(reason);
}
extern "C" bool jwplcSPI_acquire(uint32_t){
    if(busOwner) return false;
    busOwner=taskId;
    ++takes;
    return true;
}
extern "C" void jwplcSPI_release(void){
    check(busOwner==taskId,"RELEASE_NOT_OWNER");
    busOwner=0;++releases;
}
extern "C" void jwplcSPI_deselectAll(void){
    check(busOwner==taskId,"DESELECT_NOT_OWNER");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void){
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(taskId));
}
static void holdOther(){
    taskId=2;
    check(jwplcSPI_acquire(0),"SETUP_OTHER_MUTEX");
    taskId=1;
}
static void releaseOther(){
    taskId=2;jwplcSPI_release();taskId=1;
}
static void onClose(uint8_t index,void*){
    check(ownedByThisTask()&&index==2,"WRONG_PHYSICAL_CLOSE_SCOPE");
    ++physicalClose;
}
int main(){
 try{
    LabSocketSnapshot snap[8]={};
    check(labCaptureSockets(nullptr,8,0)==LabCaptureOutcome::InvalidOutput,
          "ACCEPTED_NULL_BUFFER");
    check(labCaptureSockets(snap,7,0)==LabCaptureOutcome::InvalidOutput,
          "ACCEPTED_SHORT_BUFFER");
    SocketToken old{};
    {
        BusScope bus(20);
        check(bool(bus),"BUS_ALLOCATE_FAILED");
        old=allocateSocketLockedAuto(2);
        check(bool(old),"INVALID_FIRST_LEASE");
        // Snapshot nests safely in current task's nonrecursive W5500 scope.
        check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::Captured,
              "NESTED_SNAPSHOT_BUSY");
    }
    check(snap[2].valid && snap[2].epoch==old.chipEpoch &&
          snap[2].allocation==old.allocation &&
          snap[2].ownerCookie==old.ownerCookie,
          "INITIAL_SNAPSHOT_LEASE_MISMATCH");
    holdOther();
    snap[2].epoch=0x12345678;
    check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::BusBusy,
          "BUS_BUSY_NOT_DISTINGUISHED");
    check(snap[2].epoch==0x12345678,
          "BUS_BUSY_OVERWROTE_UNREAD_OUTPUT");
    check(requestDeferredClose(old),"PUBLISH_DEFERRED_TICKET_FAILED");
    check(serviceDeferredClose(onClose,nullptr,0)==0 &&
          physicalClose==0,"CLOSE_WHILE_BUS_BUSY");
    releaseOther();
    check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::Captured &&
          snap[2].pendingCloseAllocation==old.allocation &&
          snap[2].valid,"PENDING_TICKET_NOT_VISIBLE");
    check(serviceDeferredClose(onClose,nullptr,0)==1 &&
          physicalClose==1,"DEFERRED_CLOSE_DID_NOT_DRAIN");
    check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::Captured &&
          !snap[2].valid && snap[2].pendingCloseAllocation==0,
          "SNAPSHOT_AFTER_DRAIN_WRONG");
    std::cout<<"G10_R_R3_LAB_PENDING_BUSY_DRAIN=PASS\n";
    SocketToken newer{};
    {
        BusScope bus(20);
        check(bool(bus),"SECOND_ALLOCATE_BUS");
        newer=allocateSocketLockedAuto(2);
        check(bool(newer)&&newer.allocation>old.allocation,
              "NEW_LEASE_NOT_DISTINCT");
    }
    // Stale publisher must NEVER close the recycled/current owner.
    check(requestDeferredClose(old),"PUBLISH_OLD_LATE_TICKET");
    check(serviceDeferredClose(onClose,nullptr,0)==0 &&
          physicalClose==1,"STALE_TICKET_CLOSED_NEW_OWNER");
    check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::Captured &&
          snap[2].valid &&
          snap[2].allocation==newer.allocation &&
          snap[2].pendingCloseAllocation==0,
          "RECYCLED_OWNER_SNAPSHOT_CORRUPT");
    check(closeIfCurrent(old,onClose,nullptr,0)==CloseOutcome::Stale,
          "OLD_TOKEN_CAN_CLOSE_NEW_OWNER");
    check(closeIfCurrent(newer,onClose,nullptr,0)==CloseOutcome::Closed &&
          physicalClose==2,"CURRENT_OWNER_CANNOT_CLOSE");
    check(labCaptureSockets(snap,8,0)==LabCaptureOutcome::Captured &&
          !snap[2].valid,"CURRENT_CLOSE_NOT_VISIBLE");
    check(busOwner==0&&takes==releases,
          "BUS_MUTEX_LEAK_OR_UNBALANCED_RELEASE");
    std::cout<<"G10_R_R3_LAB_RECYCLE_STALE_GUARD=PASS\n";
    std::cout<<"G10_R_R3_HOST_SOURCE_BACKED=PASS_NOT_PHYSICAL\n";
    return 0;
 }catch(const std::exception& err){
    std::cerr<<"G10_R_R3_NATIVE_REVIEW_STOP="<<err.what()<<"\n";
    return 3;
 }
}

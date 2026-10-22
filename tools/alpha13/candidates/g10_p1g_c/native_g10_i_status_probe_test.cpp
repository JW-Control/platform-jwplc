// Alpha13 G10-I — lectura W5500 con resultado triestado.
// Ejecuta el SafeAccess candidato C++ real con stubs nativos; NO es
// integración Arduino ni autoriza cambio de semántica de connected().
#include "JWPLC_W5500_SafeAccess.h"
#include "freertos/task.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
using namespace jwplc_w5500_candidate;
static int g_task=1, g_lock=0, g_takes=0, g_releases=0, g_reads=0;
static uint8_t g_status=0x17; // SnSR::ESTABLISHED
static void check(bool b,const char* e){if(!b)throw std::runtime_error(e);}
extern "C" bool jwplcSPI_acquire(uint32_t){
    if(g_lock!=0)return false;
    g_lock=g_task;++g_takes;return true;
}
extern "C" void jwplcSPI_release(void){
    check(g_lock==g_task,"WRONG_TASK_RELEASE");
    g_lock=0;++g_releases;
}
extern "C" void jwplcSPI_deselectAll(void) {
    check(g_lock==g_task,"DESELECT_NO_MUTEX");
}
extern "C" TaskHandle_t xTaskGetCurrentTaskHandle(void) {
    return reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(g_task));
}
static uint8_t readHardware(uint8_t,void*) {
    check(ownedByThisTask() && g_lock==g_task,
          "STATUS_HARDWARE_OUTSIDE_MUTEX");
    ++g_reads;return g_status;
}
static uint8_t readNested(uint8_t socket,void* context) {
    BusScope nested(20);
    check(bool(nested),"NESTED_STATUS_LOCK_FAILED");
    return readHardware(socket,context);
}
static bool resetChip(void*) {
    check(ownedByThisTask(),"RESET_WITHOUT_MUTEX");
    return true;
}
int main(){
    try {
        SocketToken first;
        {
            BusScope bus(20);
            check(bool(bus),"INITIAL_GUARD");
            first=allocateSocketLockedAuto(3);
            check(bool(first),"TOKEN_ALLOCATION_FAILED");
        }
        uint8_t observed=0xa5;
        check(readStatusIfCurrent(first,readHardware,nullptr,20,&observed)==
                  StatusReadOutcome::Read && observed==0x17 &&
              g_reads==1,"READ_ESTABLISHED_FAILED");
        g_status=0x00;
        check(readStatusIfCurrent(first,readNested,nullptr,20,&observed)==
                  StatusReadOutcome::Read && observed==0x00 &&
              g_reads==2 && !ownedByThisTask(),
              "READ_REAL_CLOSED_OR_NESTING_FAILED");
        std::cout<<"G10_I_STATUS_READ_CLOSED_ESTABLISHED=PASS\n";
        g_task=2;
        check(jwplcSPI_acquire(0),"CONTENTION_SETUP");
        g_task=1;
        observed=0xa5;
        const int reads=g_reads;
        check(readStatusIfCurrent(first,readHardware,nullptr,20,&observed)==
                 StatusReadOutcome::BusBusy &&
              observed==0xa5 && g_reads==reads,
              "SPI_BUSY_RETURNED_FAKED_CLOSED_OR_READ");
        g_task=2;jwplcSPI_release();g_task=1;
        std::cout<<"G10_I_STATUS_SPI_BUSY_NO_FALSE_CLOSED=PASS\n";
        SocketToken second;
        {
            BusScope bus(20);
            check(bool(bus),"REALLOC_GUARD");
            second=allocateSocketLockedAuto(3);
            check(bool(second) && second.allocation!=first.allocation,
                  "NEW_OWNER_LEASE_REUSED");
        }
        observed=0xa5;
        check(readStatusIfCurrent(first,readHardware,nullptr,20,&observed)==
                 StatusReadOutcome::Stale &&
              observed==0xa5 && g_reads==reads,
              "STALE_READ_TOUCHED_NEW_OWNER");
        check(readStatusIfCurrent(second,readHardware,nullptr,20,&observed)==
                 StatusReadOutcome::Read &&
              observed==0x00,"NEW_OWNER_NOT_READ");
        check(resetAndInvalidate(resetChip,nullptr,20),"RESET_CALLBACK_FAILED");
        observed=0xa5;
        check(readStatusIfCurrent(second,readHardware,nullptr,20,&observed)==
                 StatusReadOutcome::Stale &&
              observed==0xa5,"RESET_OLD_LEASE_ACCEPTED");
        check(g_takes==g_releases&&g_lock==0,"SPI_LOCK_LEAK");
        std::cout<<"G10_I_STATUS_REUSE_RESET_STALE=PASS\n";
        std::cout<<"G10_I_STATUS_NATIVE=PASS_TRISTATE_PRIMITIVE_ONLY_ADOPTION_BLOCKED\n";
        return 0;
    } catch(const std::exception& ex) {
        std::cerr<<"G10_I_STATUS_NATIVE_ERROR="<<ex.what()<<"\n";
        return 3;
    }
}

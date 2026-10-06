#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint32_t calls;
    uint32_t total_us;
    uint32_t max_us;
} JWPLCH3E0BStageStats;

typedef struct
{
    JWPLCH3E0BStageStats tcp_pre;
    JWPLCH3E0BStageStats tcp_post;
    JWPLCH3E0BStageStats serial_event;
    JWPLCH3E0BStageStats task_yield;
    JWPLCH3E0BStageStats outside_total;

    JWPLCH3E0BStageStats system_active;
    JWPLCH3E0BStageStats system_io;
    JWPLCH3E0BStageStats system_rtc;
    JWPLCH3E0BStageStats system_ethernet;
    JWPLCH3E0BStageStats system_datalog;
    JWPLCH3E0BStageStats system_display;

    uint32_t worst_outside_us;
    uint32_t worst_tcp_pre_us;
    uint32_t worst_tcp_post_us;
    uint32_t worst_serial_event_us;
    uint32_t worst_task_yield_us;
    uint32_t worst_direct_sum_us;
    uint32_t worst_residual_us;

    uint32_t worst_system_active_delta_us;
    uint32_t worst_system_io_delta_us;
    uint32_t worst_system_rtc_delta_us;
    uint32_t worst_system_ethernet_delta_us;
    uint32_t worst_system_datalog_delta_us;
    uint32_t worst_system_display_delta_us;
} JWPLCH3E0BStats;

// Hook de qualification. El core normal lo deja deshabilitado.
bool jwplcH3E0BProfilerEnabled(void);

// Reset y snapshot internos para gates de diagnóstico Alpha14.
void jwplcH3E0BReset(void);
void jwplcH3E0BSnapshot(JWPLCH3E0BStats *out);

#ifdef __cplusplus
}
#endif

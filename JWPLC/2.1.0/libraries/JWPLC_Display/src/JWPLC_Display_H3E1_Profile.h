#ifndef JWPLC_DISPLAY_H3E1_PROFILE_H
#define JWPLC_DISPLAY_H3E1_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define JWPLC_H3E1_MAX_FIELDS 16U

typedef struct
{
    uint32_t calls;
    uint32_t total_us;
    uint32_t max_us;
} JWPLCH3E1StageStats;

typedef struct
{
    uint8_t used;
    uint8_t field_id;
    uint8_t field_type;
    uint8_t text_length;

    uint16_t value_w;
    uint16_t value_h;

    uint32_t setter_calls;
    uint32_t setter_changed;
    uint32_t setter_unchanged;

    uint32_t draw_calls;
    uint32_t draw_total_us;
    uint32_t draw_max_us;

    uint32_t clear_total_us;
    uint32_t clear_max_us;

    uint32_t align_total_us;
    uint32_t align_max_us;

    uint32_t print_total_us;
    uint32_t print_max_us;
} JWPLCH3E1FieldStats;

typedef struct
{
    JWPLCH3E1StageStats refresh_total;
    JWPLCH3E1StageStats precheck;
    JWPLCH3E1StageStats refresh_needed;
    JWPLCH3E1StageStats spi_wait;
    JWPLCH3E1StageStats page_clear;
    JWPLCH3E1StageStats page_enter;
    JWPLCH3E1StageStats draw_static;
    JWPLCH3E1StageStats user_callback;
    JWPLCH3E1StageStats ui_update;
    JWPLCH3E1StageStats draw_dirty;
    JWPLCH3E1StageStats ui_dirty_core;
    JWPLCH3E1StageStats indicator;
    JWPLCH3E1StageStats release_bus;

    uint32_t refresh_skipped_clean;
    uint32_t spi_acquire_failures;

    uint32_t setter_calls;
    uint32_t setter_changed;
    uint32_t setter_unchanged;

    uint32_t dirty_pass_calls;
    uint32_t dirty_fields_total;
    uint32_t dirty_fields_max;

    JWPLCH3E1FieldStats fields[JWPLC_H3E1_MAX_FIELDS];
} JWPLCH3E1Stats;

typedef enum
{
    JWPLC_H3E1_STAGE_REFRESH_TOTAL = 0,
    JWPLC_H3E1_STAGE_PRECHECK,
    JWPLC_H3E1_STAGE_REFRESH_NEEDED,
    JWPLC_H3E1_STAGE_SPI_WAIT,
    JWPLC_H3E1_STAGE_PAGE_CLEAR,
    JWPLC_H3E1_STAGE_PAGE_ENTER,
    JWPLC_H3E1_STAGE_DRAW_STATIC,
    JWPLC_H3E1_STAGE_USER_CALLBACK,
    JWPLC_H3E1_STAGE_UI_UPDATE,
    JWPLC_H3E1_STAGE_DRAW_DIRTY,
    JWPLC_H3E1_STAGE_UI_DIRTY_CORE,
    JWPLC_H3E1_STAGE_INDICATOR,
    JWPLC_H3E1_STAGE_RELEASE_BUS
} JWPLCH3E1StageId;

bool jwplcH3E1ProfilerEnabled(void);
void jwplcH3E1Reset(void);
void jwplcH3E1Snapshot(JWPLCH3E1Stats *out);

void jwplcH3E1RecordStage(JWPLCH3E1StageId stage, uint32_t elapsed_us);
void jwplcH3E1RecordRefreshSkippedClean(void);
void jwplcH3E1RecordSpiAcquireFailure(void);

void jwplcH3E1RecordSetter(uint8_t field_id, bool changed);
void jwplcH3E1RecordDirtyPass(uint32_t dirty_fields);

void jwplcH3E1RecordFieldDraw(
    uint8_t field_id,
    uint8_t field_type,
    uint16_t value_w,
    uint16_t value_h,
    uint8_t text_length,
    uint32_t total_us,
    uint32_t clear_us,
    uint32_t align_us,
    uint32_t print_us);

#ifdef __cplusplus
}
#endif

#endif

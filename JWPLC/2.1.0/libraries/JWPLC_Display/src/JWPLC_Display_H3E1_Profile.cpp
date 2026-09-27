#include "JWPLC_Display_H3E1_Profile.h"

#include <Arduino.h>
#include <cstring>

static JWPLCH3E1Stats g_h3e1 = {};

extern "C" bool __attribute__((weak)) jwplcH3E1ProfilerEnabled(void)
{
    return false;
}

static void updateStage(
    JWPLCH3E1StageStats &stage,
    uint32_t elapsed_us)
{
    stage.calls++;
    stage.total_us += elapsed_us;

    if (elapsed_us > stage.max_us)
    {
        stage.max_us = elapsed_us;
    }
}

static JWPLCH3E1StageStats *stageForId(
    JWPLCH3E1StageId id)
{
    switch (id)
    {
    case JWPLC_H3E1_STAGE_REFRESH_TOTAL:
        return &g_h3e1.refresh_total;
    case JWPLC_H3E1_STAGE_PRECHECK:
        return &g_h3e1.precheck;
    case JWPLC_H3E1_STAGE_REFRESH_NEEDED:
        return &g_h3e1.refresh_needed;
    case JWPLC_H3E1_STAGE_SPI_WAIT:
        return &g_h3e1.spi_wait;
    case JWPLC_H3E1_STAGE_PAGE_CLEAR:
        return &g_h3e1.page_clear;
    case JWPLC_H3E1_STAGE_PAGE_ENTER:
        return &g_h3e1.page_enter;
    case JWPLC_H3E1_STAGE_DRAW_STATIC:
        return &g_h3e1.draw_static;
    case JWPLC_H3E1_STAGE_USER_CALLBACK:
        return &g_h3e1.user_callback;
    case JWPLC_H3E1_STAGE_UI_UPDATE:
        return &g_h3e1.ui_update;
    case JWPLC_H3E1_STAGE_DRAW_DIRTY:
        return &g_h3e1.draw_dirty;
    case JWPLC_H3E1_STAGE_UI_DIRTY_CORE:
        return &g_h3e1.ui_dirty_core;
    case JWPLC_H3E1_STAGE_INDICATOR:
        return &g_h3e1.indicator;
    case JWPLC_H3E1_STAGE_RELEASE_BUS:
        return &g_h3e1.release_bus;
    default:
        return nullptr;
    }
}

static JWPLCH3E1FieldStats *fieldForId(uint8_t field_id)
{
    for (uint8_t i = 0; i < JWPLC_H3E1_MAX_FIELDS; ++i)
    {
        if (g_h3e1.fields[i].used &&
            g_h3e1.fields[i].field_id == field_id)
        {
            return &g_h3e1.fields[i];
        }
    }

    for (uint8_t i = 0; i < JWPLC_H3E1_MAX_FIELDS; ++i)
    {
        if (!g_h3e1.fields[i].used)
        {
            g_h3e1.fields[i].used = 1U;
            g_h3e1.fields[i].field_id = field_id;
            return &g_h3e1.fields[i];
        }
    }

    return nullptr;
}

extern "C" void jwplcH3E1Reset(void)
{
    vTaskSuspendAll();
    memset(&g_h3e1, 0, sizeof(g_h3e1));
    (void)xTaskResumeAll();
}

extern "C" void jwplcH3E1Snapshot(JWPLCH3E1Stats *out)
{
    if (out == nullptr)
    {
        return;
    }

    vTaskSuspendAll();
    *out = g_h3e1;
    (void)xTaskResumeAll();
}

extern "C" void jwplcH3E1RecordStage(
    JWPLCH3E1StageId stage,
    uint32_t elapsed_us)
{
    if (!jwplcH3E1ProfilerEnabled())
    {
        return;
    }

    JWPLCH3E1StageStats *target = stageForId(stage);

    if (target != nullptr)
    {
        updateStage(*target, elapsed_us);
    }
}

extern "C" void jwplcH3E1RecordRefreshSkippedClean(void)
{
    if (jwplcH3E1ProfilerEnabled())
    {
        g_h3e1.refresh_skipped_clean++;
    }
}

extern "C" void jwplcH3E1RecordSpiAcquireFailure(void)
{
    if (jwplcH3E1ProfilerEnabled())
    {
        g_h3e1.spi_acquire_failures++;
    }
}

extern "C" void jwplcH3E1RecordSetter(
    uint8_t field_id,
    bool changed)
{
    if (!jwplcH3E1ProfilerEnabled())
    {
        return;
    }

    g_h3e1.setter_calls++;

    if (changed)
    {
        g_h3e1.setter_changed++;
    }
    else
    {
        g_h3e1.setter_unchanged++;
    }

    JWPLCH3E1FieldStats *field = fieldForId(field_id);

    if (field == nullptr)
    {
        return;
    }

    field->setter_calls++;

    if (changed)
    {
        field->setter_changed++;
    }
    else
    {
        field->setter_unchanged++;
    }
}

extern "C" void jwplcH3E1RecordDirtyPass(
    uint32_t dirty_fields)
{
    if (!jwplcH3E1ProfilerEnabled())
    {
        return;
    }

    g_h3e1.dirty_pass_calls++;
    g_h3e1.dirty_fields_total += dirty_fields;

    if (dirty_fields > g_h3e1.dirty_fields_max)
    {
        g_h3e1.dirty_fields_max = dirty_fields;
    }
}

extern "C" void jwplcH3E1RecordFieldDraw(
    uint8_t field_id,
    uint8_t field_type,
    uint16_t value_w,
    uint16_t value_h,
    uint8_t text_length,
    uint32_t total_us,
    uint32_t clear_us,
    uint32_t align_us,
    uint32_t print_us)
{
    if (!jwplcH3E1ProfilerEnabled())
    {
        return;
    }

    JWPLCH3E1FieldStats *field = fieldForId(field_id);

    if (field == nullptr)
    {
        return;
    }

    field->field_type = field_type;
    field->value_w = value_w;
    field->value_h = value_h;
    field->text_length = text_length;

    field->draw_calls++;
    field->draw_total_us += total_us;
    field->clear_total_us += clear_us;
    field->align_total_us += align_us;
    field->print_total_us += print_us;

    if (total_us > field->draw_max_us)
    {
        field->draw_max_us = total_us;
    }

    if (clear_us > field->clear_max_us)
    {
        field->clear_max_us = clear_us;
    }

    if (align_us > field->align_max_us)
    {
        field->align_max_us = align_us;
    }

    if (print_us > field->print_max_us)
    {
        field->print_max_us = print_us;
    }
}

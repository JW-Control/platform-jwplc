#include "JWPLC_HMI_Generated.h"
#include <math.h>

namespace
{
uint32_t g_lastSimMs = 0UL;
uint32_t g_measStartMs = 0UL;
bool g_prevMeasRun = false;
float g_phase = 0.0f;

float g_tcVal = 26.4f;
float g_codVal = 54.2f;
float g_bodVal = 28.7f;
float g_tocVal = 19.5f;
float g_turbVal = 12.8f;

void syncDisplayVariables()
{
    valTCod = g_tcVal;
    valCod = g_codVal;
    valBod = g_bodVal;
    valToc = g_tocVal;
    valTurb = g_turbVal;

    trendTCod = g_tcVal;
    trendCod = g_codVal;
    trendBod = g_bodVal;
    trendToc = g_tocVal;
    trendTurb = g_turbVal;

    stCfg = measRun;
    stGen = measRun;
    stTCod = measRun;
    stCod = measRun;
    stBod = measRun;
    stToc = measRun;
    stTurb = measRun;
}

void updateSensorSimulation(uint32_t nowMs)
{
    if (!simActive || (nowMs - g_lastSimMs) < 100UL)
    {
        return;
    }

    const float dt = (float)(nowMs - g_lastSimMs) * 0.001f;
    g_lastSimMs = nowMs;
    g_phase += dt * 0.65f;

    const float measBoost = measRun ? 1.12f : 1.0f;

    // Simulación suave multicurva inspirada en SENSOR_COD (solo visualización y simulación local)
    g_tcVal = 27.5f + 5.2f * sinf(g_phase * 0.90f) + 2.6f * sinf(g_phase * 2.15f + 0.4f);
    g_codVal = (54.0f + 19.5f * sinf(g_phase * 0.75f + 0.7f) + 9.0f * cosf(g_phase * 1.90f)) * measBoost;
    g_bodVal = (28.5f + 11.0f * sinf(g_phase * 0.80f + 1.4f) + 5.5f * sinf(g_phase * 2.05f)) * measBoost;
    g_tocVal = (19.5f + 8.0f * sinf(g_phase * 0.85f + 2.2f) + 4.2f * cosf(g_phase * 1.75f)) * measBoost;
    g_turbVal = (13.5f + 7.5f * sinf(g_phase * 1.05f + 3.0f) + 4.5f * sinf(g_phase * 2.40f)) * measBoost;

    if (g_tcVal < 15.0f) g_tcVal = 15.0f;
    if (g_codVal < 5.0f) g_codVal = 5.0f;
    if (g_bodVal < 2.0f) g_bodVal = 2.0f;
    if (g_tocVal < 2.0f) g_tocVal = 2.0f;
    if (g_turbVal < 0.5f) g_turbVal = 0.5f;
}

void updateMeasurementState(uint32_t nowMs)
{
    if (JWPLC_Display.menuChanged(MENU_MEDICION))
    {
        if (JWPLC_Display.menuNode() == MENU_MEDICION_MEDICION)
        {
            if (measRun)
            {
                g_measStartMs = nowMs;
                measElapsedS = 0;
                measProgress = 0.0f;
            }
            else
            {
                measProgress = 0.0f;
                measElapsedS = 0;
            }
        }
    }

    if (measRun && !g_prevMeasRun)
    {
        g_measStartMs = nowMs;
    }

    if (measRun)
    {
        const uint32_t durationS = (uint32_t)((measMins > 0) ? measMins : 1) * 60UL;
        const uint32_t elapsedMs = nowMs - g_measStartMs;
        const uint32_t elapsedS = elapsedMs / 1000UL;

        measElapsedS = elapsedS;
        measProgress = (100.0f * (float)elapsedMs) / ((float)durationS * 1000.0f);

        if (measProgress >= 100.0f || elapsedS >= durationS)
        {
            measProgress = 100.0f;
            measElapsedS = durationS;
            measRun = false;
            measTag = (measTag >= 99) ? 1 : (measTag + 1);
        }
    }

    g_prevMeasRun = measRun;
}
} // namespace

void setup()
{
    g_lastSimMs = millis();
    syncDisplayVariables();
    jwplcHMISetup();
}

void loop()
{
    const uint32_t nowMs = millis();
    updateMeasurementState(nowMs);
    updateSensorSimulation(nowMs);
    syncDisplayVariables();
}

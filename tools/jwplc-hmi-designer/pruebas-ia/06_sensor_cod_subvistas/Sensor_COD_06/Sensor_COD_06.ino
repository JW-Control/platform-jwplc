#include "JWPLC_HMI_Generated.h"
#include <math.h>
#include <stdio.h>

namespace
{
uint32_t g_lastSimMs = 0UL;
uint32_t g_measStartMs = 0UL;
bool g_prevMeasRun = false;
bool g_wasPopupOpen = false;
float g_phase = 0.0f;

float g_tcVal = 26.4f;
float g_codVal = 54.2f;
float g_bodVal = 28.7f;
float g_tocVal = 19.5f;
float g_turbVal = 12.8f;

void formatMonitoringTexts(uint32_t elapsedS, uint32_t durationS)
{
    const long tag = (measTag < 1) ? 1L : ((measTag > 99) ? 99L : (long)measTag);
    const long totalMins = (measMins < 1) ? 1L : ((measMins > 60) ? 60L : (long)measMins);

    snprintf(measTagText, sizeof(measTagText), "P%02ld", tag);

    if (!measRun)
    {
        snprintf(measStatus, sizeof(measStatus), "ESPERA 5");
    }
    else if (elapsedS < 5UL)
    {
        snprintf(measStatus, sizeof(measStatus), "ESPERA %lu", (unsigned long)(5UL - elapsedS));
    }
    else if (elapsedS + 3UL >= durationS)
    {
        snprintf(measStatus, sizeof(measStatus), "GUARDAR");
    }
    else
    {
        snprintf(measStatus, sizeof(measStatus), "MID COD");
    }

    const unsigned long mm = (unsigned long)(elapsedS / 60UL);
    const unsigned long ss = (unsigned long)(elapsedS % 60UL);
    snprintf(
        measSubtext,
        sizeof(measSubtext),
        "Muestra actual: P%02ld    %02lu:%02lu / %02ld:00",
        tag,
        mm,
        ss,
        totalMins);
}

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

    stGen = measRun;
    stRep = measRun;
}

void updateSensorSimulation(uint32_t nowMs)
{
    if ((nowMs - g_lastSimMs) < 100UL)
    {
        return;
    }

    const float dt = (float)(nowMs - g_lastSimMs) * 0.001f;
    g_lastSimMs = nowMs;
    g_phase += dt * 0.65f;

    const float measBoost = measRun ? 1.12f : 1.0f;

    // Simulación suave multicurva inspirada en SENSOR_COD
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
    // Si el operador confirmó el popup modal de inicio de medición
    if (JWPLC_Display.popupConfirmed(POPUP_CONFIRM_MEDICION))
    {
        measRun = true;
        g_measStartMs = nowMs;
        JWPLC_Display.setPageView(PAGE_MEDICION, 1);
    }

    if (measRun && !g_prevMeasRun)
    {
        g_measStartMs = nowMs;
    }

    const uint32_t durationS = (uint32_t)((measMins > 0) ? measMins : 1) * 60UL;

    if (measRun)
    {
        const uint32_t elapsedMs = nowMs - g_measStartMs;
        const uint32_t elapsedS = elapsedMs / 1000UL;

        if (elapsedS >= durationS)
        {
            measElapsed = durationS;
            measProgress = 100.0f;
            formatMonitoringTexts(durationS, durationS);

            measRun = false;
            measTag = (measTag >= 99) ? 1 : (measTag + 1);

            // Dispara popup informativo de fin de medición (que al cerrarse regresa a la subvista 0)
            popupFinMedicion = true;
        }
        else
        {
            measElapsed = elapsedS;
            measProgress = ((float)elapsedMs * 100.0f) / ((float)durationS * 1000.0f);
            if (measProgress > 100.0f) measProgress = 100.0f;
            formatMonitoringTexts(elapsedS, durationS);
        }

        // En la subvista 1 (Monitoreo), pulsar OK detiene el ciclo y regresa a Ajustes (subvista 0)
        if (!JWPLC_Display.popupActive() &&
            !JWPLC_Display.isUserPageSelection() &&
            JWPLC_Display.userPage() == PAGE_MEDICION &&
            JWPLC_Display.pageView(PAGE_MEDICION) == 1)
        {
            if (JWPLC_Buttons.pressed(BTN_OK))
            {
                measRun = false;
                JWPLC_Display.setPageView(PAGE_MEDICION, 0);
            }
        }
    }
    else
    {
        // Estado de previsualización inicial (00:51 / 05:00 como en la referencia visual)
        measElapsed = 51UL;
        measProgress = 17.0f;
        formatMonitoringTexts(measElapsed, durationS);
    }

    // Rearma los triggers booleanos una vez que la ventana emergente se cierra
    const bool popupOpen = JWPLC_Display.popupActive();
    if (g_wasPopupOpen && !popupOpen)
    {
        popupFinMedicion = false;
        popupAvisoReactivo = false;
        popupFallaSensor = false;
    }
    g_wasPopupOpen = popupOpen;

    g_prevMeasRun = measRun;
}
} // namespace

void setup()
{
    g_lastSimMs = millis();
    formatMonitoringTexts(51UL, 300UL);
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

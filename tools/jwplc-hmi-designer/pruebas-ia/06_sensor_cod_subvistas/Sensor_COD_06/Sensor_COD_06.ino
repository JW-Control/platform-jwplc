#include "JWPLC_HMI_Generated.h"
#include <math.h>
#include <stdio.h>

namespace
{
uint32_t g_lastSimMs = 0UL;
uint32_t g_measStartMs = 0UL;
uint32_t g_stopWaitStartMs = 0UL;
bool g_prevMeasRun = false;
bool g_stopWaitActive = false;
bool g_stopConfirmPending = false;
bool g_stopConfirmSeenActive = false;
bool g_wasPopupOpen = false;
float g_phase = 0.0f;

float g_tcVal = 26.4f;
float g_codVal = 54.2f;
float g_bodVal = 28.7f;
float g_tocVal = 19.5f;
float g_turbVal = 12.8f;

void formatMonitoringTexts(uint32_t elapsedS, uint32_t durationS, uint8_t waitRemS)
{
    const long tag = (measTag < 1) ? 1L : ((measTag > 99) ? 99L : (long)measTag);
    const long totalMins = (measMins < 1) ? 1L : ((measMins > 60) ? 60L : (long)measMins);

    snprintf(measTagText, sizeof(measTagText), "P%02ld", tag);

    const uint8_t shownWait = (waitRemS > 5U) ? 5U : waitRemS;
    if (shownWait == 0U)
    {
        snprintf(measStatus, sizeof(measStatus), "ESPERA 0");
    }
    else
    {
        snprintf(measStatus, sizeof(measStatus), "ESPERA %u", (unsigned)shownWait);
    }

    const unsigned long mm = (unsigned long)(elapsedS / 60UL);
    const unsigned long ss = (unsigned long)(elapsedS % 60UL);
    (void)durationS;
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
    bool finJustTriggered = false;
    bool stoppedJustTriggered = false;

    // Si el operador confirmó el popup modal de inicio de medición
    if (JWPLC_Display.popupConfirmed(POPUP_CONFIRM_MEDICION) || (measRun && !g_prevMeasRun))
    {
        measRun = true;
        g_measStartMs = nowMs;
        g_stopWaitActive = false;
        g_stopConfirmPending = false;
        g_stopConfirmSeenActive = false;
        popupConfirmDetener = false;
        stopConfirmed = false;
        JWPLC_Display.setPageView(PAGE_MEDICION, 1);
    }

    const uint32_t durationS = (uint32_t)((measMins > 0) ? measMins : 1) * 60UL;
    const bool popupOpenNow = JWPLC_Display.popupActive();

    if (measRun)
    {
        const uint32_t elapsedMs = nowMs - g_measStartMs;
        const uint32_t elapsedS = elapsedMs / 1000UL;

        if (elapsedS >= durationS)
        {
            measElapsed = durationS;
            measProgress = 100.0f;
            formatMonitoringTexts(durationS, durationS, 0U);

            measRun = false;
            g_stopWaitActive = false;
            g_stopConfirmPending = false;
            g_stopConfirmSeenActive = false;
            popupConfirmDetener = false;
            stopConfirmed = false;
            measTag = (measTag >= 99) ? 1 : (measTag + 1);

            if (popupOpenNow)
            {
                JWPLC_Display.dismissPopup(POPUP_CONFIRM_DETENER);
            }
            // Dispara popup informativo de ciclo completado (al cerrarse regresa a subvista 0)
            popupFinMedicion = true;
            finJustTriggered = true;
        }
        else
        {
            measElapsed = elapsedS;
            measProgress = ((float)elapsedMs * 100.0f) / ((float)durationS * 1000.0f);
            if (measProgress > 100.0f) measProgress = 100.0f;

            // En la subvista 1 (Monitoreo, botón rojo DETENER), pulsar OK inicia ESPERA 5s en subvista 2
            if (!g_stopWaitActive &&
                !popupOpenNow &&
                !JWPLC_Display.isUserPageSelection() &&
                JWPLC_Display.userPage() == PAGE_MEDICION &&
                JWPLC_Display.pageView(PAGE_MEDICION) == 1 &&
                JWPLC_Buttons.pressed(BTN_OK))
            {
                g_stopWaitActive = true;
                g_stopWaitStartMs = nowMs;
                g_stopConfirmPending = false;
                g_stopConfirmSeenActive = false;
                JWPLC_Display.setPageView(PAGE_MEDICION, 2);
            }

            if (g_stopWaitActive)
            {
                const uint32_t waitElapsedS = (nowMs - g_stopWaitStartMs) / 1000UL;
                const uint8_t waitRemS = (waitElapsedS < 5UL) ? (uint8_t)(5UL - waitElapsedS) : 0U;
                formatMonitoringTexts(elapsedS, durationS, waitRemS);

                if (waitRemS == 0U && !g_stopConfirmPending && !popupOpenNow)
                {
                    g_stopConfirmPending = true;
                    g_stopConfirmSeenActive = false;
                    stopConfirmed = false;
                    popupConfirmDetener = true;
                }
            }
            else
            {
                formatMonitoringTexts(elapsedS, durationS, 5U);
            }

            if (g_stopConfirmPending && popupOpenNow)
            {
                g_stopConfirmSeenActive = true;
            }

            // Resultado del popup CONFIRM para detener la medición
            if (JWPLC_Display.popupConfirmed(POPUP_CONFIRM_DETENER))
            {
                measRun = false;
                g_stopWaitActive = false;
                g_stopConfirmPending = false;
                g_stopConfirmSeenActive = false;
                popupConfirmDetener = false;
                stopConfirmed = false;
                // Dispara popup amarillo de ciclo detenido (al pulsar OK regresa a subvista 0)
                popupMedicionDetenida = true;
                stoppedJustTriggered = true;
            }
            else if (g_stopConfirmPending && g_stopConfirmSeenActive && !popupOpenNow)
            {
                // Operador seleccionó SEGUIR o ESC: cancelar detención y volver a botón rojo DETENER
                g_stopWaitActive = false;
                g_stopConfirmPending = false;
                g_stopConfirmSeenActive = false;
                popupConfirmDetener = false;
                JWPLC_Display.setPageView(PAGE_MEDICION, 1);
            }
        }
    }
    else
    {
        // Estado de previsualización inicial (00:02 / 05:00 como en la referencia visual)
        g_stopWaitActive = false;
        g_stopConfirmPending = false;
        g_stopConfirmSeenActive = false;
        measElapsed = 2UL;
        measProgress = 1.0f;
        formatMonitoringTexts(measElapsed, durationS, 5U);
    }

    // En la subvista 3 (WiFi_QR), pulsar OK regresa a Ajustes (subvista 0)
    if (!popupOpenNow &&
        !JWPLC_Display.isUserPageSelection() &&
        JWPLC_Display.userPage() == PAGE_MEDICION &&
        JWPLC_Display.pageView(PAGE_MEDICION) == 3)
    {
        if (JWPLC_Buttons.pressed(BTN_OK))
        {
            JWPLC_Display.setPageView(PAGE_MEDICION, 0);
        }
    }

    // Rearma los triggers booleanos una vez que la ventana emergente se cierra
    if (g_wasPopupOpen && !popupOpenNow)
    {
        if (!finJustTriggered) popupFinMedicion = false;
        if (!stoppedJustTriggered) popupMedicionDetenida = false;
        popupConfirmDetener = false;
        popupAvisoReactivo = false;
        popupFallaSensor = false;
    }
    g_wasPopupOpen = popupOpenNow;

    g_prevMeasRun = measRun;
}
} // namespace

void setup()
{
    g_lastSimMs = millis();
    formatMonitoringTexts(2UL, 300UL, 5U);
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

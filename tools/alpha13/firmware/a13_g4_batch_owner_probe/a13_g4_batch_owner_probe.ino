#include <Arduino.h>
#include <JWPLC_TFT.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

// Probar ownership de batch sin tocar GPIO virtuales ni salidas de rele.
// La tarea auxiliar mantiene un batch; la tarea loop intenta robarlo/cerrarlo.
static const char kRun[] = "__G4_TOKEN__";
static SemaphoreHandle_t sStart = nullptr;
static SemaphoreHandle_t sHeld = nullptr;
static SemaphoreHandle_t sRelease = nullptr;
static SemaphoreHandle_t sDone = nullptr;
static volatile uint32_t sOwnerErrors = 0;
static volatile bool sTaskCreated = false;
static constexpr int kTrials = 50;

static void ownerTask(void *)
{
    for (;;)
    {
        if (xSemaphoreTake(sStart, portMAX_DELAY) != pdTRUE)
            continue;

        const bool locked = JWPLC_TFT.beginBatch(1000);
        if (!locked)
        {
            ++sOwnerErrors;
        }
        else if (!JWPLC_TFT.fillRect(0, 0, 1, 1, JWPLC_TFT_BLACK, 0))
        {
            ++sOwnerErrors;
        }
        xSemaphoreGive(sHeld);

        if (xSemaphoreTake(sRelease, pdMS_TO_TICKS(1200)) != pdTRUE)
            ++sOwnerErrors;

        if (locked)
            JWPLC_TFT.endBatch();
        xSemaphoreGive(sDone);
    }
}

static void runTest()
{
    int errors = 0;
    int completed = 0;
    const uint32_t ownerBefore = sOwnerErrors;

    for (int i = 0; i < kTrials; ++i)
    {
        xSemaphoreGive(sStart);
        if (xSemaphoreTake(sHeld, pdMS_TO_TICKS(1500)) != pdTRUE)
        {
            ++errors;
            xSemaphoreGive(sRelease);
            break;
        }

        // La segunda tarea no debe fingir poseer el batch.
        if (!JWPLC_TFT.batchActive())
            ++errors;
        if (JWPLC_TFT.beginBatch(0))
        {
            ++errors;
            // Si una version rota devuelve true, no liberar desde otra tarea.
        }
        JWPLC_TFT.endBatch(); // Debe ser estrictamente inofensivo.

        // Tampoco debe dibujar durante el batch de otra tarea.
        if (JWPLC_TFT.fillRect(0, 0, 1, 1, JWPLC_TFT_BLACK, 0))
            ++errors;

        xSemaphoreGive(sRelease);
        if (xSemaphoreTake(sDone, pdMS_TO_TICKS(2000)) != pdTRUE)
        {
            ++errors;
            break;
        }

        if (JWPLC_TFT.batchActive())
            ++errors;

        // Verificar que el mutex vuelve a estar disponible, sin fuga.
        if (!JWPLC_TFT.beginBatch(1000))
        {
            ++errors;
            break;
        }
        if (!JWPLC_TFT.beginBatch(0)) // idempotencia misma tarea
            ++errors;
        if (!JWPLC_TFT.fillRect(0, 0, 1, 1, JWPLC_TFT_BLACK, 0))
            ++errors;
        JWPLC_TFT.endBatch();
        if (JWPLC_TFT.batchActive())
            ++errors;

        ++completed;
        if (errors)
            break;
    }

    errors += static_cast<int>(sOwnerErrors - ownerBefore);
    Serial.print("G4_RESULT=");
    Serial.println(errors == 0 && completed == kTrials ? "PASS" : "FAIL");
    Serial.print("G4_ERRORS=");
    Serial.println(errors);
    Serial.print("G4_TRIALS=");
    Serial.println(completed);
    Serial.print("G4_BATCH_FINAL=");
    Serial.println(JWPLC_TFT.batchActive() ? "ACTIVE" : "INACTIVE");
    Serial.print("G4_DONE=");
    Serial.println(kRun);
    Serial.flush();
}

void setup()
{
    Serial.begin(115200);
    Serial.setTimeout(350);
    sStart = xSemaphoreCreateBinary();
    sHeld = xSemaphoreCreateBinary();
    sRelease = xSemaphoreCreateBinary();
    sDone = xSemaphoreCreateBinary();
    if (sStart && sHeld && sRelease && sDone && JWPLC_TFT.isReady())
    {
        sTaskCreated = (xTaskCreatePinnedToCore(ownerTask, "G4_TFT_OWNER",
                                                3072, nullptr, 2,
                                                nullptr, 0) == pdPASS);
    }
    if (!sTaskCreated)
    {
        Serial.println("G4_SETUP=FAIL");
        return;
    }
    Serial.print("G4_READY=");
    Serial.println(kRun);
}

void loop()
{
    if (!sTaskCreated)
    {
        delay(100);
        return;
    }
    if (Serial.available())
    {
        String cmd = Serial.readStringUntil('\n');
        cmd.trim();
        if (cmd == String("G4_RUN:") + kRun)
        {
            runTest();
            sTaskCreated = false; // Ensayo de una sola vez, no repetir.
        }
    }
    static uint32_t last = 0;
    if (millis() - last > 500 && sTaskCreated)
    {
        last = millis();
        Serial.print("G4_READY=");
        Serial.println(kRun);
    }
    delay(5);
}

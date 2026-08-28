#include <Arduino.h>
#include <JWPLC_Display.h>

struct ErrVisualStep
{
    const char *label;
    const char *code;
    bool legacy;
    bool legacyState;
};

static const ErrVisualStep STEPS[] =
{
    {"OFF inicial", "",     false, false},
    {"Codigo 1",    "1",    false, false},
    {"Codigo A01",  "A01",  false, false},
    {"Normaliza",   "temp", false, false},
    {"Codigo ZZZZ", "ZZZZ", false, false},
    {"Cero = OFF",  "0",    false, false},
    {"Legacy rojo", nullptr, true,  true},
    {"OFF final",   nullptr, true,  false}
};

static constexpr uint8_t STEP_COUNT =
    sizeof(STEPS) / sizeof(STEPS[0]);

static constexpr uint32_t STEP_MS = 4000UL;

static uint8_t currentStep = 0;
static uint32_t stepStartedMs = 0;

static void applyStep(uint8_t index)
{
    const ErrVisualStep &step = STEPS[index];

    bool ok = true;

    if (step.legacy)
    {
        JWPLC_Display.setErrLed(step.legacyState);
    }
    else
    {
        ok = JWPLC_Display.setErrCode(step.code);
    }

    Serial.println();
    Serial.print(F("ERR_VISUAL_STEP="));
    Serial.println(index);

    Serial.print(F("LABEL="));
    Serial.println(step.label);

    Serial.print(F("SET_OK="));
    Serial.println(ok ? F("YES") : F("NO"));

    Serial.print(F("ERR_LED="));
    Serial.println(JWPLC_Display.errLed() ? F("ON") : F("OFF"));

    Serial.print(F("ERR_CODE='"));
    Serial.print(JWPLC_Display.errCode());
    Serial.println(F("'"));

    if (!ok)
    {
        Serial.println(F("ERR_VISUAL_API=FAIL"));
    }
}

void setup()
{
    Serial.begin(115200);
    delay(800);

    Serial.println();
    Serial.println(F("=============================================="));
    Serial.println(F(" ALPHA6 - ERR CODE VISUAL"));
    Serial.println(F("=============================================="));
    Serial.println(F("Secuencia automatica cada 4 segundos:"));
    Serial.println(F("OFF -> 1 -> A01 -> TEMP -> ZZZZ -> OFF -> legacy rojo -> OFF"));

    JWPLC_Display.goIdle();

    currentStep = 0;
    applyStep(currentStep);
    stepStartedMs = millis();
}

void loop()
{
    const uint32_t now = millis();

    if ((uint32_t)(now - stepStartedMs) < STEP_MS)
    {
        delay(10);
        return;
    }

    stepStartedMs = now;

    currentStep++;

    if (currentStep >= STEP_COUNT)
    {
        currentStep = 0;
        Serial.println();
        Serial.println(F("ERR_VISUAL_SEQUENCE=REPEAT"));
    }

    applyStep(currentStep);
}
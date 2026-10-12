/*
 * Alpha13 G9-P5: smoke fisico read-only de SD, RTC y botonera en JWPLC Basic.
 * USB CH340 COM4. Preservar autoload normal, sin tocar Q/relés, FRAM o RTC.
 * Nunca crear/escribir/borrar archivos en la microSD.
 * Espera G9_P5_START:<token> y luego una pulsacion/liberacion fisica.
 */
#include <JWPLC_GlobalPeripherals.h>
#include <string.h>

static const uint8_t kIds[] = {
    BTN_LEFT, BTN_UP, BTN_RIGHT, BTN_ESC, BTN_OK, BTN_DOWN
};
static constexpr size_t kButtons = sizeof(kIds) / sizeof(kIds[0]);
static char lineBuf[72];
static size_t lineLen = 0;
static char session[25] = {};
static bool running = false;
static bool finished = false;
static bool sdPresent = false;
static bool sdReady = false;
static bool sdRoot = false;
static bool pressed = false;
static bool released = false;
static int chosenButton = -1;
static uint32_t lastReadyMs = 0;
static uint32_t lastSampleMs = 0;

static void printSummary()
{
    finished = true;
    running = false;
    Serial.print("G9_P5_RESULT:");
    Serial.print(session);
    Serial.print(" SD_PRESENT=");
    Serial.print(sdPresent ? 1 : 0);
    Serial.print(" SD_READY=");
    Serial.print(sdReady ? 1 : 0);
    Serial.print(" SD_ROOT=");
    Serial.print(sdRoot ? 1 : 0);
    Serial.print(" RTC_PRESENT=");
    Serial.print(JWPLC_Time.present() ? 1 : 0);
    Serial.print(" RTC_VALID=");
    Serial.print(JWPLC_Time.valid() ? 1 : 0);
    Serial.print(" RTC_LOST=");
    Serial.print(JWPLC_Time.lostPower() ? 1 : 0);
    Serial.print(" BTN_PRESS=");
    Serial.print(pressed ? 1 : 0);
    Serial.print(" BTN_RELEASE=");
    Serial.print(released ? 1 : 0);
    Serial.print(" BTN_ID=");
    Serial.println(chosenButton);
}

static void probeSD()
{
    sdPresent = JWPLC_SD.isCardPresent();
    sdReady = JWPLC_SD.isReady();
    if (sdPresent && sdReady)
    {
        // FILE_READ exclusivamente. Un directorio vacio es valido.
        JWPLCFile root = JWPLC_SD.open("/", FILE_READ);
        if (root)
        {
            sdRoot = root.isDirectory();
            root.close();
        }
    }
    Serial.print("G9_P5_SD:");
    Serial.print(session);
    Serial.print(" PRESENT=");
    Serial.print(sdPresent ? 1 : 0);
    Serial.print(" READY=");
    Serial.print(sdReady ? 1 : 0);
    Serial.print(" ROOT=");
    Serial.println(sdRoot ? 1 : 0);
}

static void processLine(const char *line)
{
    static const char kPrefix[] = "G9_P5_START:";
    if (running || finished ||
        strncmp(line, kPrefix, sizeof(kPrefix) - 1) != 0)
        return;
    const char *value = line + sizeof(kPrefix) - 1;
    if (strlen(value) != 24)
        return;
    for (size_t i = 0; i < 24; ++i)
        if (!((value[i] >= '0' && value[i] <= '9') ||
              (value[i] >= 'a' && value[i] <= 'f')))
            return;
    memcpy(session, value, 24);
    session[24] = 0;
    JWPLC_Buttons.clearPendingInput();
    running = true;
    Serial.print("G9_P5_STARTED:");
    Serial.println(session);
    probeSD();
    Serial.print("G9_P5_BUTTON_WAIT:");
    Serial.println(session);
}

static void serviceUSB()
{
    while (Serial.available() > 0)
    {
        int c = Serial.read();
        if (c < 0) break;
        if (c == '\n' || c == '\r')
        {
            if (lineLen)
            {
                lineBuf[lineLen] = '\0';
                processLine(lineBuf);
                lineLen = 0;
            }
        }
        else if (lineLen + 1 < sizeof(lineBuf))
            lineBuf[lineLen++] = (char)c;
        else
            lineLen = 0;
    }
}

void setup()
{
    Serial.begin(115200);
    JWPLC_Buttons.clearPendingInput();
}

void loop()
{
    serviceUSB();
    if (finished) return;
    const uint32_t now = millis();
    if (!running)
    {
        if ((uint32_t)(now - lastReadyMs) >= 500)
        {
            lastReadyMs = now;
            Serial.println("G9_P5_READY:COM4:SD_RTC_BUTTONS:READ_ONLY");
        }
        return;
    }

    // El scanner de matriz lo mantiene el runtime, no llamar update().
    for (size_t i = 0; i < kButtons; ++i)
    {
        if (JWPLC_Buttons.pressed(kIds[i]) && !pressed)
        {
            chosenButton = (int)kIds[i];
            pressed = true;
            Serial.print("G9_P5_PRESS:");
            Serial.print(session);
            Serial.print(" ID=");
            Serial.println(chosenButton);
        }
        if (JWPLC_Buttons.released(kIds[i]) &&
            pressed && !released && chosenButton == (int)kIds[i])
        {
            released = true;
            Serial.print("G9_P5_RELEASE:");
            Serial.print(session);
            Serial.print(" ID=");
            Serial.println(chosenButton);
        }
    }

    if ((uint32_t)(now - lastSampleMs) >= 1000)
    {
        lastSampleMs = now;
        Serial.print("G9_P5_RTC:");
        Serial.print(session);
        Serial.print(" PRESENT=");
        Serial.print(JWPLC_Time.present() ? 1 : 0);
        Serial.print(" VALID=");
        Serial.print(JWPLC_Time.valid() ? 1 : 0);
        Serial.print(" LOST=");
        Serial.println(JWPLC_Time.lostPower() ? 1 : 0);
    }
    if (pressed && released)
        printSummary();
}

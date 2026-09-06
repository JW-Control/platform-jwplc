#include <JWPLC_Display.h>

// =============================================================
// JWPLC HMI Designer — Live Preview Bridge
// Alpha11 · herramienta de desarrollo, NO runtime de producción.
//
// Transporte:
//   Web Serial @ 921600 baud
//   RLE RGB565 del framebuffer lógico 320x170.
//
// El Designer sigue generando C++ mediante la API pública JWPLC_UI.
// Este bridge sólo permite ver el framebuffer del editor en la TFT física
// sin recompilar el sketch en cada cambio.
// =============================================================

namespace
{
    static constexpr uint16_t FRAME_W = 320;
    static constexpr uint16_t FRAME_H = 170;
    static constexpr uint32_t FRAME_PIXELS = (uint32_t)FRAME_W * FRAME_H;
    static constexpr uint32_t MAX_RUNS = FRAME_PIXELS;
    static constexpr uint32_t SERIAL_BAUD = 921600;

    static constexpr uint8_t MAGIC[4] = {'J', 'W', 'H', '1'};

    uint16_t g_frame[FRAME_PIXELS] = {};
    volatile bool g_frameReady = false;
    volatile uint32_t g_frameSequence = 0;

    uint16_t readU16LE(const uint8_t *p)
    {
        return (uint16_t)p[0] |
               ((uint16_t)p[1] << 8);
    }

    uint32_t readU32LE(const uint8_t *p)
    {
        return (uint32_t)p[0] |
               ((uint32_t)p[1] << 8) |
               ((uint32_t)p[2] << 16) |
               ((uint32_t)p[3] << 24);
    }

    bool readExact(uint8_t *dst, size_t length, uint32_t timeoutMs = 1000)
    {
        const uint32_t started = millis();
        size_t received = 0;

        while (received < length)
        {
            while (Serial.available() > 0 && received < length)
            {
                dst[received++] = (uint8_t)Serial.read();
            }

            if (received >= length)
            {
                return true;
            }

            if (millis() - started >= timeoutMs)
            {
                return false;
            }

            delay(0);
        }

        return true;
    }

    bool waitForMagic()
    {
        static uint8_t matched = 0;

        while (Serial.available() > 0)
        {
            const uint8_t value = (uint8_t)Serial.read();

            if (value == MAGIC[matched])
            {
                ++matched;
                if (matched == sizeof(MAGIC))
                {
                    matched = 0;
                    return true;
                }
            }
            else
            {
                matched = (value == MAGIC[0]) ? 1 : 0;
            }
        }

        return false;
    }

    bool receiveFrame()
    {
        // Header posterior a JWH1:
        //   u32 sequence
        //   u32 runCount
        //   u16 width
        //   u16 height
        uint8_t header[12] = {};
        if (!readExact(header, sizeof(header)))
        {
            Serial.println("JWHMI_LIVE_ERROR header_timeout");
            return false;
        }

        const uint32_t sequence = readU32LE(header + 0);
        const uint32_t runCount = readU32LE(header + 4);
        const uint16_t width = readU16LE(header + 8);
        const uint16_t height = readU16LE(header + 10);

        if (width != FRAME_W || height != FRAME_H ||
            runCount == 0 || runCount > MAX_RUNS)
        {
            Serial.println("JWHMI_LIVE_ERROR invalid_header");
            return false;
        }

        // No sobrescribir un frame que todavía está siendo consumido por
        // jwplcUIUpdate().
        const uint32_t waitStarted = millis();
        while (g_frameReady)
        {
            if (millis() - waitStarted > 500)
            {
                Serial.println("JWHMI_LIVE_ERROR display_busy");
                return false;
            }
            delay(1);
        }

        uint32_t pixelIndex = 0;
        uint8_t pair[4] = {};

        for (uint32_t i = 0; i < runCount; ++i)
        {
            if (!readExact(pair, sizeof(pair)))
            {
                Serial.println("JWHMI_LIVE_ERROR payload_timeout");
                return false;
            }

            const uint16_t count = readU16LE(pair + 0);
            const uint16_t color = readU16LE(pair + 2);

            if (count == 0 || pixelIndex + count > FRAME_PIXELS)
            {
                Serial.println("JWHMI_LIVE_ERROR invalid_run");
                return false;
            }

            for (uint16_t p = 0; p < count; ++p)
            {
                g_frame[pixelIndex++] = color;
            }
        }

        if (pixelIndex != FRAME_PIXELS)
        {
            Serial.println("JWHMI_LIVE_ERROR incomplete_frame");
            return false;
        }

        g_frameSequence = sequence;
        g_frameReady = true;
        JWPLC_Display.requestUserRefresh();
        return true;
    }
}

extern "C" bool jwplcCanReturnToIdle(void)
{
    return false;
}

extern "C" void jwplcUIUpdate(void)
{
    if (!g_frameReady)
    {
        return;
    }

    // El acceso directo a tft() está encapsulado exclusivamente en este bridge
    // de desarrollo. El codegen del Designer NO genera llamadas tft.*.
    auto &tft = JWPLC_Display.tft();
    tft.drawRGBBitmap(0, 0, g_frame, FRAME_W, FRAME_H);

    const uint32_t sequence = g_frameSequence;
    g_frameReady = false;

    Serial.print("JWHMI_LIVE_FRAME ");
    Serial.println(sequence);
}

void setup()
{
    Serial.begin(SERIAL_BAUD);
    delay(250);

    // El bridge mantiene USER visible y deja el refresco bajo demanda.
    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
    JWPLC_Display.clearFields();
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.enterUserUI();

    delay(150);
    Serial.println("JWHMI_LIVE_READY 1");
}

void loop()
{
    if (waitForMagic())
    {
        receiveFrame();
    }

    delay(0);
}

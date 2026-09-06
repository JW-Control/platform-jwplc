#include <JWPLC_Display.h>
#include <jwplc_spi_bus.h>

// =============================================================
// JWPLC HMI Designer — Live Preview Bridge
// Alpha11 · herramienta de desarrollo, NO runtime de producción.
//
// Transporte:
//   Web Serial @ 921600 baud
//   RLE RGB565 del framebuffer lógico 320x170.
//
// Importante:
//   El JWPLC Basic v2 no dispone de PSRAM. Por ello el bridge NO reserva
//   un framebuffer completo de 320x170x16 bits (~106 KiB). El RLE recibido
//   se decodifica de forma streaming usando sólo una línea RGB565 (640 B)
//   y cada línea se envía a la TFT bajo el mutex SPI compartido.
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
    static constexpr size_t SERIAL_RX_BUFFER = 4096;

    static constexpr uint8_t MAGIC[4] = {'J', 'W', 'H', '1'};

    // Sólo una línea: 320 * 2 bytes = 640 bytes.
    uint16_t g_row[FRAME_W] = {};

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

    bool drawRow(uint16_t y)
    {
        if (y >= FRAME_H)
        {
            return false;
        }

        // El display comparte SPI con Ethernet, SD y FRAM. Aunque este sketch
        // sea un bridge de desarrollo, respetamos el mutex común del package.
        if (!jwplcSPI_acquire(50))
        {
            return false;
        }

        jwplcSPI_prepareForTFT();
        JWPLC_Display.tft().drawRGBBitmap(0, (int16_t)y, g_row, FRAME_W, 1);
        jwplcSPI_release();
        return true;
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

        uint32_t pixelIndex = 0;
        uint16_t rowFill = 0;
        uint16_t rowY = 0;
        uint8_t pair[4] = {};

        for (uint32_t i = 0; i < runCount; ++i)
        {
            if (!readExact(pair, sizeof(pair)))
            {
                Serial.println("JWHMI_LIVE_ERROR payload_timeout");
                return false;
            }

            uint16_t remaining = readU16LE(pair + 0);
            const uint16_t color = readU16LE(pair + 2);

            if (remaining == 0 || pixelIndex + remaining > FRAME_PIXELS)
            {
                Serial.println("JWHMI_LIVE_ERROR invalid_run");
                return false;
            }

            while (remaining > 0)
            {
                const uint16_t freeInRow = FRAME_W - rowFill;
                const uint16_t chunk = (remaining < freeInRow)
                                           ? remaining
                                           : freeInRow;

                for (uint16_t p = 0; p < chunk; ++p)
                {
                    g_row[rowFill + p] = color;
                }

                rowFill += chunk;
                pixelIndex += chunk;
                remaining -= chunk;

                if (rowFill == FRAME_W)
                {
                    if (!drawRow(rowY))
                    {
                        Serial.println("JWHMI_LIVE_ERROR spi_busy");
                        return false;
                    }

                    rowFill = 0;
                    ++rowY;
                }
            }
        }

        if (pixelIndex != FRAME_PIXELS || rowFill != 0 || rowY != FRAME_H)
        {
            Serial.println("JWHMI_LIVE_ERROR incomplete_frame");
            return false;
        }

        Serial.print("JWHMI_LIVE_FRAME ");
        Serial.println(sequence);
        return true;
    }
}

extern "C" bool jwplcCanReturnToIdle(void)
{
    return false;
}

// Evita que el servicio periódico del display intente refrescar mientras el
// bridge dibuja las líneas directamente bajo el mutex SPI. enterUserUI() puede
// realizar su transición inicial; después el framebuffer lo gobierna el LIVE.
extern "C" bool jwplcUserDisplayRefreshNeededCallback(
    const JWPLC_IOState *io,
    const JWPLC_RTCState *rtc)
{
    (void)io;
    (void)rtc;
    return false;
}

extern "C" void jwplcUIUpdate(void)
{
    // Intencionalmente vacío: el LIVE se pinta en streaming desde loop().
}

void setup()
{
    Serial.setRxBufferSize(SERIAL_RX_BUFFER);
    Serial.begin(SERIAL_BAUD);
    delay(250);

    JWPLC_Display.setIdleWakeMode(IDLE_WAKE_DISABLED);
    JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
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

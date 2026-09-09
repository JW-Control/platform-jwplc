#include <JWPLC_Display.h>
#include <jwplc_spi_bus.h>

// =============================================================
// JWPLC HMI Designer — Live Preview Bridge
// Alpha11 · herramienta de desarrollo, NO runtime de producción.
//
// Transporte:
//   Web Serial @ 921600 baud
//   RGB565 + RLE.
//
// Protocolos:
//   JWH1 = framebuffer FULL 320x170.
//   JWH2 = REGION rectangular del framebuffer.
//   JWH? = probe para solicitar READY.
//
// Flujo:
//   El host envía una imagen y espera JWHMI_LIVE_FRAME <seq> antes de
//   transmitir la siguiente. Durante edición rápida el host conserva sólo
//   el estado más reciente y no acumula frames obsoletos.
//
// Buffer de dibujo:
//   El JWPLC Basic v2 no dispone de PSRAM. No reservamos el framebuffer
//   completo de 320x170x16 bits (~106 KiB). FULL y REGION se decodifican en
//   un framebuffer parcial de hasta 32 filas (20 KiB máximo) y se dibujan
//   por bloques respetando el mutex SPI compartido.
//
// Telemetría:
//   Cada 30 imágenes confirmadas se publica JWHMI_LIVE_STATS con tiempos,
//   memoria, FULL/REGION y errores acumulados.
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
    static constexpr uint32_t SERIAL_BAUD = 921600;
    static constexpr size_t SERIAL_RX_BUFFER = 8192;

    static constexpr uint16_t FRAME_BUFFER_ROWS = 32;
    static constexpr uint32_t FRAME_BUFFER_PIXELS =
        (uint32_t)FRAME_W * FRAME_BUFFER_ROWS;
    static constexpr uint32_t STATS_INTERVAL_FRAMES = 30;

    enum InputCommand : uint8_t
    {
        INPUT_NONE = 0,
        INPUT_FULL,
        INPUT_REGION,
        INPUT_PROBE
    };

    struct LiveStats
    {
        uint32_t frames = 0;
        uint32_t frameUsTotal = 0;
        uint32_t frameUsMax = 0;
        uint32_t rxUsTotal = 0;
        uint32_t drawUsTotal = 0;
        uint32_t drawUsMax = 0;
        uint32_t fullFrames = 0;
        uint32_t regionFrames = 0;
        uint32_t regionPixelsTotal = 0;
        uint32_t errors = 0;
    };

    // Framebuffer PARCIAL máximo: 320 * 32 * 2 bytes = 20,480 bytes.
    uint16_t g_frame[FRAME_BUFFER_PIXELS] = {};
    LiveStats g_stats;

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

    void sendReady()
    {
        Serial.println("JWHMI_LIVE_READY 1");
    }

    void reportError(const char *reason)
    {
        ++g_stats.errors;
        Serial.print("JWHMI_LIVE_ERROR ");
        Serial.println(reason);
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

    InputCommand pollInputCommand()
    {
        static uint8_t matched = 0;

        while (Serial.available() > 0)
        {
            const uint8_t value = (uint8_t)Serial.read();

            if (matched == 0)
            {
                matched = (value == 'J') ? 1 : 0;
                continue;
            }

            if (matched == 1)
            {
                if (value == 'W')
                {
                    matched = 2;
                }
                else
                {
                    matched = (value == 'J') ? 1 : 0;
                }
                continue;
            }

            if (matched == 2)
            {
                if (value == 'H')
                {
                    matched = 3;
                }
                else
                {
                    matched = (value == 'J') ? 1 : 0;
                }
                continue;
            }

            // matched == 3: ya recibimos JWH.
            matched = 0;

            if (value == '1')
            {
                return INPUT_FULL;
            }

            if (value == '2')
            {
                return INPUT_REGION;
            }

            if (value == '?')
            {
                return INPUT_PROBE;
            }

            if (value == 'J')
            {
                matched = 1;
            }
        }

        return INPUT_NONE;
    }

    bool drawBlock(
        uint16_t x,
        uint16_t startY,
        uint16_t width,
        uint16_t rows,
        uint32_t &drawUs)
    {
        if (width == 0 || rows == 0 ||
            x >= FRAME_W || startY >= FRAME_H ||
            (uint32_t)x + width > FRAME_W ||
            (uint32_t)startY + rows > FRAME_H)
        {
            return false;
        }

        const uint32_t started = micros();

        // El display comparte SPI con Ethernet, SD y FRAM. Aunque este sketch
        // sea un bridge de desarrollo, respetamos el mutex común del package.
        if (!jwplcSPI_acquire(50))
        {
            drawUs += micros() - started;
            return false;
        }

        jwplcSPI_prepareForTFT();
        JWPLC_Display.tft().drawRGBBitmap(
            (int16_t)x,
            (int16_t)startY,
            g_frame,
            (int16_t)width,
            (int16_t)rows);
        jwplcSPI_release();

        drawUs += micros() - started;
        return true;
    }

    void sendStatsIfNeeded()
    {
        if (g_stats.frames < STATS_INTERVAL_FRAMES)
        {
            return;
        }

        const uint32_t frames = g_stats.frames;
        const uint32_t frameUsAvg = g_stats.frameUsTotal / frames;
        const uint32_t rxUsAvg = g_stats.rxUsTotal / frames;
        const uint32_t drawUsAvg = g_stats.drawUsTotal / frames;
        const uint32_t regionPixelsAvg =
            (g_stats.regionFrames > 0)
                ? (g_stats.regionPixelsTotal / g_stats.regionFrames)
                : 0;

        Serial.print("JWHMI_LIVE_STATS frames=");
        Serial.print(frames);
        Serial.print(" frame_us_avg=");
        Serial.print(frameUsAvg);
        Serial.print(" frame_us_max=");
        Serial.print(g_stats.frameUsMax);
        Serial.print(" rx_us_avg=");
        Serial.print(rxUsAvg);
        Serial.print(" draw_us_avg=");
        Serial.print(drawUsAvg);
        Serial.print(" draw_us_max=");
        Serial.print(g_stats.drawUsMax);
        Serial.print(" full=");
        Serial.print(g_stats.fullFrames);
        Serial.print(" region=");
        Serial.print(g_stats.regionFrames);
        Serial.print(" region_px_avg=");
        Serial.print(regionPixelsAvg);
        Serial.print(" free=");
        Serial.print(ESP.getFreeHeap());
        Serial.print(" min=");
        Serial.print(ESP.getMinFreeHeap());
        Serial.print(" largest=");
        Serial.print(ESP.getMaxAllocHeap());
        Serial.print(" errors=");
        Serial.print(g_stats.errors);
        Serial.print(" buffer_rows=");
        Serial.println(FRAME_BUFFER_ROWS);

        // Las métricas por ventana se reinician. El contador de errores queda
        // acumulado para detectar fallos durante un soak largo.
        g_stats.frames = 0;
        g_stats.frameUsTotal = 0;
        g_stats.frameUsMax = 0;
        g_stats.rxUsTotal = 0;
        g_stats.drawUsTotal = 0;
        g_stats.drawUsMax = 0;
        g_stats.fullFrames = 0;
        g_stats.regionFrames = 0;
        g_stats.regionPixelsTotal = 0;
    }

    void recordFrameStats(
        uint32_t frameUs,
        uint32_t drawUs,
        bool regionMode,
        uint32_t targetPixels)
    {
        const uint32_t rxUs = (frameUs >= drawUs)
                                  ? frameUs - drawUs
                                  : 0;

        ++g_stats.frames;
        g_stats.frameUsTotal += frameUs;
        g_stats.rxUsTotal += rxUs;
        g_stats.drawUsTotal += drawUs;

        if (regionMode)
        {
            ++g_stats.regionFrames;
            g_stats.regionPixelsTotal += targetPixels;
        }
        else
        {
            ++g_stats.fullFrames;
        }

        if (frameUs > g_stats.frameUsMax)
        {
            g_stats.frameUsMax = frameUs;
        }

        if (drawUs > g_stats.drawUsMax)
        {
            g_stats.drawUsMax = drawUs;
        }
    }

    bool receiveImage(bool regionMode)
    {
        const uint32_t frameStarted = micros();
        uint32_t drawUs = 0;

        uint32_t sequence = 0;
        uint32_t runCount = 0;
        uint16_t x = 0;
        uint16_t y = 0;
        uint16_t width = FRAME_W;
        uint16_t height = FRAME_H;

        if (regionMode)
        {
            // Header posterior a JWH2:
            //   u32 sequence
            //   u32 runCount
            //   u16 x
            //   u16 y
            //   u16 width
            //   u16 height
            uint8_t header[16] = {};
            if (!readExact(header, sizeof(header)))
            {
                reportError("region_header_timeout");
                return false;
            }

            sequence = readU32LE(header + 0);
            runCount = readU32LE(header + 4);
            x = readU16LE(header + 8);
            y = readU16LE(header + 10);
            width = readU16LE(header + 12);
            height = readU16LE(header + 14);
        }
        else
        {
            // Header posterior a JWH1:
            //   u32 sequence
            //   u32 runCount
            //   u16 width
            //   u16 height
            uint8_t header[12] = {};
            if (!readExact(header, sizeof(header)))
            {
                reportError("full_header_timeout");
                return false;
            }

            sequence = readU32LE(header + 0);
            runCount = readU32LE(header + 4);
            width = readU16LE(header + 8);
            height = readU16LE(header + 10);

            if (width != FRAME_W || height != FRAME_H)
            {
                reportError("invalid_full_size");
                return false;
            }
        }

        if (width == 0 || height == 0 ||
            x >= FRAME_W || y >= FRAME_H ||
            (uint32_t)x + width > FRAME_W ||
            (uint32_t)y + height > FRAME_H)
        {
            reportError("invalid_region");
            return false;
        }

        const uint32_t targetPixels = (uint32_t)width * height;
        if (runCount == 0 || runCount > targetPixels)
        {
            reportError("invalid_run_count");
            return false;
        }

        const uint16_t rowsPerBlock =
            (height < FRAME_BUFFER_ROWS) ? height : FRAME_BUFFER_ROWS;
        const uint32_t blockCapacity = (uint32_t)width * rowsPerBlock;

        if (blockCapacity == 0 || blockCapacity > FRAME_BUFFER_PIXELS)
        {
            reportError("invalid_block_capacity");
            return false;
        }

        uint32_t pixelIndex = 0;
        uint32_t blockFill = 0;
        uint16_t blockStartY = y;
        uint8_t pair[4] = {};

        for (uint32_t i = 0; i < runCount; ++i)
        {
            if (!readExact(pair, sizeof(pair)))
            {
                reportError("payload_timeout");
                return false;
            }

            uint32_t remaining = readU16LE(pair + 0);
            const uint16_t color = readU16LE(pair + 2);

            if (remaining == 0 || pixelIndex + remaining > targetPixels)
            {
                reportError("invalid_run");
                return false;
            }

            while (remaining > 0)
            {
                const uint32_t freeInBlock = blockCapacity - blockFill;
                const uint32_t chunk =
                    (remaining < freeInBlock) ? remaining : freeInBlock;

                for (uint32_t p = 0; p < chunk; ++p)
                {
                    g_frame[blockFill + p] = color;
                }

                blockFill += chunk;
                pixelIndex += chunk;
                remaining -= chunk;

                if (blockFill == blockCapacity)
                {
                    if (!drawBlock(x, blockStartY, width, rowsPerBlock, drawUs))
                    {
                        reportError("spi_busy");
                        return false;
                    }

                    blockStartY += rowsPerBlock;
                    blockFill = 0;
                }
            }
        }

        if (blockFill > 0)
        {
            if ((blockFill % width) != 0)
            {
                reportError("incomplete_block");
                return false;
            }

            const uint16_t rows = (uint16_t)(blockFill / width);
            if (!drawBlock(x, blockStartY, width, rows, drawUs))
            {
                reportError("spi_busy");
                return false;
            }

            blockStartY += rows;
            blockFill = 0;
        }

        if (pixelIndex != targetPixels ||
            blockStartY != (uint16_t)(y + height))
        {
            reportError("incomplete_image");
            return false;
        }

        const uint32_t frameUs = micros() - frameStarted;
        recordFrameStats(frameUs, drawUs, regionMode, targetPixels);

        // ACK común para FULL y REGION. Sale antes de la telemetría para
        // liberar al host lo antes posible.
        Serial.print("JWHMI_LIVE_FRAME ");
        Serial.println(sequence);
        sendStatsIfNeeded();
        return true;
    }
}

extern "C" bool jwplcCanReturnToIdle(void)
{
    return false;
}

// Evita que el servicio periódico del display intente refrescar mientras el
// bridge dibuja directamente bajo el mutex SPI. enterUserUI() puede realizar
// su transición inicial; después el framebuffer lo gobierna el LIVE.
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
    // Intencionalmente vacío: el LIVE se pinta desde loop().
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
    sendReady();
}

void loop()
{
    const InputCommand command = pollInputCommand();

    if (command == INPUT_PROBE)
    {
        // El navegador puede abrir el COM mucho después de setup().
        // Responder al probe evita depender de capturar el READY inicial.
        sendReady();
    }
    else if (command == INPUT_FULL)
    {
        receiveImage(false);
    }
    else if (command == INPUT_REGION)
    {
        receiveImage(true);
    }

    delay(0);
}

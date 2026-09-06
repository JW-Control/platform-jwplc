#include <JWPLC_Display.h>
#include <jwplc_spi_bus.h>

// =============================================================
// JWPLC HMI Designer — Live Preview Bridge
// Alpha11 · herramienta de desarrollo, NO runtime de producción.
//
// Transporte:
//   Web Serial @ 500000 baud
//   RLE RGB565 del framebuffer lógico 320x170.
//
// Handshake:
//   Host -> JWH?
//   Bridge -> JWHMI_LIVE_READY 1
//
// Flujo:
//   El host envía un frame y espera JWHMI_LIVE_FRAME <seq> antes de
//   transmitir el siguiente. Esto evita acumular frames durante drag/edición.
//
// Buffer de dibujo:
//   El JWPLC Basic v2 no dispone de PSRAM. No reservamos el framebuffer
//   completo de 320x170x16 bits (~106 KiB). El RLE se decodifica en un
//   framebuffer parcial de 16 filas (10 KiB) y se envía a la TFT por bloques.
//   Esto reduce drásticamente adquisiciones del mutex SPI frente al buffer de
//   una sola fila, manteniendo un consumo de RAM muy inferior al frame completo.
//
// Telemetría:
//   Cada 30 frames se publica JWHMI_LIVE_STATS con tiempos de frame/dibujo,
//   heap libre/mínimo/bloque mayor y contador de errores del bridge.
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
    static constexpr uint32_t SERIAL_BAUD = 500000;
    static constexpr size_t SERIAL_RX_BUFFER = 8192;

    static constexpr uint16_t FRAME_BUFFER_ROWS = 16;
    static constexpr uint32_t FRAME_BUFFER_PIXELS =
        (uint32_t)FRAME_W * FRAME_BUFFER_ROWS;
    static constexpr uint32_t STATS_INTERVAL_FRAMES = 30;

    // JWH1 = inicio de frame binario.
    // JWH? = probe corto del navegador para solicitar READY.
    static constexpr uint8_t FRAME_MAGIC[4] = {'J', 'W', 'H', '1'};

    enum InputCommand : uint8_t
    {
        INPUT_NONE = 0,
        INPUT_FRAME,
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
        uint32_t errors = 0;
    };

    // Framebuffer PARCIAL: 320 * 16 * 2 bytes = 10,240 bytes.
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

            if (value == FRAME_MAGIC[3])
            {
                return INPUT_FRAME;
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

    bool drawBlock(uint16_t startY, uint16_t rows, uint32_t &drawUs)
    {
        if (rows == 0 || startY >= FRAME_H || startY + rows > FRAME_H)
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
            0,
            (int16_t)startY,
            g_frame,
            FRAME_W,
            rows);
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

        // Las métricas temporales se reinician por ventana; el contador de
        // errores se conserva para detectar fallos acumulados durante el soak.
        g_stats.frames = 0;
        g_stats.frameUsTotal = 0;
        g_stats.frameUsMax = 0;
        g_stats.rxUsTotal = 0;
        g_stats.drawUsTotal = 0;
        g_stats.drawUsMax = 0;
    }

    void recordFrameStats(uint32_t frameUs, uint32_t drawUs)
    {
        const uint32_t rxUs = (frameUs >= drawUs)
                                  ? frameUs - drawUs
                                  : 0;

        ++g_stats.frames;
        g_stats.frameUsTotal += frameUs;
        g_stats.rxUsTotal += rxUs;
        g_stats.drawUsTotal += drawUs;

        if (frameUs > g_stats.frameUsMax)
        {
            g_stats.frameUsMax = frameUs;
        }

        if (drawUs > g_stats.drawUsMax)
        {
            g_stats.drawUsMax = drawUs;
        }
    }

    bool receiveFrame()
    {
        const uint32_t frameStarted = micros();
        uint32_t drawUs = 0;

        // Header posterior a JWH1:
        //   u32 sequence
        //   u32 runCount
        //   u16 width
        //   u16 height
        uint8_t header[12] = {};
        if (!readExact(header, sizeof(header)))
        {
            reportError("header_timeout");
            return false;
        }

        const uint32_t sequence = readU32LE(header + 0);
        const uint32_t runCount = readU32LE(header + 4);
        const uint16_t width = readU16LE(header + 8);
        const uint16_t height = readU16LE(header + 10);

        if (width != FRAME_W || height != FRAME_H ||
            runCount == 0 || runCount > MAX_RUNS)
        {
            reportError("invalid_header");
            return false;
        }

        uint32_t pixelIndex = 0;
        uint32_t blockFill = 0;
        uint16_t blockStartY = 0;
        uint8_t pair[4] = {};

        for (uint32_t i = 0; i < runCount; ++i)
        {
            if (!readExact(pair, sizeof(pair)))
            {
                reportError("payload_timeout");
                return false;
            }

            uint16_t remaining = readU16LE(pair + 0);
            const uint16_t color = readU16LE(pair + 2);

            if (remaining == 0 || pixelIndex + remaining > FRAME_PIXELS)
            {
                reportError("invalid_run");
                return false;
            }

            while (remaining > 0)
            {
                const uint32_t freeInBlock = FRAME_BUFFER_PIXELS - blockFill;
                const uint16_t chunk =
                    (remaining < freeInBlock)
                        ? remaining
                        : (uint16_t)freeInBlock;

                for (uint16_t p = 0; p < chunk; ++p)
                {
                    g_frame[blockFill + p] = color;
                }

                blockFill += chunk;
                pixelIndex += chunk;
                remaining -= chunk;

                if (blockFill == FRAME_BUFFER_PIXELS)
                {
                    if (!drawBlock(blockStartY, FRAME_BUFFER_ROWS, drawUs))
                    {
                        reportError("spi_busy");
                        return false;
                    }

                    blockStartY += FRAME_BUFFER_ROWS;
                    blockFill = 0;
                }
            }
        }

        // 170 no es múltiplo de 16: el último bloque contiene 10 filas.
        if (blockFill > 0)
        {
            if ((blockFill % FRAME_W) != 0)
            {
                reportError("incomplete_block");
                return false;
            }

            const uint16_t rows = (uint16_t)(blockFill / FRAME_W);
            if (!drawBlock(blockStartY, rows, drawUs))
            {
                reportError("spi_busy");
                return false;
            }

            blockStartY += rows;
            blockFill = 0;
        }

        if (pixelIndex != FRAME_PIXELS || blockStartY != FRAME_H)
        {
            reportError("incomplete_frame");
            return false;
        }

        const uint32_t frameUs = micros() - frameStarted;
        recordFrameStats(frameUs, drawUs);

        Serial.print("JWHMI_LIVE_FRAME ");
        Serial.println(sequence);

        // El ACK sale primero para liberar al host lo antes posible.
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
    else if (command == INPUT_FRAME)
    {
        receiveFrame();
    }

    delay(0);
}

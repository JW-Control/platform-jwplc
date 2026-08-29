#include <JWPLC_Bundled_Adafruit_GFX.h>
#include <Adafruit_GFX.h>

GFXcanvas1 canvas(16, 16);
volatile uint8_t jwplcGfxGateSink = 0;

void setup()
{
    canvas.fillScreen(0);
    canvas.drawPixel(1, 1, 1);
    canvas.drawLine(0, 0, 15, 15, 1);
    canvas.drawRect(2, 2, 8, 8, 1);

    jwplcGfxGateSink = canvas.getPixel(1, 1) ? 1 : 0;
}

void loop()
{
}

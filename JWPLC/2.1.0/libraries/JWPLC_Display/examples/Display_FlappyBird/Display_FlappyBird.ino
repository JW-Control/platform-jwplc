/*
  JWPLC_FlappyBird_Optimized.ino

  Flappy Bird optimizado para JWPLC Basic 2.1.0.

  Requiere JWPLC_Display con:
  - setUserRefreshPeriodMs() respetado por el core.
  - setIdleWakeMode()/setIdleReturnMode() configurables desde setup().

  Controles:
  - OK / UP = saltar
  - ESC     = volver a IDLE

  Notas:
  - Ya no usa workaround con JWPLC_Display.forceRedraw().
  - La configuracion de transiciones y refresh se hace desde setup().
  - Solo se espera JWPLC_Display.isReady() para confirmar que ya se puede dibujar/loguear.
*/

#include <JWPLC_Display.h>

// =====================================================
// Configuracion general
// =====================================================

// 30 FPS recomendado. Con el clamp actual del core, 50 FPS es el techo practico.
static const uint8_t GAME_TARGET_FPS = 30;
static const uint32_t FRAME_PERIOD_MS = 1000UL / GAME_TARGET_FPS;

// Velocidad global del juego.
// 100 = velocidad nominal.
// 75  = demo comoda.
// 60  = mas facil.
static const uint16_t GAME_SPEED_PERCENT = 75;

static const uint32_t GAME_OVER_HOLD_MS = 1200;
static const uint32_t WAIT_OK_HINT_MS = 900;

static const bool ENABLE_SERIAL_DEBUG = true;

// =====================================================
// Colores
// =====================================================

static const uint16_t COLOR_BG      = ST77XX_BLACK;
static const uint16_t COLOR_SKY     = ST77XX_BLUE;
static const uint16_t COLOR_GROUND  = ST77XX_GREEN;
static const uint16_t COLOR_PIPE    = ST77XX_GREEN;
static const uint16_t COLOR_PIPE_B  = ST77XX_WHITE;
static const uint16_t COLOR_BIRD    = ST77XX_YELLOW;
static const uint16_t COLOR_BIRD_B  = ST77XX_BLACK;
static const uint16_t COLOR_BEAK    = ST77XX_RED;
static const uint16_t COLOR_WING    = ST77XX_ORANGE;
static const uint16_t COLOR_TEXT    = ST77XX_WHITE;
static const uint16_t COLOR_TITLE   = ST77XX_CYAN;

// =====================================================
// Configuracion del juego
// =====================================================

// Pollito circular.
// DRAW_R: radio visual.
// COLLISION_R: radio logico, un poco menor para que la colision sea mas justa.
static const int16_t BIRD_DRAW_R = 7;
static const int16_t BIRD_COLLISION_R = 5;

static const int16_t PIPE_W = 22;
static const int16_t PIPE_GAP = 48;
static const int16_t PIPE_SPACING = 105;

// Fisica en unidades por segundo para no depender del FPS.
static const int16_t PIPE_SPEED_PX_S = 90;
static const int16_t GRAVITY_PX_S2   = 270;
static const int16_t FLAP_VEL_PX_S   = -145;
static const int16_t MAX_FALL_PX_S   = 170;

static const uint32_t MAX_SIM_DT_MS = 80;

enum GameState : uint8_t
{
  GAME_WAIT_OK = 0,
  GAME_RUNNING,
  GAME_OVER
};

struct Pipe
{
  int32_t x100;
  int16_t x;
  int16_t gapY;
  bool scored;
};

struct RectI
{
  int16_t x;
  int16_t y;
  int16_t w;
  int16_t h;
};

static GameState gameState = GAME_WAIT_OK;

static Pipe pipes[3];

static int16_t screenW = 0;
static int16_t screenH = 0;

static int16_t gameTop = 18;
static int16_t groundY = 0;

static int16_t birdX = 0;
static int32_t birdY100 = 0;
static int32_t birdV100 = 0;

static uint16_t score = 0;
static uint16_t bestScore = 0;

static uint32_t lastFrameMs = 0;
static uint32_t lastSimMs = 0;
static uint32_t gameOverMs = 0;
static uint32_t userEnterMs = 0;

static bool gameOverDrawn = false;

// Metricas
static uint32_t renderedFrames = 0;
static uint32_t fpsWindowStartMs = 0;
static uint16_t measuredFps = 0;

// Ultimos valores dibujados en header
static uint16_t lastDrawScore = 0xFFFF;
static uint16_t lastDrawBest = 0xFFFF;
static uint16_t lastDrawFps = 0xFFFF;

// =====================================================
// Utilidades
// =====================================================

static RectI makeRect(int16_t x, int16_t y, int16_t w, int16_t h)
{
  RectI r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  return r;
}

static RectI unionRect(const RectI &a, const RectI &b)
{
  int16_t ax1 = a.x + a.w;
  int16_t ay1 = a.y + a.h;
  int16_t bx1 = b.x + b.w;
  int16_t by1 = b.y + b.h;

  int16_t x0 = (a.x < b.x) ? a.x : b.x;
  int16_t y0 = (a.y < b.y) ? a.y : b.y;
  int16_t x1 = (ax1 > bx1) ? ax1 : bx1;
  int16_t y1 = (ay1 > by1) ? ay1 : by1;

  return makeRect(x0, y0, x1 - x0, y1 - y0);
}

static bool rectIntersects(const RectI &a, const RectI &b)
{
  if (a.w <= 0 || a.h <= 0 || b.w <= 0 || b.h <= 0)
  {
    return false;
  }

  return !(
    (a.x + a.w <= b.x) ||
    (b.x + b.w <= a.x) ||
    (a.y + a.h <= b.y) ||
    (b.y + b.h <= a.y)
  );
}

static int16_t x100ToPx(int32_t value100)
{
  if (value100 >= 0)
  {
    return (int16_t)((value100 + 50) / 100);
  }

  return (int16_t)(-(((-value100) + 50) / 100));
}

static int16_t birdY()
{
  return x100ToPx(birdY100);
}

static void debugPrintln(const char *msg)
{
  if (ENABLE_SERIAL_DEBUG)
  {
    Serial.println(msg);
  }
}

static void cacheScreenGeometry()
{
  auto &tft = JWPLC_Display.tft();

  screenW = tft.width();
  screenH = tft.height();

  gameTop = 18;
  groundY = screenH - 13;
  birdX = screenW / 4;

  if (screenW < 100 || screenH < 80)
  {
    screenW = 320;
    screenH = 170;
    gameTop = 18;
    groundY = screenH - 13;
    birdX = screenW / 4;
  }
}

static uint32_t scaleDtMs(uint32_t dtMs)
{
  if (dtMs > MAX_SIM_DT_MS)
  {
    dtMs = MAX_SIM_DT_MS;
  }

  return (dtMs * GAME_SPEED_PERCENT) / 100UL;
}

// =====================================================
// Primitivas con clipping
// =====================================================

static void fillRectClip(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (w <= 0 || h <= 0)
  {
    return;
  }

  int16_t x0 = x;
  int16_t y0 = y;
  int16_t x1 = x + w;
  int16_t y1 = y + h;

  if (x1 <= 0 || y1 <= 0 || x0 >= screenW || y0 >= screenH)
  {
    return;
  }

  if (x0 < 0) x0 = 0;
  if (y0 < 0) y0 = 0;
  if (x1 > screenW) x1 = screenW;
  if (y1 > screenH) y1 = screenH;

  if (x1 <= x0 || y1 <= y0)
  {
    return;
  }

  JWPLC_Display.tft().fillRect(x0, y0, x1 - x0, y1 - y0, color);
}

static void drawHLineClip(int16_t x, int16_t y, int16_t w, uint16_t color)
{
  if (w <= 0 || y < 0 || y >= screenH)
  {
    return;
  }

  int16_t x0 = x;
  int16_t x1 = x + w;

  if (x1 <= 0 || x0 >= screenW)
  {
    return;
  }

  if (x0 < 0) x0 = 0;
  if (x1 > screenW) x1 = screenW;

  if (x1 <= x0)
  {
    return;
  }

  JWPLC_Display.tft().drawFastHLine(x0, y, x1 - x0, color);
}

static void drawVLineClip(int16_t x, int16_t y, int16_t h, uint16_t color)
{
  if (h <= 0 || x < 0 || x >= screenW)
  {
    return;
  }

  int16_t y0 = y;
  int16_t y1 = y + h;

  if (y1 <= 0 || y0 >= screenH)
  {
    return;
  }

  if (y0 < 0) y0 = 0;
  if (y1 > screenH) y1 = screenH;

  if (y1 <= y0)
  {
    return;
  }

  JWPLC_Display.tft().drawFastVLine(x, y0, y1 - y0, color);
}

static void drawRectClip(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
  if (w <= 0 || h <= 0)
  {
    return;
  }

  drawHLineClip(x, y, w, color);
  drawHLineClip(x, y + h - 1, w, color);
  drawVLineClip(x, y, h, color);
  drawVLineClip(x + w - 1, y, h, color);
}

// =====================================================
// FPS / Header
// =====================================================

static void resetHeaderCache()
{
  lastDrawScore = 0xFFFF;
  lastDrawBest = 0xFFFF;
  lastDrawFps = 0xFFFF;
}

static void updateMeasuredFps()
{
  renderedFrames++;

  uint32_t now = millis();

  if (fpsWindowStartMs == 0)
  {
    fpsWindowStartMs = now;
    renderedFrames = 0;
    measuredFps = 0;
    return;
  }

  uint32_t elapsed = now - fpsWindowStartMs;

  if (elapsed >= 1000)
  {
    measuredFps = (uint16_t)((renderedFrames * 1000UL) / elapsed);
    renderedFrames = 0;
    fpsWindowStartMs = now;
  }
}

static void drawHeaderLabels()
{
  auto &tft = JWPLC_Display.tft();

  tft.fillRect(0, 0, screenW, gameTop, COLOR_BG);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(4, 5);
  tft.print("S:");

  tft.setCursor(46, 5);
  tft.print("B:");

  tft.setCursor(96, 5);
  tft.print("FPS:");

  tft.setCursor(screenW - 72, 5);
  tft.print("V:");
  tft.print(GAME_SPEED_PERCENT);
  tft.print("%");
}

static void drawHeaderValues(bool force)
{
  auto &tft = JWPLC_Display.tft();

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  if (force || score != lastDrawScore)
  {
    lastDrawScore = score;

    tft.fillRect(18, 4, 25, 10, COLOR_BG);
    tft.setCursor(18, 5);
    tft.print(score);
  }

  if (force || bestScore != lastDrawBest)
  {
    lastDrawBest = bestScore;

    tft.fillRect(60, 4, 32, 10, COLOR_BG);
    tft.setCursor(60, 5);
    tft.print(bestScore);
  }

  if (force || measuredFps != lastDrawFps)
  {
    lastDrawFps = measuredFps;

    tft.fillRect(126, 4, 28, 10, COLOR_BG);
    tft.setCursor(126, 5);
    tft.print(measuredFps);
  }
}

// =====================================================
// Tuberias
// =====================================================

static int16_t randomGapY()
{
  int16_t minY = gameTop + PIPE_GAP / 2 + 8;
  int16_t maxY = groundY - PIPE_GAP / 2 - 8;

  if (maxY <= minY)
  {
    return (gameTop + groundY) / 2;
  }

  return (int16_t)random(minY, maxY);
}

static void setPipeX(Pipe &p, int32_t x100)
{
  p.x100 = x100;
  p.x = x100ToPx(x100);
}

static void resetPipe(uint8_t i, int16_t newX)
{
  setPipeX(pipes[i], (int32_t)newX * 100L);
  pipes[i].gapY = randomGapY();
  pipes[i].scored = false;
}

static int16_t farthestPipeX()
{
  int16_t mx = pipes[0].x;

  for (uint8_t i = 1; i < 3; i++)
  {
    if (pipes[i].x > mx)
    {
      mx = pipes[i].x;
    }
  }

  return mx;
}

static void drawPipeAt(const Pipe &p)
{
  if (p.x >= screenW || (p.x + PIPE_W) <= 0)
  {
    return;
  }

  int16_t gapTop = p.gapY - PIPE_GAP / 2;
  int16_t gapBottom = p.gapY + PIPE_GAP / 2;

  if (gapTop > gameTop)
  {
    int16_t h = gapTop - gameTop;
    fillRectClip(p.x, gameTop, PIPE_W, h, COLOR_PIPE);
    drawRectClip(p.x, gameTop, PIPE_W, h, COLOR_PIPE_B);
  }

  if (gapBottom < groundY)
  {
    int16_t h = groundY - gapBottom;
    fillRectClip(p.x, gapBottom, PIPE_W, h, COLOR_PIPE);
    drawRectClip(p.x, gapBottom, PIPE_W, h, COLOR_PIPE_B);
  }
}

static void drawPipeSliceAt(const Pipe &p, int16_t sliceX, int16_t sliceW)
{
  if (sliceW <= 0)
  {
    return;
  }

  int16_t pipeL = p.x;
  int16_t pipeR = p.x + PIPE_W;

  int16_t sx0 = sliceX;
  int16_t sx1 = sliceX + sliceW;

  if (sx1 <= pipeL || sx0 >= pipeR)
  {
    return;
  }

  if (sx0 < pipeL) sx0 = pipeL;
  if (sx1 > pipeR) sx1 = pipeR;

  int16_t w = sx1 - sx0;

  if (w <= 0)
  {
    return;
  }

  int16_t gapTop = p.gapY - PIPE_GAP / 2;
  int16_t gapBottom = p.gapY + PIPE_GAP / 2;

  if (gapTop > gameTop)
  {
    int16_t y = gameTop;
    int16_t h = gapTop - gameTop;

    fillRectClip(sx0, y, w, h, COLOR_PIPE);

    drawHLineClip(sx0, y, w, COLOR_PIPE_B);
    drawHLineClip(sx0, y + h - 1, w, COLOR_PIPE_B);

    if (sx0 <= pipeL && pipeL < sx1)
    {
      drawVLineClip(pipeL, y, h, COLOR_PIPE_B);
    }

    int16_t rightEdge = pipeR - 1;
    if (sx0 <= rightEdge && rightEdge < sx1)
    {
      drawVLineClip(rightEdge, y, h, COLOR_PIPE_B);
    }
  }

  if (gapBottom < groundY)
  {
    int16_t y = gapBottom;
    int16_t h = groundY - gapBottom;

    fillRectClip(sx0, y, w, h, COLOR_PIPE);

    drawHLineClip(sx0, y, w, COLOR_PIPE_B);
    drawHLineClip(sx0, y + h - 1, w, COLOR_PIPE_B);

    if (sx0 <= pipeL && pipeL < sx1)
    {
      drawVLineClip(pipeL, y, h, COLOR_PIPE_B);
    }

    int16_t rightEdge = pipeR - 1;
    if (sx0 <= rightEdge && rightEdge < sx1)
    {
      drawVLineClip(rightEdge, y, h, COLOR_PIPE_B);
    }
  }
}

static void updatePipeSliding(const Pipe &oldP, const Pipe &newP)
{
  int16_t dx = oldP.x - newP.x;

  if (dx == 0 && oldP.gapY == newP.gapY)
  {
    return;
  }

  bool normalSlide =
    (dx > 0) &&
    (dx <= 8) &&
    (oldP.gapY == newP.gapY);

  if (!normalSlide)
  {
    fillRectClip(oldP.x - 2, gameTop, PIPE_W + 4, groundY - gameTop, COLOR_SKY);
    drawPipeAt(newP);
    return;
  }

  fillRectClip(
    oldP.x + PIPE_W - dx,
    gameTop,
    dx,
    groundY - gameTop,
    COLOR_SKY
  );

  drawPipeSliceAt(newP, newP.x, dx + 1);
  drawPipeSliceAt(newP, newP.x + PIPE_W - 1, 1);
}

// =====================================================
// Bird
// =====================================================

static RectI birdBoundsAt(int16_t y)
{
  return makeRect(
    birdX - 11,
    y - 9,
    25,
    18
  );
}

static void drawBirdAt(int16_t y)
{
  auto &tft = JWPLC_Display.tft();

  int16_t x = birdX;

  int16_t wingOffsetY = 0;

  if (birdV100 < -900)
  {
    wingOffsetY = -2;
  }
  else if (birdV100 > 1200)
  {
    wingOffsetY = 2;
  }

  // Cola pequena
  tft.fillTriangle(
    x - BIRD_DRAW_R + 1, y,
    x - BIRD_DRAW_R - 4, y - 3,
    x - BIRD_DRAW_R - 4, y + 3,
    COLOR_BEAK
  );

  tft.drawTriangle(
    x - BIRD_DRAW_R + 1, y,
    x - BIRD_DRAW_R - 4, y - 3,
    x - BIRD_DRAW_R - 4, y + 3,
    COLOR_BIRD_B
  );

  // Cuerpo circular
  tft.fillCircle(x, y, BIRD_DRAW_R, COLOR_BIRD);
  tft.drawCircle(x, y, BIRD_DRAW_R, COLOR_BIRD_B);

  // Ala centrada dentro del cuerpo
  tft.fillTriangle(
    x - 2, y,
    x - 6, y + wingOffsetY,
    x - 3, y + 5,
    COLOR_WING
  );

  tft.drawTriangle(
    x - 2, y,
    x - 6, y + wingOffsetY,
    x - 3, y + 5,
    COLOR_BIRD_B
  );

  // Pico pequeno
  tft.fillTriangle(
    x + BIRD_DRAW_R - 1, y - 2,
    x + BIRD_DRAW_R + 6, y,
    x + BIRD_DRAW_R - 1, y + 3,
    COLOR_BEAK
  );

  tft.drawTriangle(
    x + BIRD_DRAW_R - 1, y - 2,
    x + BIRD_DRAW_R + 6, y,
    x + BIRD_DRAW_R - 1, y + 3,
    COLOR_BIRD_B
  );

  // Ojo
  tft.fillCircle(x + 3, y - 3, 2, ST77XX_WHITE);
  tft.drawPixel(x + 4, y - 3, ST77XX_BLACK);
}

static void restoreGameBackgroundRect(const RectI &dirty)
{
  if (dirty.w <= 0 || dirty.h <= 0)
  {
    return;
  }

  int16_t x = dirty.x;
  int16_t y = dirty.y;
  int16_t w = dirty.w;
  int16_t h = dirty.h;

  int16_t skyY0 = y;
  int16_t skyY1 = y + h;

  if (skyY0 < gameTop)
  {
    skyY0 = gameTop;
  }

  if (skyY1 > groundY)
  {
    skyY1 = groundY;
  }

  if (skyY1 > skyY0)
  {
    fillRectClip(x, skyY0, w, skyY1 - skyY0, COLOR_SKY);
  }

  if ((y + h) > groundY)
  {
    int16_t gy0 = groundY;
    int16_t gy1 = y + h;

    if (gy1 > screenH)
    {
      gy1 = screenH;
    }

    if (gy1 > gy0)
    {
      fillRectClip(x, gy0, w, gy1 - gy0, COLOR_GROUND);
      drawHLineClip(x, groundY, w, COLOR_PIPE_B);
    }
  }

  if (y < gameTop)
  {
    fillRectClip(x, 0, w, gameTop, COLOR_BG);
    drawHeaderLabels();
    drawHeaderValues(true);
  }

  RectI dirtySky = makeRect(x, skyY0, w, skyY1 - skyY0);

  for (uint8_t i = 0; i < 3; i++)
  {
    RectI pipeRect = makeRect(
      pipes[i].x,
      gameTop,
      PIPE_W,
      groundY - gameTop
    );

    if (rectIntersects(dirtySky, pipeRect))
    {
      drawPipeSliceAt(pipes[i], x, w);
    }
  }
}

static void restoreBirdTrail(int16_t oldY, int16_t newY)
{
  RectI oldR = birdBoundsAt(oldY);
  RectI newR = birdBoundsAt(newY);

  RectI dirty = unionRect(oldR, newR);

  dirty.x -= 1;
  dirty.y -= 1;
  dirty.w += 2;
  dirty.h += 2;

  restoreGameBackgroundRect(dirty);
}

// =====================================================
// Pantallas
// =====================================================

static void drawCenteredText(const char *text, int16_t y, uint8_t textSize, uint16_t color)
{
  auto &tft = JWPLC_Display.tft();

  int16_t x1, y1;
  uint16_t w, h;

  tft.setTextSize(textSize);
  tft.setTextColor(color, COLOR_BG);
  tft.getTextBounds(text, 0, y, &x1, &y1, &w, &h);

  int16_t x = (screenW - (int16_t)w) / 2;

  if (x < 0)
  {
    x = 0;
  }

  tft.setCursor(x, y);
  tft.print(text);
}

static void drawStaticGameLayout()
{
  auto &tft = JWPLC_Display.tft();

  tft.fillScreen(COLOR_BG);

  tft.fillRect(0, gameTop, screenW, groundY - gameTop, COLOR_SKY);

  tft.fillRect(0, groundY, screenW, screenH - groundY, COLOR_GROUND);
  tft.drawFastHLine(0, groundY, screenW, COLOR_PIPE_B);

  drawHeaderLabels();
  resetHeaderCache();
  drawHeaderValues(true);
}

static void drawCurrentGameFullFrame()
{
  drawStaticGameLayout();

  for (uint8_t i = 0; i < 3; i++)
  {
    drawPipeAt(pipes[i]);
  }

  drawBirdAt(birdY());
}

static void drawGameFramePartial(const Pipe oldPipes[3], int16_t oldBirdY)
{
  for (uint8_t i = 0; i < 3; i++)
  {
    updatePipeSliding(oldPipes[i], pipes[i]);
  }

  int16_t newBirdY = birdY();
  restoreBirdTrail(oldBirdY, newBirdY);
  drawBirdAt(newBirdY);

  updateMeasuredFps();
  drawHeaderValues(false);
}

static void drawStartHint()
{
  auto &tft = JWPLC_Display.tft();

  cacheScreenGeometry();

  tft.fillScreen(COLOR_BG);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BG);
  tft.setCursor(10, 12);
  tft.print("JWPLC FLAPPY");

  tft.drawFastHLine(0, 38, screenW, ST77XX_BLUE);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(18, 58);
  tft.print("OK: iniciar / saltar");

  tft.setCursor(18, 74);
  tft.print("UP: saltar");

  tft.setCursor(18, 90);
  tft.print("ESC: volver a IDLE");

  tft.setCursor(18, 116);
  tft.print("FPS objetivo: ");
  tft.print(GAME_TARGET_FPS);

  tft.setCursor(18, 132);
  tft.print("Velocidad: ");
  tft.print(GAME_SPEED_PERCENT);
  tft.print("%");
}

static void drawGameOver()
{
  auto &tft = JWPLC_Display.tft();

  drawCurrentGameFullFrame();

  int16_t boxW = 170;
  int16_t boxH = 70;
  int16_t boxX = (screenW - boxW) / 2;
  int16_t boxY = (screenH - boxH) / 2;

  tft.fillRect(boxX, boxY, boxW, boxH, COLOR_BG);
  tft.drawRect(boxX, boxY, boxW, boxH, COLOR_PIPE_B);

  drawCenteredText("GAME OVER", boxY + 10, 2, ST77XX_RED);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(boxX + 18, boxY + 40);
  tft.print("Score: ");
  tft.print(score);

  tft.setCursor(boxX + 18, boxY + 54);
  tft.print("Volviendo a IDLE...");
}

// =====================================================
// Logica del juego
// =====================================================

static void startGame()
{
  cacheScreenGeometry();

  randomSeed((uint32_t)micros());

  score = 0;

  birdY100 = ((int32_t)((gameTop + groundY) / 2)) * 100L;
  birdV100 = 0;

  resetPipe(0, screenW + 25);
  resetPipe(1, screenW + 25 + PIPE_SPACING);
  resetPipe(2, screenW + 25 + PIPE_SPACING * 2);

  renderedFrames = 0;
  measuredFps = 0;
  fpsWindowStartMs = millis();

  gameState = GAME_RUNNING;
  lastFrameMs = millis();
  lastSimMs = millis();
  gameOverDrawn = false;

  resetHeaderCache();

  JWPLC_Display.notifyActivity();
  JWPLC_Display.clearPendingInput();

  drawCurrentGameFullFrame();

  debugPrintln("Flappy: START");
}

static void setGameOver()
{
  gameState = GAME_OVER;
  gameOverMs = millis();
  gameOverDrawn = false;

  if (score > bestScore)
  {
    bestScore = score;
  }

  debugPrintln("Flappy: GAME OVER");
}

static bool checkCollision()
{
  int16_t by = birdY();

  int16_t r = BIRD_COLLISION_R;

  if ((by - r) <= gameTop)
  {
    return true;
  }

  if ((by + r) >= groundY)
  {
    return true;
  }

  int16_t birdLeft = birdX - r;
  int16_t birdRight = birdX + r;
  int16_t birdTop = by - r;
  int16_t birdBottom = by + r;

  for (uint8_t i = 0; i < 3; i++)
  {
    int16_t pipeLeft = pipes[i].x;
    int16_t pipeRight = pipes[i].x + PIPE_W;

    bool overlapX =
      (birdRight >= pipeLeft) &&
      (birdLeft <= pipeRight);

    if (!overlapX)
    {
      continue;
    }

    int16_t gapTop = pipes[i].gapY - PIPE_GAP / 2;
    int16_t gapBottom = pipes[i].gapY + PIPE_GAP / 2;

    static const int16_t COLLISION_MARGIN = 2;

    bool safelyInsideGap =
      ((birdTop + COLLISION_MARGIN) > gapTop) &&
      ((birdBottom - COLLISION_MARGIN) < gapBottom);

    if (!safelyInsideGap)
    {
      return true;
    }
  }

  return false;
}

static void updateGame(uint32_t rawDtMs)
{
  uint32_t dtMs = scaleDtMs(rawDtMs);

  if (dtMs == 0)
  {
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK) || JWPLC_Buttons.pressed(BTN_UP))
  {
    birdV100 = (int32_t)FLAP_VEL_PX_S * 100L;
    JWPLC_Display.notifyActivity();
  }

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    gameState = GAME_WAIT_OK;
    JWPLC_Display.goIdle();
    return;
  }

  birdV100 += ((int32_t)GRAVITY_PX_S2 * 100L * (int32_t)dtMs) / 1000L;

  int32_t maxFall100 = (int32_t)MAX_FALL_PX_S * 100L;

  if (birdV100 > maxFall100)
  {
    birdV100 = maxFall100;
  }

  birdY100 += (birdV100 * (int32_t)dtMs) / 1000L;

  int32_t pipeDx100 =
    ((int32_t)PIPE_SPEED_PX_S * 100L * (int32_t)dtMs) / 1000L;

  if (pipeDx100 <= 0)
  {
    pipeDx100 = 1;
  }

  for (uint8_t i = 0; i < 3; i++)
  {
    setPipeX(pipes[i], pipes[i].x100 - pipeDx100);

    if (!pipes[i].scored && (pipes[i].x + PIPE_W < birdX))
    {
      pipes[i].scored = true;
      score++;
    }

    if (pipes[i].x + PIPE_W < 0)
    {
      resetPipe(i, farthestPipeX() + PIPE_SPACING);
    }
  }

  if (checkCollision())
  {
    setGameOver();
  }
}

// =====================================================
// Callbacks USER del JWPLC_Display
// =====================================================

extern "C" bool jwplcCanReturnToIdle(void)
{
  return (gameState != GAME_RUNNING);
}

extern "C" void jwplcUserDisplayEnterCallback()
{
  userEnterMs = millis();

  cacheScreenGeometry();

  if (JWPLC_Buttons.isDown(BTN_OK))
  {
    startGame();
  }
  else
  {
    gameState = GAME_WAIT_OK;
    drawStartHint();
  }
}

extern "C" void jwplcUserDisplayRefreshCallback(const JWPLC_IOState *io, const JWPLC_RTCState *rtc)
{
  (void)io;
  (void)rtc;

  uint32_t now = millis();

  if (gameState == GAME_WAIT_OK)
  {
    if (JWPLC_Buttons.pressed(BTN_OK) || JWPLC_Buttons.isDown(BTN_OK))
    {
      startGame();
      return;
    }

    if (JWPLC_Buttons.pressed(BTN_ESC))
    {
      JWPLC_Display.goIdle();
      return;
    }

    if ((uint32_t)(now - userEnterMs) >= WAIT_OK_HINT_MS)
    {
      JWPLC_Display.goIdle();
    }

    return;
  }

  if (gameState == GAME_RUNNING)
  {
    if ((uint32_t)(now - lastFrameMs) < FRAME_PERIOD_MS)
    {
      return;
    }

    Pipe oldPipes[3];

    for (uint8_t i = 0; i < 3; i++)
    {
      oldPipes[i] = pipes[i];
    }

    int16_t oldBirdY = birdY();

    uint32_t dtMs = now - lastSimMs;
    lastSimMs = now;
    lastFrameMs = now;

    updateGame(dtMs);

    if (gameState == GAME_RUNNING)
    {
      drawGameFramePartial(oldPipes, oldBirdY);
    }

    return;
  }

  if (gameState == GAME_OVER)
  {
    if (!gameOverDrawn)
    {
      drawGameOver();
      gameOverDrawn = true;
    }

    if ((uint32_t)(now - gameOverMs) >= GAME_OVER_HOLD_MS)
    {
      gameState = GAME_WAIT_OK;
      JWPLC_Display.goIdle();
    }

    return;
  }
}

extern "C" void jwplcUserDisplayExitCallback()
{
  gameState = GAME_WAIT_OK;

  measuredFps = 0;
  renderedFrames = 0;
  fpsWindowStartMs = 0;

  debugPrintln("Flappy: retorno a IDLE");
}

// =====================================================
// Arduino setup / loop
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(800);

  /*
    En JWPLC_Display 2.1.0 estas configuraciones ya pueden hacerse
    desde setup(), incluso antes de que la TFT este lista.
  */
  JWPLC_Display.setIdleWakeMode(IDLE_WAKE_BUTTON_ONLY);
  JWPLC_Display.setIdleWakeButton(BTN_OK);
  JWPLC_Display.setIdleReturnMode(IDLE_RETURN_DISABLED);
  JWPLC_Display.setUserRefreshPeriodMs(FRAME_PERIOD_MS);
  JWPLC_Display.setIdleRefreshPeriodMs(250);
  JWPLC_Display.clearPendingInput();  

  if (ENABLE_SERIAL_DEBUG)
  {
    Serial.println();
    Serial.println("JWPLC TFT Flappy Bird - Optimized");
    Serial.print("FPS objetivo: ");
    Serial.println(GAME_TARGET_FPS);
    Serial.print("Velocidad juego: ");
    Serial.print(GAME_SPEED_PERCENT);
    Serial.println("%");
  }
}

void loop()
{
  static bool displayReadyLogged = false;
  static uint32_t lastLogMs = 0;

  if (!displayReadyLogged && JWPLC_Display.isReady())
  {
    displayReadyLogged = true;

    if (ENABLE_SERIAL_DEBUG)
    {
      Serial.println("Display listo.");
      Serial.print("UserRefreshPeriodMs: ");
      Serial.println(JWPLC_Display.userRefreshPeriodMs());
      Serial.println("Presiona OK en IDLE para iniciar Flappy.");
    }
  }

  if (ENABLE_SERIAL_DEBUG && displayReadyLogged)
  {
    uint32_t now = millis();

    if ((uint32_t)(now - lastLogMs) >= 2000)
    {
      lastLogMs = now;

      Serial.print("Mode: ");
      Serial.print(JWPLC_Display.isIdleMode() ? "IDLE" : "USER");

      Serial.print(" | Game: ");
      Serial.print((uint8_t)gameState);

      Serial.print(" | Target FPS: ");
      Serial.print(GAME_TARGET_FPS);

      Serial.print(" | Measured FPS: ");
      Serial.print(measuredFps);

      Serial.print(" | Speed: ");
      Serial.print(GAME_SPEED_PERCENT);
      Serial.print("%");

      Serial.print(" | Best: ");
      Serial.println(bestScore);
    }
  }

  delay(2);
}

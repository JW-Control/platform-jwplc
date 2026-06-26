/*
  JWPLC_Tetris_Optimized_Buzzer.ino

  Tetris optimizado para JWPLC Basic 2.1.0.

  Requiere JWPLC_Display con:
  - setUserRefreshPeriodMs() respetado por el core.
  - setIdleWakeMode()/setIdleReturnMode() configurables desde setup().

  Controles:
  - LEFT  : mover izquierda
  - RIGHT : mover derecha
  - DOWN  : caida suave
  - OK    : rotar
  - UP    : caida instantanea
  - ESC   : volver a IDLE

  Buzzer:
  - IO26
  - Musica y SFX no bloqueantes con tone()/noTone().

  Optimizacion:
  - No redibuja toda la pantalla en cada frame.
  - Solo borra/dibuja la pieza activa cuando cambia.
  - El tablero completo solo se redibuja al fijar pieza o limpiar lineas.
  - Ya no usa workaround con JWPLC_Display.forceRedraw().
*/

#include <JWPLC_Display.h>

// =====================================================
// Configuracion general
// =====================================================

// 30 FPS recomendado. Puedes probar 40 o 50.
// Con el clamp actual del core, 50 FPS es el techo practico.
static const uint8_t GAME_TARGET_FPS = 30;
static const uint32_t FRAME_PERIOD_MS = 1000UL / GAME_TARGET_FPS;

// Velocidad global del Tetris.
// 100 = normal.
// 80  = mas lento.
// 130 = mas rapido.
static const uint16_t TETRIS_SPEED_PERCENT = 100;

// Caida automatica base.
// Con TETRIS_SPEED_PERCENT = 100, la pieza baja 1 celda cada 650 ms.
static const uint32_t BASE_DROP_INTERVAL_MS = 650;

// Caida suave con DOWN mantenido.
static const uint32_t SOFT_DROP_INTERVAL_MS = 55;

// Repeticion izquierda/derecha al mantener botones.
static const uint32_t LR_REPEAT_MS = 115;

static const uint32_t GAME_OVER_HOLD_MS = 1500;
static const uint32_t WAIT_OK_HINT_MS = 1000;

static const bool ENABLE_SERIAL_DEBUG = true;

// =====================================================
// Buzzer JWPLC Basic
// =====================================================

static const uint8_t BUZZER_PIN = 26;
static const bool ENABLE_BUZZER = true;
static const bool ENABLE_BACKGROUND_MUSIC = true;
static const bool ENABLE_SFX = false;

// 100 = normal. 80 = mas lento. 120 = mas rapido.
static const uint16_t MUSIC_SPEED_PERCENT = 100;

static const uint8_t BUZZER_PWM_RES_BITS = 8;

// 0   = apagado
// 16  = muy bajo
// 32  = bajo
// 48  = medio-bajo
// 64  = medio
// 96  = medio-alto
// 128 = fuerte
// 255 = máximo
static uint8_t buzzerVolume = 16;


// =====================================================
// Layout
// =====================================================

static const uint8_t BOARD_COLS = 10;
static const uint8_t BOARD_ROWS = 18;
static const int16_t CELL = 8;

static int16_t screenW = 0;
static int16_t screenH = 0;

static int16_t boardX = 12;
static int16_t boardY = 20;

static int16_t boardW = BOARD_COLS * CELL;
static int16_t boardH = BOARD_ROWS * CELL;

static int16_t sideX = 110;

// =====================================================
// Colores
// =====================================================

static const uint16_t COLOR_BG = ST77XX_BLACK;
static const uint16_t COLOR_BOARD_BG = ST77XX_BLACK;
static const uint16_t COLOR_TEXT = ST77XX_WHITE;
static const uint16_t COLOR_TITLE = ST77XX_CYAN;
static const uint16_t COLOR_BORDER = ST77XX_WHITE;

// =====================================================
// Estado del juego
// =====================================================

enum GameState : uint8_t
{
  GAME_WAIT_OK = 0,
  GAME_RUNNING,
  GAME_OVER
};

struct Piece
{
  uint8_t type;
  uint8_t rot;
  int8_t x;
  int8_t y;
};

static GameState gameState = GAME_WAIT_OK;

static uint8_t board[BOARD_ROWS][BOARD_COLS];

static Piece active;
static uint8_t nextType = 0;

static uint32_t score = 0;
static uint16_t linesClearedTotal = 0;
static uint32_t bestScore = 0;

static uint32_t userEnterMs = 0;
static uint32_t gameOverMs = 0;

static uint32_t lastFrameMs = 0;
static uint32_t lastDropMs = 0;

static uint32_t lastLeftMs = 0;
static uint32_t lastRightMs = 0;
static uint32_t lastDownMs = 0;

static bool gameOverDrawn = false;

// Metricas
static uint32_t callbackFrames = 0;
static uint32_t fpsWindowStartMs = 0;
static uint16_t measuredFps = 0;

// Cache de info dibujada
static uint32_t lastDrawScore = 0xFFFFFFFF;
static uint16_t lastDrawLines = 0xFFFF;
static uint32_t lastDrawBest = 0xFFFFFFFF;
static uint16_t lastDrawFps = 0xFFFF;

// =====================================================
// Tetrominos
// Tipos: 0 I, 1 O, 2 T, 3 S, 4 Z, 5 J, 6 L
// =====================================================

static const int8_t SHAPES[7][4][4][2] = {
  // I
  {
    {{0,1},{1,1},{2,1},{3,1}},
    {{2,0},{2,1},{2,2},{2,3}},
    {{0,2},{1,2},{2,2},{3,2}},
    {{1,0},{1,1},{1,2},{1,3}}
  },

  // O
  {
    {{1,0},{2,0},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{2,1}}
  },

  // T
  {
    {{1,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{2,1},{1,2}},
    {{0,1},{1,1},{2,1},{1,2}},
    {{1,0},{0,1},{1,1},{1,2}}
  },

  // S
  {
    {{1,0},{2,0},{0,1},{1,1}},
    {{1,0},{1,1},{2,1},{2,2}},
    {{1,1},{2,1},{0,2},{1,2}},
    {{0,0},{0,1},{1,1},{1,2}}
  },

  // Z
  {
    {{0,0},{1,0},{1,1},{2,1}},
    {{2,0},{1,1},{2,1},{1,2}},
    {{0,1},{1,1},{1,2},{2,2}},
    {{1,0},{0,1},{1,1},{0,2}}
  },

  // J
  {
    {{0,0},{0,1},{1,1},{2,1}},
    {{1,0},{2,0},{1,1},{1,2}},
    {{0,1},{1,1},{2,1},{2,2}},
    {{1,0},{1,1},{0,2},{1,2}}
  },

  // L
  {
    {{2,0},{0,1},{1,1},{2,1}},
    {{1,0},{1,1},{1,2},{2,2}},
    {{0,1},{1,1},{2,1},{0,2}},
    {{0,0},{1,0},{1,1},{1,2}}
  }
};

// =====================================================
// Audio no bloqueante
// =====================================================

#define NOTE_REST 0
#define NOTE_E4   330
#define NOTE_F4   349
#define NOTE_G4   392
#define NOTE_A4   440
#define NOTE_B4   494
#define NOTE_C5   523
#define NOTE_D5   587
#define NOTE_E5   659
#define NOTE_F5   698
#define NOTE_G5   784
#define NOTE_GS5  831
#define NOTE_A5   880
#define NOTE_B5   988

struct MusicNote
{
  uint16_t freq;
  uint16_t durMs;
};

static const MusicNote TETRIS_THEME[] = {
  {NOTE_E5, 417}, {NOTE_B4, 208}, {NOTE_C5, 208}, {NOTE_D5, 417},
  {NOTE_C5, 208}, {NOTE_B4, 208}, {NOTE_A4, 417}, {NOTE_A4, 208},
  {NOTE_C5, 208}, {NOTE_E5, 417}, {NOTE_D5, 208}, {NOTE_C5, 208},
  {NOTE_B4, 625}, {NOTE_C5, 208}, {NOTE_D5, 417}, {NOTE_E5, 417},
  {NOTE_C5, 417}, {NOTE_A4, 417}, {NOTE_A4, 417}, {NOTE_REST, 417},

  {NOTE_REST, 208}, {NOTE_D5, 417}, {NOTE_F5, 208}, {NOTE_A5, 417},
  {NOTE_G5, 208}, {NOTE_F5, 208}, {NOTE_E5, 625}, {NOTE_C5, 208},
  {NOTE_E5, 417}, {NOTE_D5, 208}, {NOTE_C5, 208}, {NOTE_B4, 417},
  {NOTE_B4, 208}, {NOTE_C5, 208}, {NOTE_D5, 417}, {NOTE_E5, 417},
  {NOTE_C5, 417}, {NOTE_A4, 417}, {NOTE_A4, 417}, {NOTE_REST, 417},

  {NOTE_E5, 833}, {NOTE_C5, 833},
  {NOTE_D5, 833}, {NOTE_B4, 833},
  {NOTE_C5, 833}, {NOTE_A4, 833},
  {NOTE_B4, 1667},

  {NOTE_E5, 833}, {NOTE_C5, 833},
  {NOTE_D5, 833}, {NOTE_B4, 833},
  {NOTE_C5, 417}, {NOTE_E5, 417}, {NOTE_A5, 833},
  {NOTE_GS5, 1667},

  {NOTE_E5, 417}, {NOTE_B4, 208}, {NOTE_C5, 208}, {NOTE_D5, 417},
  {NOTE_C5, 208}, {NOTE_B4, 208}, {NOTE_A4, 417}, {NOTE_A4, 208},
  {NOTE_C5, 208}, {NOTE_E5, 417}, {NOTE_D5, 208}, {NOTE_C5, 208},
  {NOTE_B4, 625}, {NOTE_C5, 208}, {NOTE_D5, 417}, {NOTE_E5, 417},
  {NOTE_C5, 417}, {NOTE_A4, 417}, {NOTE_A4, 417}, {NOTE_REST, 417},

  {NOTE_REST, 208}, {NOTE_D5, 417}, {NOTE_F5, 208}, {NOTE_A5, 417},
  {NOTE_G5, 208}, {NOTE_F5, 208},

  {NOTE_REST, 208}, {NOTE_E5, 417}, {NOTE_C5, 208}, {NOTE_E5, 417},
  {NOTE_D5, 208}, {NOTE_C5, 208},

  {NOTE_REST, 208}, {NOTE_B4, 417}, {NOTE_C5, 208}, {NOTE_D5, 417},
  {NOTE_E5, 417},

  {NOTE_REST, 208}, {NOTE_C5, 417}, {NOTE_A4, 208},
  {NOTE_A4, 417}, {NOTE_REST, 417}
};

static const uint8_t TETRIS_THEME_LEN =
  sizeof(TETRIS_THEME) / sizeof(TETRIS_THEME[0]);

enum AudioMode : uint8_t
{
  AUDIO_MODE_MUSIC = 0,
  AUDIO_MODE_SFX
};

static bool buzzerInitialized = false;
static bool musicEnabledRuntime = false;

static AudioMode audioMode = AUDIO_MODE_MUSIC;

static uint8_t musicIndex = 0;
static uint32_t nextMusicEventMs = 0;
static uint32_t noteOffMs = 0;
static bool noteIsOn = false;

static uint32_t sfxStartMs = 0;
static uint32_t sfxEndMs = 0;

static uint16_t scaleMusicDuration(uint16_t durMs)
{
  uint32_t scaled = ((uint32_t)durMs * 100UL) / MUSIC_SPEED_PERCENT;

  if (scaled < 20)
  {
    scaled = 20;
  }

  return (uint16_t)scaled;
}

static void buzzerBegin()
{
  if (!ENABLE_BUZZER)
  {
    return;
  }

  if (buzzerInitialized)
  {
    return;
  }

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  ledcAttach(BUZZER_PIN, 2000, BUZZER_PWM_RES_BITS);
  ledcWrite(BUZZER_PIN, 0);

  buzzerInitialized = true;
}

static void buzzerTone(uint16_t freq)
{
  if (!ENABLE_BUZZER || !buzzerInitialized)
  {
    return;
  }

  if (freq == NOTE_REST)
  {
    ledcWrite(BUZZER_PIN, 0);
    noteIsOn = false;
    return;
  }

  ledcWriteTone(BUZZER_PIN, freq);
  ledcWrite(BUZZER_PIN, buzzerVolume);

  noteIsOn = true;
}

static void buzzerOff()
{
  if (!ENABLE_BUZZER || !buzzerInitialized)
  {
    return;
  }

  ledcWrite(BUZZER_PIN, 0);
  noteIsOn = false;
}

static void musicReset()
{
  musicIndex = 0;
  nextMusicEventMs = 0;
  noteOffMs = 0;
  noteIsOn = false;
  audioMode = AUDIO_MODE_MUSIC;
}

static void musicStart()
{
  if (!ENABLE_BUZZER)
  {
    return;
  }

  buzzerBegin();
  musicEnabledRuntime = true;
  musicReset();
}

static void musicStop()
{
  musicEnabledRuntime = false;
  buzzerOff();
  musicReset();
}

static void playSfx(uint16_t freq, uint16_t durMs)
{
  if (!ENABLE_BUZZER || !ENABLE_SFX)
  {
    return;
  }

  buzzerBegin();

  uint32_t now = millis();

  audioMode = AUDIO_MODE_SFX;
  sfxStartMs = now;
  sfxEndMs = now + durMs;

  buzzerTone(freq);
}

static void serviceSfx(uint32_t now)
{
  if (audioMode != AUDIO_MODE_SFX)
  {
    return;
  }

  if ((int32_t)(now - sfxEndMs) >= 0)
  {
    uint32_t pausedMs = now - sfxStartMs;

    buzzerOff();

    if (nextMusicEventMs != 0)
    {
      nextMusicEventMs += pausedMs;
    }

    if (noteOffMs != 0)
    {
      noteOffMs += pausedMs;
    }

    audioMode = AUDIO_MODE_MUSIC;
  }
}

static void serviceBackgroundMusic(uint32_t now)
{
  if (!ENABLE_BUZZER || !ENABLE_BACKGROUND_MUSIC || !musicEnabledRuntime)
  {
    return;
  }

  if (audioMode != AUDIO_MODE_MUSIC)
  {
    return;
  }

  if (noteIsOn && (int32_t)(now - noteOffMs) >= 0)
  {
    buzzerOff();
  }

  if ((int32_t)(now - nextMusicEventMs) < 0)
  {
    return;
  }

  const MusicNote &n = TETRIS_THEME[musicIndex];

  uint16_t dur = scaleMusicDuration(n.durMs);
  uint16_t gate = (dur * 90) / 100;

  if (n.freq == NOTE_REST)
  {
    buzzerOff();
  }
  else
  {
    buzzerTone(n.freq);
  }

  noteOffMs = now + gate;
  nextMusicEventMs = now + dur;

  musicIndex++;
  if (musicIndex >= TETRIS_THEME_LEN)
  {
    musicIndex = 0;
  }
}

static void serviceBuzzerMusic()
{
  if (!ENABLE_BUZZER)
  {
    return;
  }

  uint32_t now = millis();

  serviceSfx(now);
  serviceBackgroundMusic(now);
}

static void sfxMove()     { playSfx(880, 18); }
static void sfxRotate()   { playSfx(1175, 28); }
static void sfxDrop()     { playSfx(988, 35); }
static void sfxLock()     { playSfx(523, 45); }
static void sfxLine()     { playSfx(1568, 90); }
static void sfxGameOver() { playSfx(196, 350); }

// =====================================================
// Utilidades
// =====================================================

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

  boardW = BOARD_COLS * CELL;
  boardH = BOARD_ROWS * CELL;

  boardX = 12;
  boardY = 20;
  sideX = boardX + boardW + 18;

  if (screenW < 160 || screenH < 120)
  {
    screenW = 320;
    screenH = 170;
    boardX = 12;
    boardY = 20;
    sideX = boardX + boardW + 18;
  }
}

static uint32_t currentDropIntervalMs()
{
  uint32_t interval = (BASE_DROP_INTERVAL_MS * 100UL) / TETRIS_SPEED_PERCENT;

  if (interval < 80)
  {
    interval = 80;
  }

  return interval;
}

static uint16_t colorForCell(uint8_t v)
{
  switch (v)
  {
    case 1: return ST77XX_CYAN;
    case 2: return ST77XX_YELLOW;
    case 3: return ST77XX_MAGENTA;
    case 4: return ST77XX_GREEN;
    case 5: return ST77XX_RED;
    case 6: return ST77XX_BLUE;
    case 7: return ST77XX_WHITE;
    default: return COLOR_BOARD_BG;
  }
}

static void updateMeasuredFps()
{
  callbackFrames++;

  uint32_t now = millis();

  if (fpsWindowStartMs == 0)
  {
    fpsWindowStartMs = now;
    callbackFrames = 0;
    measuredFps = 0;
    return;
  }

  uint32_t elapsed = now - fpsWindowStartMs;

  if (elapsed >= 1000)
  {
    measuredFps = (uint16_t)((callbackFrames * 1000UL) / elapsed);
    callbackFrames = 0;
    fpsWindowStartMs = now;
  }
}

// =====================================================
// Dibujo de tablero y celdas
// =====================================================

static void drawCell(uint8_t row, uint8_t col, uint8_t value)
{
  if (row >= BOARD_ROWS || col >= BOARD_COLS)
  {
    return;
  }

  auto &tft = JWPLC_Display.tft();

  int16_t x = boardX + col * CELL;
  int16_t y = boardY + row * CELL;

  if (value == 0)
  {
    tft.fillRect(x, y, CELL, CELL, COLOR_BOARD_BG);
    return;
  }

  uint16_t c = colorForCell(value);

  tft.fillRect(x, y, CELL, CELL, COLOR_BOARD_BG);
  tft.fillRect(x + 1, y + 1, CELL - 2, CELL - 2, c);

  tft.drawFastHLine(x + 1, y + 1, CELL - 2, ST77XX_WHITE);
  tft.drawFastVLine(x + 1, y + 1, CELL - 2, ST77XX_WHITE);
}

static void drawBoardFrame()
{
  auto &tft = JWPLC_Display.tft();

  tft.drawRect(boardX - 1, boardY - 1, boardW + 2, boardH + 2, COLOR_BORDER);
}

static void drawBoardCells()
{
  for (uint8_t r = 0; r < BOARD_ROWS; r++)
  {
    for (uint8_t c = 0; c < BOARD_COLS; c++)
    {
      drawCell(r, c, board[r][c]);
    }
  }
}

static void drawPiece(const Piece &p, bool erase)
{
  uint8_t value = erase ? 0 : (p.type + 1);

  for (uint8_t i = 0; i < 4; i++)
  {
    int8_t bx = SHAPES[p.type][p.rot][i][0];
    int8_t by = SHAPES[p.type][p.rot][i][1];

    int8_t col = p.x + bx;
    int8_t row = p.y + by;

    if (row < 0)
    {
      continue;
    }

    if (col < 0 || col >= BOARD_COLS || row >= BOARD_ROWS)
    {
      continue;
    }

    if (erase)
    {
      drawCell(row, col, board[row][col]);
    }
    else
    {
      drawCell(row, col, value);
    }
  }
}

// =====================================================
// Panel lateral
// =====================================================

static void resetInfoCache()
{
  lastDrawScore = 0xFFFFFFFF;
  lastDrawLines = 0xFFFF;
  lastDrawBest = 0xFFFFFFFF;
  lastDrawFps = 0xFFFF;
}

static void drawInfoStatic()
{
  auto &tft = JWPLC_Display.tft();

  tft.fillRect(sideX, 0, screenW - sideX, screenH, COLOR_BG);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BG);
  tft.setCursor(sideX, 10);
  tft.print("TETRIS");

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(sideX, 38);
  tft.print("NEXT");

  tft.setCursor(sideX, 85);
  tft.print("SCORE:");

  tft.setCursor(sideX, 103);
  tft.print("LINES:");

  tft.setCursor(sideX, 121);
  tft.print("BEST:");

  tft.setCursor(sideX, 139);
  tft.print("FPS:");

  tft.setCursor(sideX, 154);
  tft.print("SPD:");
  tft.print(TETRIS_SPEED_PERCENT);
  tft.print("%");
}

static void drawInfoValues(bool force)
{
  auto &tft = JWPLC_Display.tft();

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  if (force || score != lastDrawScore)
  {
    lastDrawScore = score;

    tft.fillRect(sideX + 48, 85, 75, 10, COLOR_BG);
    tft.setCursor(sideX + 48, 85);
    tft.print(score);
  }

  if (force || linesClearedTotal != lastDrawLines)
  {
    lastDrawLines = linesClearedTotal;

    tft.fillRect(sideX + 48, 103, 55, 10, COLOR_BG);
    tft.setCursor(sideX + 48, 103);
    tft.print(linesClearedTotal);
  }

  if (force || bestScore != lastDrawBest)
  {
    lastDrawBest = bestScore;

    tft.fillRect(sideX + 48, 121, 75, 10, COLOR_BG);
    tft.setCursor(sideX + 48, 121);
    tft.print(bestScore);
  }

  if (force || measuredFps != lastDrawFps)
  {
    lastDrawFps = measuredFps;

    tft.fillRect(sideX + 48, 139, 45, 10, COLOR_BG);
    tft.setCursor(sideX + 48, 139);
    tft.print(measuredFps);
  }
}

static void drawMiniCell(int16_t x, int16_t y, uint8_t value)
{
  auto &tft = JWPLC_Display.tft();

  static const int16_t MINI = 7;

  if (value == 0)
  {
    tft.fillRect(x, y, MINI, MINI, COLOR_BG);
    return;
  }

  tft.fillRect(x, y, MINI, MINI, COLOR_BG);
  tft.fillRect(x + 1, y + 1, MINI - 2, MINI - 2, colorForCell(value));
}

static void drawNextPiece()
{
  auto &tft = JWPLC_Display.tft();

  int16_t nx = sideX;
  int16_t ny = 50;

  tft.fillRect(nx, ny, 48, 32, COLOR_BG);

  static const int16_t MINI = 7;

  for (uint8_t i = 0; i < 4; i++)
  {
    int8_t bx = SHAPES[nextType][0][i][0];
    int8_t by = SHAPES[nextType][0][i][1];

    drawMiniCell(
      nx + bx * MINI,
      ny + by * MINI,
      nextType + 1
    );
  }
}

// =====================================================
// Logica de piezas
// =====================================================

static bool canPlace(const Piece &p)
{
  for (uint8_t i = 0; i < 4; i++)
  {
    int8_t bx = SHAPES[p.type][p.rot][i][0];
    int8_t by = SHAPES[p.type][p.rot][i][1];

    int8_t col = p.x + bx;
    int8_t row = p.y + by;

    if (col < 0 || col >= BOARD_COLS)
    {
      return false;
    }

    if (row >= BOARD_ROWS)
    {
      return false;
    }

    if (row >= 0 && board[row][col] != 0)
    {
      return false;
    }
  }

  return true;
}

static uint8_t randomPieceType()
{
  return (uint8_t)random(0, 7);
}

static void spawnPiece()
{
  active.type = nextType;
  active.rot = 0;
  active.x = 3;
  active.y = -1;

  nextType = randomPieceType();
  drawNextPiece();
}

static bool tryMove(int8_t dx, int8_t dy)
{
  Piece p = active;
  p.x += dx;
  p.y += dy;

  if (!canPlace(p))
  {
    return false;
  }

  active = p;
  return true;
}

static bool tryRotate()
{
  Piece original = active;

  Piece p = active;
  p.rot = (p.rot + 1) & 0x03;

  if (canPlace(p))
  {
    active = p;
    return true;
  }

  static const int8_t kicks[] = { -1, 1, -2, 2 };

  for (uint8_t i = 0; i < sizeof(kicks); i++)
  {
    p = active;
    p.rot = (p.rot + 1) & 0x03;
    p.x += kicks[i];

    if (canPlace(p))
    {
      active = p;
      return true;
    }
  }

  active = original;
  return false;
}

static void hardDrop()
{
  while (tryMove(0, 1))
  {
    // Baja hasta que ya no pueda.
  }
}

static void triggerGameOver()
{
  gameState = GAME_OVER;
  gameOverMs = millis();
  gameOverDrawn = false;
  sfxGameOver();
}

static void lockActiveToBoard()
{
  for (uint8_t i = 0; i < 4; i++)
  {
    int8_t bx = SHAPES[active.type][active.rot][i][0];
    int8_t by = SHAPES[active.type][active.rot][i][1];

    int8_t col = active.x + bx;
    int8_t row = active.y + by;

    if (row < 0)
    {
      triggerGameOver();
      return;
    }

    if (col >= 0 && col < BOARD_COLS && row >= 0 && row < BOARD_ROWS)
    {
      board[row][col] = active.type + 1;
    }
  }
}

static uint8_t clearFullLines()
{
  uint8_t cleared = 0;

  for (int8_t r = BOARD_ROWS - 1; r >= 0; r--)
  {
    bool full = true;

    for (uint8_t c = 0; c < BOARD_COLS; c++)
    {
      if (board[r][c] == 0)
      {
        full = false;
        break;
      }
    }

    if (!full)
    {
      continue;
    }

    cleared++;

    for (int8_t rr = r; rr > 0; rr--)
    {
      for (uint8_t c = 0; c < BOARD_COLS; c++)
      {
        board[rr][c] = board[rr - 1][c];
      }
    }

    for (uint8_t c = 0; c < BOARD_COLS; c++)
    {
      board[0][c] = 0;
    }

    r++;
  }

  return cleared;
}

static void addScoreForLines(uint8_t cleared)
{
  if (cleared == 0)
  {
    score += 5;
    return;
  }

  linesClearedTotal += cleared;

  switch (cleared)
  {
    case 1: score += 100; break;
    case 2: score += 300; break;
    case 3: score += 500; break;
    default: score += 800; break;
  }
}

static void lockAndSpawn(const Piece &oldScreenPiece)
{
  drawPiece(oldScreenPiece, true);

  lockActiveToBoard();

  if (gameState == GAME_OVER)
  {
    return;
  }

  sfxLock();

  uint8_t cleared = clearFullLines();
  addScoreForLines(cleared);

  if (cleared > 0)
  {
    sfxLine();
  }

  if (score > bestScore)
  {
    bestScore = score;
  }

  drawBoardCells();

  spawnPiece();

  if (!canPlace(active))
  {
    triggerGameOver();
    return;
  }

  drawPiece(active, false);
  drawInfoValues(false);

  lastDropMs = millis();
}

// =====================================================
// Inputs
// =====================================================

static bool buttonRepeatAction(uint8_t btnId, uint32_t &lastMs, uint32_t repeatMs)
{
  uint32_t now = millis();

  if (JWPLC_Buttons.pressed(btnId))
  {
    lastMs = now;
    return true;
  }

  if (JWPLC_Buttons.isDown(btnId))
  {
    if (lastMs == 0)
    {
      lastMs = now;
      return false;
    }

    if ((uint32_t)(now - lastMs) >= repeatMs)
    {
      lastMs = now;
      return true;
    }
  }
  else
  {
    lastMs = 0;
  }

  return false;
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

static void drawStartHint()
{
  auto &tft = JWPLC_Display.tft();

  cacheScreenGeometry();

  tft.fillScreen(COLOR_BG);

  tft.setTextSize(2);
  tft.setTextColor(COLOR_TITLE, COLOR_BG);
  tft.setCursor(12, 12);
  tft.print("JWPLC TETRIS");

  tft.drawFastHLine(0, 40, screenW, ST77XX_BLUE);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(16, 58);
  tft.print("OK: iniciar / rotar");

  tft.setCursor(16, 74);
  tft.print("LEFT/RIGHT: mover");

  tft.setCursor(16, 90);
  tft.print("DOWN: bajar rapido");

  tft.setCursor(16, 106);
  tft.print("UP: caida instantanea");

  tft.setCursor(16, 122);
  tft.print("ESC: volver a IDLE");

  tft.setCursor(16, 146);
  tft.print("FPS:");
  tft.print(GAME_TARGET_FPS);
  tft.print(" SPD:");
  tft.print(TETRIS_SPEED_PERCENT);
  tft.print("%");
}

static void drawGameScreen()
{
  auto &tft = JWPLC_Display.tft();

  tft.fillScreen(COLOR_BG);

  drawBoardFrame();
  drawBoardCells();

  drawInfoStatic();
  resetInfoCache();
  drawInfoValues(true);
  drawNextPiece();

  drawPiece(active, false);
}

static void drawGameOver()
{
  auto &tft = JWPLC_Display.tft();

  drawGameScreen();

  int16_t boxW = 180;
  int16_t boxH = 72;
  int16_t boxX = (screenW - boxW) / 2;
  int16_t boxY = (screenH - boxH) / 2;

  tft.fillRect(boxX, boxY, boxW, boxH, COLOR_BG);
  tft.drawRect(boxX, boxY, boxW, boxH, COLOR_BORDER);

  drawCenteredText("GAME OVER", boxY + 10, 2, ST77XX_RED);

  tft.setTextSize(1);
  tft.setTextColor(COLOR_TEXT, COLOR_BG);

  tft.setCursor(boxX + 20, boxY + 42);
  tft.print("Score: ");
  tft.print(score);

  tft.setCursor(boxX + 20, boxY + 56);
  tft.print("Volviendo a IDLE...");
}

// =====================================================
// Juego
// =====================================================

static void resetBoard()
{
  for (uint8_t r = 0; r < BOARD_ROWS; r++)
  {
    for (uint8_t c = 0; c < BOARD_COLS; c++)
    {
      board[r][c] = 0;
    }
  }
}

static void startGame()
{
  cacheScreenGeometry();

  randomSeed((uint32_t)micros());

  resetBoard();

  score = 0;
  linesClearedTotal = 0;

  callbackFrames = 0;
  fpsWindowStartMs = millis();
  measuredFps = 0;

  lastLeftMs = 0;
  lastRightMs = 0;
  lastDownMs = 0;

  nextType = randomPieceType();
  spawnPiece();

  gameState = GAME_RUNNING;
  gameOverDrawn = false;

  lastFrameMs = millis();
  lastDropMs = millis();

  JWPLC_Display.notifyActivity();
  JWPLC_Display.clearPendingInput();

  musicStart();

  drawGameScreen();

  debugPrintln("Tetris: START");
}

static void updateGame()
{
  updateMeasuredFps();

  uint32_t now = millis();

  Piece oldPiece = active;

  bool pieceChanged = false;
  bool shouldLock = false;

  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    gameState = GAME_WAIT_OK;
    JWPLC_Display.goIdle();
    return;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    if (tryRotate())
    {
      pieceChanged = true;
      sfxRotate();
    }
  }

  if (JWPLC_Buttons.pressed(BTN_UP))
  {
    hardDrop();
    shouldLock = true;
    sfxDrop();
  }

  if (!shouldLock && buttonRepeatAction(BTN_LEFT, lastLeftMs, LR_REPEAT_MS))
  {
    if (tryMove(-1, 0))
    {
      pieceChanged = true;
      sfxMove();
    }
  }

  if (!shouldLock && buttonRepeatAction(BTN_RIGHT, lastRightMs, LR_REPEAT_MS))
  {
    if (tryMove(1, 0))
    {
      pieceChanged = true;
      sfxMove();
    }
  }

  if (!shouldLock && buttonRepeatAction(BTN_DOWN, lastDownMs, SOFT_DROP_INTERVAL_MS))
  {
    if (tryMove(0, 1))
    {
      pieceChanged = true;
      lastDropMs = now;
    }
    else
    {
      shouldLock = true;
    }
  }

  if (!shouldLock && (uint32_t)(now - lastDropMs) >= currentDropIntervalMs())
  {
    lastDropMs = now;

    if (tryMove(0, 1))
    {
      pieceChanged = true;
    }
    else
    {
      shouldLock = true;
    }
  }

  if (shouldLock)
  {
    lockAndSpawn(oldPiece);
    return;
  }

  if (pieceChanged)
  {
    drawPiece(oldPiece, true);
    drawPiece(active, false);
  }

  drawInfoValues(false);
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

    lastFrameMs = now;
    updateGame();
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
      gameOverDrawn = false;
      gameState = GAME_WAIT_OK;
      JWPLC_Display.goIdle();
    }

    return;
  }
}

extern "C" void jwplcUserDisplayExitCallback()
{
  musicStop();

  gameState = GAME_WAIT_OK;

  measuredFps = 0;
  callbackFrames = 0;
  fpsWindowStartMs = 0;

  debugPrintln("Tetris: retorno a IDLE");
}

// =====================================================
// Arduino setup / loop
// =====================================================

void setup()
{
  Serial.begin(115200);
  delay(800);

  buzzerBegin();

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
    Serial.println("JWPLC TFT Tetris - Optimized + Buzzer");
    Serial.print("FPS objetivo: ");
    Serial.println(GAME_TARGET_FPS);
    Serial.print("Velocidad: ");
    Serial.print(TETRIS_SPEED_PERCENT);
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
      Serial.println("Presiona OK en IDLE para iniciar Tetris.");
    }
  }

  serviceBuzzerMusic();

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
      Serial.print(TETRIS_SPEED_PERCENT);
      Serial.print("%");

      Serial.print(" | Score: ");
      Serial.print(score);

      Serial.print(" | Lines: ");
      Serial.println(linesClearedTotal);
    }
  }

  delay(2);
}

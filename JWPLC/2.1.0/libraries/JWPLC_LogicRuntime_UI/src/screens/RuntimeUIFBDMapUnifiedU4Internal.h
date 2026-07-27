#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_U4_INTERNAL_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_UNIFIED_U4_INTERNAL_H

#include "RuntimeUIFBDMapUnified.h"
#include "../widgets/RuntimeUIWidgets.h"

namespace JWPLCUnifiedU4
{
enum class U4Type : uint8_t
{
  DigitalInput = 0,
  Not,
  And2,
  Ton,
  DigitalOutput,
  Count
};

enum class U4MainFocus : uint8_t
{
  Sources = 0,
  Parameters,
  Create
};

enum class U4TonBase : uint8_t
{
  Seconds = 0,
  Minutes,
  Hours
};

enum class U4TonField : uint8_t
{
  Major = 0,
  Minor,
  Base
};

struct U4State
{
  const RuntimeUIFBDMapUnified *owner = nullptr;
  RuntimeUIV2ReadModel *model = nullptr;

  bool addSelected = false;
  bool previewDrawn = false;
  uint16_t addOrigin = 0;

  U4Type type = U4Type::DigitalInput;
  U4MainFocus mainFocus = U4MainFocus::Parameters;
  uint8_t sourceInput = 0;
  uint8_t parameterIndex = 0;
  uint16_t sourceA = JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  uint16_t sourceB = JWPLC_LOGIC_V2_SOURCE_CONST_TRUE;
  uint16_t sourceBackup = JWPLC_LOGIC_V2_SOURCE_CONST_FALSE;
  uint8_t resource = 0;
  uint8_t resourceBackup = 0;

  uint32_t tonMajor = 1;
  uint32_t tonMinor = 0;
  U4TonBase tonBase = U4TonBase::Seconds;
  U4TonField tonFocus = U4TonField::Major;
  uint32_t tonMajorBackup = 1;
  uint32_t tonMinorBackup = 0;
  U4TonBase tonBaseBackup = U4TonBase::Seconds;

  bool applying = false;
  bool error = false;
  uint16_t newIndex = 0;
};

extern U4State gU4;

static constexpr int16_t TYPE_X = 8;
static constexpr int16_t TYPE_Y = 33;
static constexpr int16_t TYPE_W = 132;
static constexpr int16_t TYPE_H = 22;
static constexpr int16_t TYPE_STEP = 25;

static constexpr int16_t GROUP_X[3] = {8, 110, 212};
static constexpr int16_t GROUP_Y = 42;
static constexpr int16_t GROUP_W = 98;
static constexpr int16_t GROUP_H = 24;

static constexpr int16_t LIST_X = 10;
static constexpr int16_t LIST_Y = 42;
static constexpr int16_t LIST_W = 300;
// LIST_H colisiona con una macro incluida por el entorno Arduino/ESP32.
static constexpr int16_t LIST_ROW_H = 24;
static constexpr int16_t LIST_STEP = 28;

static constexpr int16_t CONTEXT_X = 10;
static constexpr int16_t CONTEXT_Y = 72;
static constexpr int16_t CONTEXT_W = 300;
static constexpr int16_t CONTEXT_H = 78;
static constexpr int16_t FOOTER_Y = 157;

static constexpr int16_t TON_FIELD_X[3] = {10, 112, 214};
static constexpr int16_t TON_FIELD_W[3] = {95, 95, 96};
static constexpr int16_t TON_FIELD_Y = 104;
static constexpr int16_t TON_FIELD_H = 39;

void resetU4(const RuntimeUIFBDMapUnified *owner, RuntimeUIV2ReadModel *model);
void ensureU4(const RuntimeUIFBDMapUnified *owner, RuntimeUIV2ReadModel *model);
const char *typeName(U4Type type);
const char *typeShort(U4Type type);
LogicV2BlockType logicType(U4Type type);
uint8_t sourceCountForType(U4Type type);
uint8_t parameterCountForType(U4Type type);
const char *sourceRole(U4Type type, uint8_t input);
const char *parameterName(U4Type type);
uint16_t &sourceReference(uint8_t input);
uint16_t sourceValue(uint8_t input);
uint16_t sourceCandidateCount(const RuntimeUIV2ReadModel *model);
uint16_t sourceCandidateAt(const RuntimeUIV2ReadModel *model, uint16_t candidate);
uint16_t sourceCandidateIndex(const RuntimeUIV2ReadModel *model, uint16_t source);
void moveSource(RuntimeUIV2ReadModel *model, uint16_t &source, bool forward);
void formatSource(const RuntimeUIV2ReadModel *model,
                  uint16_t source,
                  char *destination,
                  size_t capacity);
uint8_t digitalInputCount(const RuntimeUIV2ReadModel *model);
uint8_t digitalOutputCount(const RuntimeUIV2ReadModel *model);
bool outputResourceUsed(const RuntimeUIV2ReadModel *model, uint8_t resource);
uint8_t firstAvailableOutput(const RuntimeUIV2ReadModel *model);
void moveResource(RuntimeUIV2ReadModel *model, bool forward);
const char *mainFocusName(U4MainFocus focus);
bool mainFocusSelectable(U4MainFocus focus);
void normalizeMainFocus();
void moveMainFocus(bool forward);
uint32_t u4TonMinorMaximum(U4TonBase base);
uint32_t tonMilliseconds();
const char *tonMajorLabel();
const char *tonMinorLabel();
const char *tonBaseText();
void changeTonBase(bool forward);
void formatTonConfigured(char *destination, size_t capacity);
void formatResource(char *destination, size_t capacity);
void drawFooter(const char *text,
                uint16_t color = JWPLCLogicRuntimeUIWidgets::COLOR_MUTED);
void drawTypeChoice(U4Type type, bool selected);
void drawDisabledGroup(uint8_t index, const char *label);
void drawMainGroup(U4MainFocus focus);
void drawListRow(uint8_t input, bool selected);
void drawMiniMap(const RuntimeUIV2ReadModel *model,
                 uint16_t selectedSource,
                 const uint8_t *levels,
                 const uint8_t *lanes,
                 uint8_t maxLevel,
                 int16_t x,
                 int16_t y,
                 int16_t width,
                 int16_t height);
void drawContextPanel(const uint8_t *levels,
                      const uint8_t *lanes,
                      uint8_t maxLevel,
                      int16_t panelY,
                      int16_t panelH,
                      bool sourceContext,
                      uint16_t source,
                      const char *message,
                      uint16_t messageColor = JWPLCLogicRuntimeUIWidgets::COLOR_MUTED);
void drawTonField(uint8_t index,
                  const char *label,
                  const char *value,
                  bool selected,
                  bool full);
void formatTwoDigit(uint32_t value, char *destination, size_t capacity);
LogicV2InputLink makeLink(uint16_t source);
}

#endif
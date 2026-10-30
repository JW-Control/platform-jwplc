#ifndef JWPLC_UI_EVENT_LOG_H
#define JWPLC_UI_EVENT_LOG_H

#include <Arduino.h>
#include <stddef.h>

#define JWPLC_UI_HAS_EVENT_LOG 1

static constexpr uint8_t JWPLC_EVENT_MAX_RAM_DEFAULT = 64;
static constexpr uint8_t JWPLC_EVENT_FRAM_MAX = 16;
static constexpr uint16_t JWPLC_EVENT_FRAM_ADDR = 0x1E00;
static constexpr uint32_t JWPLC_EVENT_FRAM_MAGIC = 0x4A574556; // "JWEV"

enum JWPLCEventSeverity : uint8_t
{
    JWPLC_EVENT_SEV_INFO = 0,
    JWPLC_EVENT_SEV_WARN = 1,
    JWPLC_EVENT_SEV_CRIT = 2
};

enum JWPLCEventCategory : uint8_t
{
    JWPLC_EVENT_CAT_ALARM = 0,
    JWPLC_EVENT_CAT_INTERLOCK = 1,
    JWPLC_EVENT_CAT_SYSTEM = 2,
    JWPLC_EVENT_CAT_PROCESS = 3
};

enum JWPLCEventTransition : uint8_t
{
    JWPLC_EVENT_TRANS_NORMAL = 0, // Cesó / Normalizado
    JWPLC_EVENT_TRANS_ACTIVE = 1, // Activado / Ocurrido
    JWPLC_EVENT_TRANS_ACK = 2     // Reconocido por operador
};

enum JWPLCEventSource : uint8_t
{
    JWPLC_EVENT_SRC_USER = 0,
    JWPLC_EVENT_SRC_ETH = 1,
    JWPLC_EVENT_SRC_MODBUS = 2,
    JWPLC_EVENT_SRC_SD = 3,
    JWPLC_EVENT_SRC_RTC = 4,
    JWPLC_EVENT_SRC_IO = 5
};

// Códigos estándar de sistema
enum JWPLCSystemEventCode : uint16_t
{
    JWPLC_EVT_SYS_BOOT = 100,
    JWPLC_EVT_SYS_ETH_UP = 101,
    JWPLC_EVT_SYS_ETH_DOWN = 102,
    JWPLC_EVT_SYS_MODBUS_ERR = 103,
    JWPLC_EVT_SYS_MODBUS_OK = 104,
    JWPLC_EVT_SYS_SD_MOUNT = 105,
    JWPLC_EVT_SYS_SD_EJECT = 106,
    JWPLC_EVT_SYS_RTC_SYNC = 107
};

#pragma pack(push, 1)
struct JWPLC_UIEventRecord
{
    uint32_t timestamp;  // Unix epoch (o uptime en s con bit 31=1)
    uint16_t code;       // Código numérico del evento
    uint8_t category;    // JWPLCEventCategory
    uint8_t transition;  // JWPLCEventTransition
    uint8_t source;      // JWPLCEventSource
    uint8_t severity;    // JWPLCEventSeverity
    uint8_t reserved[6]; // Relleno a 16 bytes exactos
};
#pragma pack(pop)

struct JWPLC_UIEventDescriptor
{
    uint16_t code;
    const char *label;
    uint8_t severity;
    uint8_t category;
};

struct JWPLC_UIEventLogColors
{
    uint16_t headerBackground;
    uint16_t headerText;
    uint16_t rowBackground;
    uint16_t rowBackgroundAlt;
    uint16_t rowText;
    uint16_t selectBackground;
    uint16_t selectText;
    uint16_t sevInfo;
    uint16_t sevWarn;
    uint16_t sevCrit;
    uint16_t frame;
};

struct JWPLC_UIEventLog
{
    uint8_t page;
    int16_t x;
    int16_t y;
    int16_t width;
    int16_t height;
    bool frame;
    uint8_t ramCapacity;
    bool enableFram;
    bool enableSd;
    const char *sdFileName;
    const JWPLC_UIEventDescriptor *descriptors;
    size_t descriptorCount;
    JWPLC_UIEventLogColors colors;
};

class JWPLC_TFTClass;

namespace JWPLCUIEventLog
{
    bool begin(const JWPLC_UIEventLog &config);
    void logEvent(uint16_t code, uint8_t transition = 1, uint8_t category = 3, uint8_t severity = 0, const char *customText = nullptr);
    size_t count();
    bool getRecord(size_t index, JWPLC_UIEventRecord &outRecord);
    void clear();

    void service();
    bool isPageActive();
    bool inDetail();
    void exitDetail();
    void draw(JWPLC_TFTClass &tft);
}

#endif // JWPLC_UI_EVENT_LOG_H

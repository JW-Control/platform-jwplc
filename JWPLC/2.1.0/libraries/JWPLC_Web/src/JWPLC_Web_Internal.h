// Estado interno compartido de JWPLC_Web (no es API del sketch).
#pragma once

#include <Arduino.h>

#include "JWPLC_Web.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace jwweb
{
const uint8_t T_BOOL = 1;
const uint8_t T_INT = 2;
const uint8_t T_LONG = 3;
const uint8_t T_ULONG = 4;
const uint8_t T_FLOAT = 5;

const uint8_t ID_LEN = 16;
const uint8_t LABEL_LEN = 32;
const uint8_t UNIT_LEN = 8;
const uint8_t GROUP_LEN = 20;

struct Var
{
    char id[ID_LEN];
    char label[LABEL_LEN];
    char unit[UNIT_LEN];
    char group[GROUP_LEN];
    uint8_t type;
    void *ptr;
    float minV, maxV;
    bool hasRange;
    uint8_t decimals; // 255 = automático
    uint8_t writeRole;
    int8_t trendSlot; // -1 = sin tendencia
};

struct Alarm
{
    char id[ID_LEN];
    char text[LABEL_LEN];
    uint8_t priority;
    bool active;
    bool acked;
    uint32_t since; // millis() del último cambio
    char ackBy[ID_LEN];
};

const uint8_t EV_MAX = 24;
const uint8_t EV_ON = 1, EV_OFF = 2, EV_ACK = 3;
struct AlarmEvent
{
    uint32_t t;
    uint8_t alarm;
    uint8_t kind;
    char user[ID_LEN];
};

const uint8_t TREND_POINTS = 120;

const uint8_t AUDIT_MAX = 16;
struct Audit
{
    uint32_t t;
    char user[ID_LEN];
    char text[56];
};

const uint8_t USER_NAME = 16;
struct User
{
    char name[USER_NAME];
    uint8_t role;
    uint8_t salt[16];
    uint8_t key[32]; // PBKDF2-HMAC-SHA256(clave, sal, PBKDF2_ITER), lo deriva la página
};
const uint32_t PBKDF2_ITER = 2000;

struct State
{
    SemaphoreHandle_t lock = nullptr;
    char name[32] = "JWPLC Basic";
    uint16_t port = 80;
    bool running = false;
    // IP vigente, la actualiza la tarea web: url() no toca el bus SPI (se puede
    // llamar desde el dibujo de la pantalla, que ya tiene el bus tomado).
    volatile uint32_t ip = 0;

    Var vars[WEB_MAX_VARIABLES];
    uint8_t varCount = 0;

    Alarm alarms[WEB_MAX_ALARMAS];
    uint8_t alarmCount = 0;
    AlarmEvent events[EV_MAX];
    uint8_t evHead = 0, evCount = 0;

    uint8_t trendVar[WEB_MAX_TENDENCIAS];
    uint8_t trendCount = 0;
    float trend[WEB_MAX_TENDENCIAS][TREND_POINTS];
    uint8_t trendHead = 0, trendFill = 0;
    uint16_t trendSeconds = 5;
    uint32_t trendLast = 0;

    Audit audit[AUDIT_MAX];
    uint8_t auditHead = 0, auditCount = 0;

    User users[WEB_MAX_USUARIOS];
    uint8_t userCount = 0;
    bool usersLoaded = false;
    uint8_t secret[16]; // sal falsa para usuarios que no existen

    bool keysAllowed = true;
};

extern State S;

// Bloqueo del estado compartido entre el loop y la tarea del servidor.
struct Guard
{
    Guard();
    ~Guard();
};

float readVar(const Var &v);
bool writeVar(Var &v, float value);
void formatVar(const Var &v, char *out, size_t size);

void addEvent(uint8_t alarm, uint8_t kind, const char *user);
void addAudit(const char *user, const char *text);
void sampleTrend();

// Usuarios (NVS).
void loadUsers();
bool saveUsers();
int8_t findUser(const char *name);
void randomBytes(uint8_t *out, size_t n);

// Código de primer ingreso.
const char *currentPairingCode();
bool checkPairingCode(const char *code);

// Servidor (JWPLC_Web_Server.cpp).
bool startServer();
uint8_t openSessions();
void dropSessions();
bool screenViewed();
} // namespace jwweb

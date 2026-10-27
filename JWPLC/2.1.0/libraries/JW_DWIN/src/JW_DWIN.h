#ifndef JW_DWIN_H
#define JW_DWIN_H

#include <Arduino.h>
#include "JW_DWIN_Transport.h"

// =====================================================
// JW_DWIN - pantallas DWIN DGUS
// =====================================================
//
// API de objetos (teclas, valores, textos) sobre la memoria VP de la DWIN,
// independiente del medio fisico (ver JW_DWIN_Transport).
//
// Compatible con la API de la clase JW_DWIN de JW_DWIN_RS485 1.x:
// mismos structs DWIN_Key / DWIN_Value / DWIN_String y mismos metodos.
// Cambia el constructor: recibe un transporte en lugar del HardwareSerial.
//
// Cache: un valor, texto o estado tactil solo se da por enviado cuando la
// pantalla confirma la escritura. Un envio fallido se reintenta en la
// siguiente llamada. Al recuperar el enlace se invalida toda la cache.

// =======================
// ESTRUCTURAS DWIN
// =======================
// Mismo orden de campos que JW_DWIN_RS485 1.x: los objetos declarados
// con { page, addr, ... } no cambian. Los campos agregados van al final.

struct DWIN_Key
{
    uint16_t page;            // Página de ubicación
    uint8_t addr;             // Dirección VP baja (offset dentro de la página)
    uint8_t ID;               // Identificador en la página
    uint8_t Code;             // Tipo de botón (ej: 0x05: Return Key Code)
    uint16_t State = 1;       // Habilitado(1) / Deshabilitado(0)
    bool Enable = 0;          // Botón pulsado correctamente
    uint16_t val = 0;         // Valor anterior (si aplica)
    uint16_t touchEpoch = 0;  // Época en que State fue confirmado por la DWIN
};

struct DWIN_String
{
    uint16_t page;       // Página de ubicación
    uint16_t addr;       // Dirección VP
    String val;          // Último string enviado
    int Length;          // Cantidad fija de caracteres en HMI
    uint16_t epoch = 0;  // Época en que val fue confirmado por la DWIN
};

struct DWIN_Value
{
    uint16_t page;            // Página de ubicación
    uint16_t addr;            // Dirección VP
    uint8_t ID;               // Identificador en la página
    uint8_t Code;             // Tipo de control (00,01,02,03...)
    uint8_t decimal;          // Cantidad de decimales
    uint16_t State = 1;       // Habilitado(1) / Deshabilitado(0)
    uint16_t val = 0;         // Valor entero en crudo
    float fval = 0;           // Valor flotante escalado
    uint16_t epoch = 0;       // Época en que val fue confirmado por la DWIN
    uint16_t touchEpoch = 0;  // Época en que State fue confirmado por la DWIN
};

// ===== DEBUG MASKS =====
enum : uint16_t
{
    DWIN_DBG_NONE = 0,
    DWIN_DBG_TX = 1 << 0,   // operaciones enviadas
    DWIN_DBG_RX = 1 << 1,   // lecturas / datos recibidos
    DWIN_DBG_ERR = 1 << 2,  // errores de transporte
    DWIN_DBG_TIME = 1 << 3, // tiempos de transacción
    DWIN_DBG_FLOW = 1 << 4, // flujo/llamadas de alto nivel
    DWIN_DBG_ALL = 0xFFFF
};

#ifndef JW_DWIN_OFFLINE_FAILURES
#define JW_DWIN_OFFLINE_FAILURES 3
#endif

#ifndef JW_DWIN_OFFLINE_RETRY_MS
#define JW_DWIN_OFFLINE_RETRY_MS 500
#endif

// Mayor texto admitido por sendText() (caracteres).
#ifndef JW_DWIN_MAX_TEXT
#define JW_DWIN_MAX_TEXT 128
#endif

// checkKeys() / readValues(): registros por lectura (Modbus admite hasta 125)
// y mayor hueco entre dos VPs que se cubre con la misma lectura. Leer un registro de más
// cuesta 2 bytes; un pedido aparte, una trama completa y la espera de la DWIN.
#ifndef JW_DWIN_MAX_READ
#define JW_DWIN_MAX_READ 125
#endif

#ifndef JW_DWIN_READ_MAX_GAP
#define JW_DWIN_READ_MAX_GAP 16
#endif

class JW_DWIN
{
public:
    explicit JW_DWIN(JW_DWIN_Transport &transport);

    // Inicia el transporte.
    bool begin();

    // Cambiar de página en el HMI (escribe comando estándar 0x5A01 en 0x0084).
    bool setPage(uint16_t page);

    // Habilitar / deshabilitar controles táctiles (Key / Value).
    // Escribe {0x5AA5, page, ID<<8|Code, estado} en 0x00B0.
    template <typename T>
    void enableTouch(T &obj, uint16_t newState, bool force = false)
    {
        if (newState != obj.State || obj.touchEpoch != _epoch || force)
        {
            uint16_t frame[4];
            frame[0] = 0x5AA5;                   // Header
            frame[1] = obj.page;                 // Page ID
            frame[2] = (obj.ID << 8) | obj.Code; // ID + Code
            frame[3] = newState;                 // New state

            obj.State = newState;
            obj.touchEpoch = writeRegisters(0x00B0, frame, 4, "enableTouch")
                ? _epoch
                : 0;
            delay(_touchDelayMs);
        }
    }

    // Ejecuta fn() con una salida y máscara de debug temporales.
    template <typename F>
    void withDebug(Stream &out, uint16_t mask, F &&fn)
    {
        Stream *oldOut = _dbg;
        uint16_t oldMask = _dbgMask;
        _dbg = &out;
        _dbgMask = mask;

        fn();

        _dbg = oldOut;
        _dbgMask = oldMask;
    }

    // Escribe un valor numérico (entero+float) en el HMI.
    void sendValue(DWIN_Value &obj, uint16_t newValue,
                   bool forceSend = false, bool debug = false);

    // Lee un valor desde el HMI. Devuelve true si hubo cambio.
    // La lectura que restablece el enlace nunca cuenta como cambio: la
    // pantalla pudo reiniciarse y mostrar su valor inicial. En ese caso el
    // objeto queda sin confirmar (ver isConfirmed) y conserva su valor.
    bool readValue(DWIN_Value &obj, bool debug = false);

    // readValue() de varios objetos con una lectura por tramo de VPs cercanos.
    // changed[i] (opcional) queda en true si objs[i] cambió. Admite nullptr.
    // Devuelve false si alguna lectura falló (esos objetos no cambian).
    bool readValues(DWIN_Value *const objs[], uint8_t count, bool changed[] = nullptr);

    // true si la pantalla confirmó el valor/texto del objeto desde la última
    // reconexión. Un valor editable por el operador solo debe leerse como
    // edición cuando está confirmado; si no, se reenvía primero.
    template <typename T>
    bool isConfirmed(const T &obj) const
    {
        return obj.epoch == _epoch;
    }

    // Envía texto a un campo string fijo. Devuelve true si quedó confirmado.
    bool sendText(DWIN_String &obj, const String &newText,
                  bool forceSend = false);

    // Lee un botón tipo Key y, si se detecta pulsación, lo resetea en la HMI.
    void checkKey(DWIN_Key &key);

    // Lee varias teclas con una lectura por tramo de VPs cercanos (en vez de
    // una por tecla) y resetea en la HMI solo las pulsadas. Como checkKey,
    // solo activa Enable; quien la consume la vuelve a 0. Admite nullptr.
    // Devuelve false si alguna lectura falló (esas teclas no cambian).
    bool checkKeys(DWIN_Key *const keys[], uint8_t count);

    // Manejo tipo encoder con dos teclas (Minus / Plus), con vuelta circular.
    void keyEncoder(DWIN_Key &minus, DWIN_Key &plus,
                    uint32_t &count,
                    uint32_t minv, uint32_t maxv);

    // Acceso directo a un VP.
    bool readRegister(uint16_t vp, uint16_t &value);
    bool readRegisters(uint16_t vp, uint16_t count, uint16_t *values);
    bool writeRegister(uint16_t vp, uint16_t value);

    // ===== Enlace =====
    bool isOnline() const;
    uint32_t failureCount() const;
    uint32_t lastSuccessMs() const;
    uint16_t currentPage() const;

    // true una sola vez tras volver a comunicar con la DWIN.
    bool reconnected();

    // Fallos seguidos para considerar la pantalla sin enlace y periodo de
    // reintento en ese estado (un intento cada retryMs).
    void setOfflinePolicy(uint8_t failures, uint32_t retryMs);

    // Espera tras cambiar el estado táctil (25 ms como en 1.x).
    void setTouchDelay(uint16_t delayMs);

    // Invalida la cache: todo se reenviará en la próxima llamada.
    void invalidateCache();

    const char *lastErrorString() const;

    // ===== Debug API =====
    void setDebug(bool enable);
    void setDebug(Stream &out, uint16_t mask = DWIN_DBG_ALL);
    void setDebugMask(uint16_t mask);
    uint16_t debugMask() const { return _dbgMask; }

private:
    JW_DWIN_Transport &_transport;
    bool _started;

    uint16_t _epoch;
    uint8_t _failures;
    uint8_t _offlineFailures;
    uint32_t _offlineRetryMs;
    uint32_t _failureTotal;
    uint32_t _lastAttemptMs;
    uint32_t _lastSuccessMs;
    bool _reconnected;
    uint16_t _page;
    uint16_t _touchDelayMs;

    Stream *_dbg;
    uint16_t _dbgMask;

    static uint16_t vp(uint16_t page, uint16_t addr) { return page * 256 + addr; }
    static float divider(uint8_t decimals);

    bool allowAttempt();
    void registerResult(bool ok);

    bool writeRegister(uint16_t address, uint16_t value, const char *tag);
    bool writeRegisters(uint16_t address, const uint16_t *values, uint16_t count, const char *tag);
    bool readRegister(uint16_t address, uint16_t &value, const char *tag);
    bool readRegisters(uint16_t address, uint16_t count, uint16_t *values, const char *tag);

    // Siguiente tramo de VPs (desde `from`) para checkKeys / readValues.
    template <typename T>
    static bool nextSpan(T *const objs[], uint8_t count, uint32_t &from,
                         uint16_t &start, uint16_t &n);

    void dbgHeader(const char *tag);
    void dbgResult(const char *tag, uint16_t address, uint16_t value, bool ok, uint32_t t0);
};

#endif // JW_DWIN_H

// JWPLC_mBlock.h
//
// Ayudantes para los bloques de mBlock en la JWPLC Basic. La extensión
// jwplc_basic envía este archivo junto al sketch, así el código que muestra
// mBlock queda en llamadas cortas (como MeMCore.h en mBot):
//
//   JWPLC_Screen.begin();                 // 4 líneas de texto en la pantalla de usuario
//   JWPLC_Screen.print(2, contador);      // texto, números o verdadero/falso
//   JWPLC_Screen.openWith(BTN_OK);        // OK abre mi pantalla desde la de entradas/salidas
//   JWPLC_Screen.closeAfter(30);          // volver a entradas/salidas tras 30 s sin tocar botones
//   JWPLC_Keypad.pressed(BTN_OK);         // OK pulsado, si no es una tecla de la pantalla
//   JWPLC_Sound.play(523, 0.5);           // nota de 523 Hz durante 0,5 s
//   JWPLC_Sound.volume(50);               // volumen al 50 %
//   JWPLC_Timer.seconds();                // cronómetro en segundos
//   JWPLC_Clock.timeText();               // "HH:MM:SS" del reloj (RTC)
//   JWPLC_Memory.save(1, contador);       // número que no se borra al apagar (FRAM)
//   JWPLC_Files.appendLine("datos.txt", x); // línea nueva en la microSD
//
// WiFi, RS485, Modbus RTU, Ethernet y Modbus TCP van en headers aparte
// (JWPLC_mBlock_WiFi.h, _RS485.h, _ModbusRTU.h, _Ethernet.h, _ModbusTCP.h)
// para que cada programa sólo compile y enlace lo que usa.
//
// Sólo header: se compila con el sketch que lo incluye y no añade nada a los
// demás sketches ni precompilados al package. Usa la API pública del package
// 2.1.0-alpha.14: JWPLC_Display (campos HMI, sin TFT_eSPI), JWPLC_Buzzer,
// JWPLC_Time/JWPLC_RTC, JWPLC_FRAM y JWPLC_SD (con el mutex del bus SPI).

#ifndef JWPLC_MBLOCK_H
#define JWPLC_MBLOCK_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_Buzzer.h>
#include <JWPLC_GlobalPeripherals.h>
#include <JWPLC_RuntimeView.h>

namespace jwplc_mblock
{
// Texto para pantalla y archivos. Las variables de mBlock son float:
// 3 se muestra "3", no "3.00"; los booleanos, "SI"/"NO".
inline String text(const String &value) { return value; }
inline String text(const char *value) { return String(value); }
inline String text(double value)
{
    if (value == (double)(long)value)
        return String((long)value);
    return String(value, 2);
}
inline String text(float value) { return text((double)value); }
inline String text(int value) { return String(value); }
inline String text(unsigned int value) { return String(value); } // uint32_t en ESP32
inline String text(long value) { return String(value); }
inline String text(unsigned long value) { return String(value); }
inline String text(bool value) { return String(value ? "SI" : "NO"); }

// Páginas con MENU de la pantalla del HMI Designer (las anota JWPLC_mBlock_HMI.h).
inline const JWPLC_UIMenu *menus = nullptr;
inline size_t menuCount = 0;

inline const JWPLC_UIMenu *rememberMenus(const JWPLC_UIMenu *list, size_t count)
{
    menus = list;
    menuCount = count;
    return list;
}

inline bool menuOnPage(uint8_t page)
{
    for (size_t i = 0; i < menuCount; i++)
        if (menus[i].page == page)
            return true;
    return false;
}
} // namespace jwplc_mblock

// Botones de «abrir mi pantalla con…» que no son una tecla.
constexpr uint8_t JWPLC_ANY_BUTTON = 0xFE;
constexpr uint8_t JWPLC_NO_BUTTON = 0xFF;

// Bloque "unir": como String(a) + String(b), pero con el formato de arriba
// ("Arranques: 3" y no "Arranques: 3.00").
template <typename A, typename B>
inline String JWPLC_Join(A a, B b)
{
    return jwplc_mblock::text(a) + jwplc_mblock::text(b);
}

// Pantalla para niños: 4 líneas de texto sobre los campos de JWPLC_Display.
// setText() sólo redibuja si el texto cambió, así que escribir en cada vuelta
// de loop() no carga el bus SPI.
class JWPLC_ScreenLines
{
public:
    static const uint8_t LINES = 4;

    // Letra tamaño 2 (12x16 px): 24 caracteres por línea (10 + 24 * 12 = 298 px
    // de 320) y 4 líneas cada 34 px en los 170 px de alto.
    static const uint8_t CHARS = 24;
    static const uint8_t TEXT_SIZE = 2;

    void begin()
    {
        static const JWPLC_UIField fields[LINES] = {
            line(1, 18), line(2, 52), line(3, 86), line(4, 120)};

        JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
        JWPLC_Display.setFields(fields, LINES);
        // Como una pantalla nueva del HMI Designer: cualquier botón abre mi
        // pantalla y ESC vuelve a la de entradas/salidas (sin esto el package
        // no despierta la IDLE y, tras ESC, la pantalla no vuelve).
        JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
        JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY);
        applyProgramSettings();
        JWPLC_Display.enterUserUI();
    }

    // Cambio entre la pantalla de entradas/salidas (IDLE) y mi pantalla, con
    // las opciones de «Pantalla y arranque» del HMI Designer:
    //   openWith(JWPLC_ANY_BUTTON | BTN_x | JWPLC_NO_BUTTON)   entrar
    //   closeWith(BTN_ESC | JWPLC_NO_BUTTON), closeAfter(s)    volver
    // Lo que pide el programa se recuerda: una pantalla del Designer trae su
    // configuración (jwplcHMISetup) y JWPLC_HMI.begin() la vuelve a aplicar.
    void openWith(uint8_t button)
    {
        _wake = button;
        _wakeSet = true;
        applyWake();
    }

    void closeWith(uint8_t button)
    {
        _back = (button == BTN_ESC) ? BACK_ESC : BACK_NONE;
        _backSet = true;
        applyBack();
    }

    void closeAfter(double seconds)
    {
        const long s = constrain(lround(seconds), 1L, 3600L); // como el Designer
        _timeoutMs = (uint32_t)s * 1000UL;
        _back = BACK_TIMEOUT;
        _backSet = true;
        applyBack();
    }

    bool isShown() const { return !JWPLC_Display.isIdleMode(); }

    void applyProgramSettings()
    {
        if (_wakeSet)
            applyWake();
        if (_backSet)
            applyBack();
    }

    template <typename T>
    void print(int line, T value)
    {
        if (line >= 1 && line <= LINES)
            JWPLC_Display.setText((uint8_t)line, jwplc_mblock::text(value).c_str());
    }

    void clear()
    {
        for (int i = 1; i <= LINES; i++)
            print(i, "");
    }

private:
    enum Back : uint8_t
    {
        BACK_ESC,
        BACK_TIMEOUT,
        BACK_NONE
    };

    uint8_t _wake = JWPLC_ANY_BUTTON;
    Back _back = BACK_ESC;
    uint32_t _timeoutMs = 15000;
    bool _wakeSet = false;
    bool _backSet = false;

    void applyWake()
    {
        if (_wake == JWPLC_ANY_BUTTON)
        {
            JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON);
        }
        else if (_wake == JWPLC_NO_BUTTON)
        {
            JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_DISABLED);
        }
        else
        {
            JWPLC_Display.setIdleWakeMode(JWPLC_DisplayClass::IDLE_WAKE_BUTTON_ONLY);
            JWPLC_Display.setIdleWakeButton(_wake);
        }
    }

    void applyBack()
    {
        if (_back == BACK_TIMEOUT)
        {
            JWPLC_Display.setIdleTimeoutMs(_timeoutMs);
            JWPLC_Display.setIdleReturnMode(JWPLC_DisplayClass::IDLE_RETURN_TIMEOUT);
        }
        else
        {
            JWPLC_Display.setIdleReturnMode(_back == BACK_ESC ? JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY
                                                              : JWPLC_DisplayClass::IDLE_RETURN_DISABLED);
        }
    }

    static JWPLC_UIField line(uint8_t id, int16_t y)
    {
        return JWPLC_UITextField(
            id,
            JWPLC_UIRect(10, y),
            JWPLC_UIText(nullptr, nullptr, CHARS),
            JWPLC_UITextFieldStyle(TEXT_SIZE));
    }
};

// Botonera de «cuando se pulse el botón» y «¿botón presionado?». La pantalla
// usa las mismas teclas y lee los mismos avisos pressed(): la tecla que abre
// mi pantalla, ESC para volver, el selector de páginas y el MENU del Designer.
// Un bloque sólo toma la tecla cuando no es de la pantalla, así no se la roba
// al sistema ni hace dos cosas con una pulsación (mismo criterio que el
// ejemplo 06.HMI_Botonera_Interactiva del package).
class JWPLC_KeypadHelper
{
public:
    bool pressed(uint8_t button) const
    {
        return isFree(button) && JWPLC_Buttons.pressed(button);
    }

    bool isDown(uint8_t button) const
    {
        return isFree(button) && JWPLC_Buttons.isDown(button);
    }

    bool isFree(uint8_t button) const
    {
        if (JWPLC_Display.isIdleMode())
        {
            switch (JWPLC_Display.idleWakeMode())
            {
            case JWPLC_DisplayClass::IDLE_WAKE_ANY_BUTTON:
                return false;
            case JWPLC_DisplayClass::IDLE_WAKE_BUTTON_ONLY:
                return button != JWPLC_Display.idleWakeButton();
            default:
                return true; // sin pantalla propia: todas las teclas son del programa
            }
        }

        if (JWPLC_Display.isUserPageSelection() || jwplc_mblock::menuOnPage(JWPLC_Display.userPage()))
            return false;

        if (JWPLC_Display.userPageCount() > 1)
            return button != BTN_ESC; // ESC vuelve al selector de páginas

        switch (JWPLC_Display.idleReturnMode())
        {
        case JWPLC_DisplayClass::IDLE_RETURN_ESC_ONLY:
            return button != BTN_ESC;
        case JWPLC_DisplayClass::IDLE_RETURN_BUTTON_ONLY:
            return button != JWPLC_Display.idleReturnButton();
        default:
            return true;
        }
    }
};

// La tecla que cambia entre la pantalla de entradas/salidas y mi pantalla ya
// cumplió su función: no debe llegar además a un bloque de botón. Hooks weak
// de JWPLC_Display (también los define JWPLC_LogicRuntime_UI, que los
// programas de mBlock no usan).
extern "C" void jwplcUserDisplayEnterCallback(void) { JWPLCButtons::clearPendingInput(); }
extern "C" void jwplcUserDisplayExitCallback(void) { JWPLCButtons::clearPendingInput(); }

// Sonido con espera, como "tocar nota durante" de mBot: el buzzer es no
// bloqueante y delay() cede la CPU al runtime JWPLC mientras suena.
class JWPLC_SoundHelper
{
public:
    void play(double hz, double seconds)
    {
        unsigned long ms = (unsigned long)(seconds * 1000);
        if (ms > 65000UL)
            ms = 65000UL;
        JWPLC_Buzzer.tone((uint16_t)hz, (uint16_t)ms);
        delay(ms);
    }

    // Volumen en % (0..100) sobre la escala 0..255 del buzzer.
    void volume(double percent)
    {
        JWPLC_Buzzer.setVolume((uint8_t)constrain((int)(percent * 255 / 100), 0, 255));
    }
};

// Cronómetro como el "temporizador" de mBot: segundos desde el arranque o
// desde el último reset().
class JWPLC_TimerHelper
{
public:
    double seconds() const { return (millis() - _start) / 1000.0; }
    void reset() { _start = millis(); }

private:
    unsigned long _start = 0;
};

// Reloj (RTC). Fecha y hora salen de JWPLC_Time, la vista cacheada por el
// runtime (sin transacciones I2C extra); la temperatura del chip del RTC se
// lee como mucho una vez por segundo.
class JWPLC_ClockHelper
{
public:
    String timeText() const
    {
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "%02u:%02u:%02u",
                 JWPLC_Time.hour(), JWPLC_Time.minute(), JWPLC_Time.second());
        return String(buffer);
    }

    String dateText() const
    {
        char buffer[11];
        snprintf(buffer, sizeof(buffer), "%02u/%02u/%04u",
                 JWPLC_Time.day(), JWPLC_Time.month(), JWPLC_Time.year());
        return String(buffer);
    }

    double temperature()
    {
        if (!_hasTemperature || millis() - _lastTemperatureMs >= 1000)
        {
            float celsius = 0;
            if (JWPLC_RTC.readTemperatureC(celsius))
            {
                _temperature = celsius;
                _hasTemperature = true;
            }
            _lastTemperatureMs = millis();
        }
        return _temperature;
    }

    // Pone el reloj con la hora de la computadora en el momento de compilar
    // (__DATE__/__TIME__). Sólo si el reloj no tiene hora o va atrasado
    // respecto a la compilación: así un reinicio no lo vuelve a la hora vieja.
    // Error esperado: el tiempo de subida (menos de un minuto).
    void setFromComputer()
    {
        JW_RTC::DateTime built = compileTime();
        JW_RTC::DateTime now;
        bool clockOk = JWPLC_RTC.isTimeValid() && JWPLC_RTC.read(now) && now.valid;
        if (clockOk && key(now) >= key(built))
            return;
        if (JWPLC_RTC.write(built))
            JWPLC_RTC.clearOscillatorStopFlag();
    }

private:
    float _temperature = 0;
    bool _hasTemperature = false;
    unsigned long _lastTemperatureMs = 0;

    static uint64_t key(const JW_RTC::DateTime &dt)
    {
        return ((((uint64_t)dt.year * 100 + dt.month) * 100 + dt.day) * 100 + dt.hour) * 10000ULL +
               (uint64_t)dt.minute * 100 + dt.second;
    }

    static JW_RTC::DateTime compileTime()
    {
        static const char *MONTHS = "JanFebMarAprMayJunJulAugSepOctNovDec";
        const char *date = __DATE__; // "Oct 24 2026"
        const char *time = __TIME__; // "12:34:56"
        JW_RTC::DateTime dt;
        char month[4] = {date[0], date[1], date[2], 0};
        dt.month = (uint8_t)((strstr(MONTHS, month) - MONTHS) / 3 + 1);
        dt.day = (uint8_t)atoi(date + 4);
        dt.year = (uint16_t)atoi(date + 7);
        dt.hour = (uint8_t)atoi(time);
        dt.minute = (uint8_t)atoi(time + 3);
        dt.second = (uint8_t)atoi(time + 6);
        // Día de la semana (Sakamoto), 1 = domingo como JW_RTC.
        static const int T[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
        int y = dt.year - (dt.month < 3 ? 1 : 0);
        dt.dayOfWeek = (uint8_t)((y + y / 4 - y / 100 + y / 400 + T[dt.month - 1] + dt.day) % 7 + 1);
        dt.valid = true;
        return dt;
    }
};

// Memoria que no se borra al apagar: 100 números en los últimos 2 KB de la
// FRAM, cada uno como bloque con checksum (JW_FRAM writeBlock/readBlock,
// 8 B de cabecera + 4 B de float, cada 16 B). Una memoria nunca guardada vale 0.
// Si el valor no cambió no se escribe: guardar dentro de "por siempre" no carga
// el bus SPI compartido con la pantalla.
class JWPLC_MemoryHelper
{
public:
    static const uint8_t SLOTS = 100;
    static const uint32_t AREA_BYTES = 2048;
    static const uint8_t SLOT_STRIDE = 16;

    void save(int slot, double value)
    {
        uint32_t addr;
        if (!address(slot, addr))
            return;
        const float v = (float)value;
        const uint8_t i = (uint8_t)(slot - 1);
        if (_known[i] && _cache[i] == v)
            return;
        if (JWPLC_FRAM.writeBlock(addr, v, 1))
        {
            _cache[i] = v;
            _known[i] = true;
        }
    }

    double load(int slot)
    {
        uint32_t addr;
        if (!address(slot, addr))
            return 0;
        const uint8_t i = (uint8_t)(slot - 1);
        if (!_known[i])
        {
            float v = 0;
            if (!JWPLC_FRAM.readBlock(addr, v, 1))
                v = 0;
            _cache[i] = v;
            _known[i] = true;
        }
        return _cache[i];
    }

private:
    float _cache[SLOTS] = {};
    bool _known[SLOTS] = {};

    static bool address(int slot, uint32_t &addr)
    {
        const uint32_t size = JWPLC_FRAM.size();
        if (slot < 1 || slot > SLOTS || size < AREA_BYTES)
            return false;
        addr = size - AREA_BYTES + (uint32_t)(slot - 1) * SLOT_STRIDE;
        return true;
    }
};

// Archivos de texto en la microSD. JWPLC_SD protege cada operación con el
// mutex del bus SPI. Los nombres van en la raíz: "datos.txt" -> "/datos.txt".
class JWPLC_FilesHelper
{
public:
    bool ready() const { return JWPLC_SD.isReady(); }

    template <typename T>
    bool appendLine(const String &name, T value)
    {
        if (!JWPLC_SD.isReady())
            return false;
        JWPLCFile file = JWPLC_SD.open(path(name).c_str(), FILE_APPEND);
        if (!file)
            return false;
        file.println(jwplc_mblock::text(value));
        file.close();
        return true;
    }

    bool exists(const String &name) { return JWPLC_SD.isReady() && JWPLC_SD.exists(path(name).c_str()); }
    bool remove(const String &name) { return JWPLC_SD.isReady() && JWPLC_SD.remove(path(name).c_str()); }

private:
    static String path(const String &name) { return name.startsWith("/") ? name : "/" + name; }
};

// 16 "registros" Modbus para los bloques (registro 1 = 40001 = dirección 0).
// Los publican el esclavo Modbus RTU (JWPLC_mBlock_ModbusRTU.h) y el servidor
// Modbus TCP (JWPLC_mBlock_ModbusTCP.h); un maestro/SCADA puede escribirlos.
class JWPLC_RegistersHelper
{
public:
    static const uint8_t COUNT = 16;

    void set(int number, double value)
    {
        if (number < 1 || number > COUNT)
            return;
        const long v = lround(value);
        _values[number - 1] = (uint16_t)constrain(v, 0L, 65535L);
    }

    double get(int number) const
    {
        return (number >= 1 && number <= COUNT) ? _values[number - 1] : 0;
    }

    uint16_t *data() { return _values; }

private:
    uint16_t _values[COUNT] = {};
};

inline JWPLC_ScreenLines JWPLC_Screen;
inline JWPLC_KeypadHelper JWPLC_Keypad;
inline JWPLC_SoundHelper JWPLC_Sound;
inline JWPLC_TimerHelper JWPLC_Timer;
inline JWPLC_ClockHelper JWPLC_Clock;
inline JWPLC_MemoryHelper JWPLC_Memory;
inline JWPLC_FilesHelper JWPLC_Files;
inline JWPLC_RegistersHelper JWPLC_Registers;

#endif // JWPLC_MBLOCK_H

// JWPLC_mBlock.h
//
// Ayudantes para los bloques de mBlock en la JWPLC Basic. La extensión
// jwplc_basic envía este archivo junto al sketch, así el código que muestra
// mBlock queda en llamadas cortas (como MeMCore.h en mBot):
//
//   JWPLC_Screen.begin();            // 4 líneas de texto en la pantalla de usuario
//   JWPLC_Screen.print(2, contador); // texto, números o verdadero/falso
//   JWPLC_Sound.play(523, 0.5);      // nota de 523 Hz durante 0,5 s
//   JWPLC_Sound.volume(50);          // volumen al 50 %
//
// Sólo header: se compila con el sketch que lo incluye y no añade nada a los
// demás sketches ni precompilados al package. Usa la API pública del package
// 2.1.0-alpha.14: JWPLC_Display (campos HMI, sin TFT_eSPI) y JWPLC_Buzzer.

#ifndef JWPLC_MBLOCK_H
#define JWPLC_MBLOCK_H

#include <Arduino.h>
#include <JWPLC_Display.h>
#include <JWPLC_Buzzer.h>

// Pantalla para niños: 4 líneas de texto sobre los campos de JWPLC_Display.
// setText() sólo redibuja si el texto cambió, así que escribir en cada vuelta
// de loop() no carga el bus SPI.
class JWPLC_ScreenLines
{
public:
    static const uint8_t LINES = 4;

    void begin()
    {
        static const JWPLC_UIField fields[LINES] = {
            JWPLC_UITextField(1, 10, 18, nullptr, 24),
            JWPLC_UITextField(2, 10, 52, nullptr, 24),
            JWPLC_UITextField(3, 10, 86, nullptr, 24),
            JWPLC_UITextField(4, 10, 120, nullptr, 24)};

        JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
        JWPLC_Display.setFields(fields, LINES);
        JWPLC_Display.enterUserUI();
    }

    void print(int line, const String &text)
    {
        if (line >= 1 && line <= LINES)
            JWPLC_Display.setText((uint8_t)line, text.c_str());
    }
    void print(int line, const char *text) { print(line, String(text)); }
    void print(int line, double value)
    {
        // Las variables de mBlock son float: 3 se muestra "3", no "3.00".
        if (value == (double)(long)value)
            print(line, String((long)value));
        else
            print(line, String(value, 2));
    }
    void print(int line, float value) { print(line, (double)value); }
    void print(int line, int value) { print(line, String(value)); }
    void print(int line, long value) { print(line, String(value)); }
    void print(int line, unsigned long value) { print(line, String(value)); }
    void print(int line, bool value) { print(line, String(value ? "SI" : "NO")); }

    void clear()
    {
        for (int i = 1; i <= LINES; i++)
            print(i, "");
    }
};

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

inline JWPLC_ScreenLines JWPLC_Screen;
inline JWPLC_SoundHelper JWPLC_Sound;

#endif // JWPLC_MBLOCK_H

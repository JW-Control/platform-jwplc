#ifndef JWPLC_BUZZER_H
#define JWPLC_BUZZER_H

#include <Arduino.h>

// =====================================================
// Buzzer integrado del JWPLC Basic (GPIO26, piezo pasivo)
// =====================================================
// Objeto global JWPLC_Buzzer, disponible sin declararlo en el sketch.
// Todo es NO bloqueante: tone(), beep() y play() vuelven de inmediato y un
// esp_timer apaga la nota o pasa a la siguiente. No hace falta llamar a
// ningún service() en loop() ni usar delay().
//
// Uso típico (en setup()):
//
//   JWPLC_Buzzer.setVolume(32);          // 0 = mudo ... 255 = máximo
//   JWPLC_Buzzer.beep();                 // pitido corto
//
// Volumen (duty PWM de 8 bits, misma escala que los ejemplos Tetris/Flappy):
//   0 apagado · 16 muy bajo · 32 bajo · 48 medio-bajo · 64 medio
//   96 medio-alto · 128 fuerte · 255 máximo
//
// Un tone()/beep() durante una melodía la interrumpe; al terminar, la
// melodía sigue desde la nota en curso (como los efectos del Tetris).

#define JWPLC_HAS_BUZZER_API 1

struct JWPLC_BuzzerNote
{
    uint16_t freq;  // Hz; 0 = silencio
    uint16_t durMs; // duración total de la nota (incluye el corte final)
};

class JWPLC_BuzzerClass
{
public:
    // Volumen 0..255 (0 = mudo). Se puede llamar antes del primer sonido.
    void setVolume(uint8_t level);
    uint8_t volume() const;

    // Silencio global sin perder el volumen configurado.
    void setEnabled(bool enabled);
    bool enabled() const;

    // Nota única no bloqueante. durMs = 0 suena hasta stop().
    bool tone(uint16_t freq, uint16_t durMs);

    // Pitido corto (2 kHz por defecto).
    bool beep(uint16_t durMs = 60, uint16_t freq = 2000);

    // Melodía no bloqueante. notes debe seguir vivo mientras suene (const
    // global o static). speedPercent: 100 normal, 80 más lento, 120 más rápido.
    // Cada nota suena el 90 % de su duración y calla el 10 % restante.
    bool play(const JWPLC_BuzzerNote *notes, size_t count, bool loop = false, uint16_t speedPercent = 100);

    // Detiene la melodía y cualquier nota.
    void stop();

    bool isPlaying() const; // melodía en curso
    bool isBusy() const;    // melodía o nota sonando
};

extern JWPLC_BuzzerClass JWPLC_Buzzer;

#endif // JWPLC_BUZZER_H

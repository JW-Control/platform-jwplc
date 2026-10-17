/*
  07.Buzzer_Basic

  Buzzer integrado del JWPLC Basic (GPIO26) con el objeto global JWPLC_Buzzer.
  No hay que declarar el pin ni llamar a ningún service(): todo es no bloqueante.

  Botonera:
  - OK:    pitido corto (beep).
  - UP:    melodía de aviso (una vez).
  - DOWN:  melodía en bucle.
  - ESC:   detener.
  - LEFT / RIGHT: bajar / subir volumen (0..255).
*/

static const JWPLC_BuzzerNote AVISO[] = {
    {1568, 120}, {0, 60}, {1568, 120}, {0, 60}, {2093, 240}};

static const JWPLC_BuzzerNote BUCLE[] = {
    {880, 200}, {988, 200}, {1047, 200}, {0, 400}};

void setup()
{
    Serial.begin(115200);

    // Configuración del buzzer: sólo el volumen (16 muy bajo ... 255 máximo).
    JWPLC_Buzzer.setVolume(32);

    JWPLC_Buttons.clearPendingInput();
    JWPLC_Buzzer.beep(); // listo
}

void loop()
{
    if (JWPLC_Buttons.pressed(BTN_OK))
        JWPLC_Buzzer.beep();

    if (JWPLC_Buttons.pressed(BTN_UP))
        JWPLC_Buzzer.play(AVISO, sizeof(AVISO) / sizeof(AVISO[0]));

    if (JWPLC_Buttons.pressed(BTN_DOWN))
        JWPLC_Buzzer.play(BUCLE, sizeof(BUCLE) / sizeof(BUCLE[0]), true);

    if (JWPLC_Buttons.pressed(BTN_ESC))
        JWPLC_Buzzer.stop();

    int delta = 0;
    if (JWPLC_Buttons.pressed(BTN_LEFT))
        delta -= 16;
    if (JWPLC_Buttons.pressed(BTN_RIGHT))
        delta += 16;
    if (delta != 0)
    {
        const int v = (int)JWPLC_Buzzer.volume() + delta;
        JWPLC_Buzzer.setVolume((uint8_t)constrain(v, 0, 255));
        Serial.printf("Volumen: %u\n", JWPLC_Buzzer.volume());
        JWPLC_Buzzer.beep(40);
    }
}

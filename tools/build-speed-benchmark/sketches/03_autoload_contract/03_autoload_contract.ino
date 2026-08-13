// Smoke test del contrato de autoload JWPLC.
//
// Intencionalmente NO incluye headers JWPLC manualmente.
// Debe compilar solamente con lo expuesto automáticamente por el package.

JW_RTC *autoloadRtc()
{
    return &JWPLC_RTC;
}

JW_FRAM *autoloadFram()
{
    return &JWPLC_FRAM;
}

JW_SD *autoloadSd()
{
    return &JWPLC_SD;
}

JW_MatrixButtons *autoloadButtons()
{
    return &JWPLC_Buttons;
}

JW_RTC::DateTime autoloadClockSnapshot()
{
    return JWPLC_RTC.now();
}

bool autoloadHelpersReady()
{
    return JWPLCButtons::isReady() || JWPLCSD::isReady();
}

void setup()
{
    pinMode(I0_0, INPUT);
    pinMode(Q0_0, OUTPUT);

    (void)autoloadRtc();
    (void)autoloadFram();
    (void)autoloadSd();
    (void)autoloadButtons();

    JW_RTC::DateTime dt = autoloadClockSnapshot();
    (void)dt;

    (void)autoloadHelpersReady();

    // Verifica que la API pública del display siga visible desde Arduino.h.
    JWPLC_Display.forceRedraw();
}

void loop()
{
    digitalWrite(Q0_0, digitalRead(I0_0));
}

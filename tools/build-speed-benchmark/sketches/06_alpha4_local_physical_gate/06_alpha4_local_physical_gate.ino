// JWPLC v2.1.0-alpha.4
// Gate fisico local final despues de P1-P8.
//
// No incluye headers JWPLC manualmente.
// Usa el contrato de autoload normal del package.
//
// Cubre:
// - Display
// - RTC
// - FRAM (lectura no destructiva)
// - microSD (write/read/verify/remove)
// - botonera
// - 8 entradas TCA
// - 8 salidas TCA / reles
//
// Ethernet y RS-485/Modbus se validan en gates separados.

static const uint16_t INPUT_PINS[8] =
{
    I0_0, I0_1, I0_2, I0_3,
    I0_4, I0_5, I0_6, I0_7
};

static const uint16_t OUTPUT_PINS[8] =
{
    Q0_0, Q0_1, Q0_2, Q0_3,
    Q0_4, Q0_5, Q0_6, Q0_7
};

static const char *INPUT_NAMES[8] =
{
    "I0_0", "I0_1", "I0_2", "I0_3",
    "I0_4", "I0_5", "I0_6", "I0_7"
};

static const char *OUTPUT_NAMES[8] =
{
    "Q0_0", "Q0_1", "Q0_2", "Q0_3",
    "Q0_4", "Q0_5", "Q0_6", "Q0_7"
};

// Botonera fisica real de JWPLC Basic.
// CANCEL se representa internamente como BTN_ESC.
static const uint8_t BUTTON_COUNT = 6;

static const uint8_t BUTTON_IDS[BUTTON_COUNT] =
{
    BTN_UP,
    BTN_DOWN,
    BTN_LEFT,
    BTN_RIGHT,
    BTN_ESC,
    BTN_OK
};

static const char *BUTTON_NAMES[BUTTON_COUNT] =
{
    "UP",
    "DOWN",
    "LEFT",
    "RIGHT",
    "CANCEL",
    "OK"
};

enum GateStage : uint8_t
{
    GATE_AUTOMATIC = 0,
    GATE_BUTTONS,
    GATE_INPUTS,
    GATE_OUTPUTS,
    GATE_DISPLAY_CONFIRM,
    GATE_DONE
};

static GateStage gateStage = GATE_AUTOMATIC;

// ------------------------------------------------------------
// Resultados
// ------------------------------------------------------------

static bool displayReadyOk = false;
static bool rtcOk = false;
static bool framOk = false;
static bool sdOk = false;
static bool buttonsOk = false;
static bool inputsOk = false;
static bool outputsOk = false;
static bool displayVisualOk = false;

// ------------------------------------------------------------
// Estado botonera
// ------------------------------------------------------------

static bool buttonSeen[BUTTON_COUNT] = {};
static bool buttonStagePrepared = false;

// ------------------------------------------------------------
// Estado entradas
// ------------------------------------------------------------

static bool inputInitial[8] = {};
static bool inputChanged[8] = {};
static bool inputReturned[8] = {};
static bool inputStagePrepared = false;

// ------------------------------------------------------------
// Estado salidas
// ------------------------------------------------------------

static bool outputStagePrepared = false;
static bool outputSequenceDone = false;

static uint8_t outputIndex = 0;
static bool outputHighPhase = false;
static uint32_t outputNextMs = 0;

// ------------------------------------------------------------
// Estado automatico
// ------------------------------------------------------------

static bool automaticTestsExecuted = false;
static uint32_t automaticStartMs = 0;

// ------------------------------------------------------------

static void printPassFail(const char *name, bool ok)
{
    Serial.print(name);
    Serial.print("=");
    Serial.println(ok ? "PASS" : "FAIL");
}

static bool allButtonsSeen()
{
    for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
    {
        if (!buttonSeen[i])
        {
            return false;
        }
    }

    return true;
}

static bool allInputsReturned()
{
    for (uint8_t i = 0; i < 8; ++i)
    {
        if (!inputReturned[i])
        {
            return false;
        }
    }

    return true;
}

static void allOutputsOff()
{
    for (uint8_t i = 0; i < 8; ++i)
    {
        digitalWrite(OUTPUT_PINS[i], LOW);
    }
}

static void runRTCCheck()
{
    JW_RTC::DateTime dt = {};

    rtcOk = JWPLC_RTC.read(dt);

    Serial.println();
    Serial.println("[RTC]");

    if (rtcOk)
    {
        Serial.print("Fecha/hora leida: ");
        Serial.print(dt.year);
        Serial.print("-");
        Serial.print(dt.month);
        Serial.print("-");
        Serial.print(dt.day);
        Serial.print(" ");
        Serial.print(dt.hour);
        Serial.print(":");
        Serial.print(dt.minute);
        Serial.print(":");
        Serial.println(dt.second);
    }

    printPassFail("ALPHA4_RTC", rtcOk);
}

static void runFRAMCheck()
{
    Serial.println();
    Serial.println("[FRAM]");

    uint8_t firstRead[8] = {};
    uint8_t secondRead[8] = {};

    const uint32_t sizeBytes = JWPLC_FRAM.size();

    const bool read1 = JWPLC_FRAM.read(
        0,
        firstRead,
        sizeof(firstRead));

    delay(5);

    const bool read2 = JWPLC_FRAM.read(
        0,
        secondRead,
        sizeof(secondRead));

    const bool equal =
        memcmp(firstRead, secondRead, sizeof(firstRead)) == 0;

    framOk =
        sizeBytes == 8192 &&
        read1 &&
        read2 &&
        equal;

    Serial.print("Size bytes: ");
    Serial.println(sizeBytes);

    Serial.print("Read #1: ");
    Serial.println(read1 ? "OK" : "FAIL");

    Serial.print("Read #2: ");
    Serial.println(read2 ? "OK" : "FAIL");

    Serial.print("Lecturas iguales: ");
    Serial.println(equal ? "SI" : "NO");

    printPassFail("ALPHA4_FRAM", framOk);
}

static void runSDCheck()
{
    Serial.println();
    Serial.println("[SD]");

    static const char *PATH =
        "/jwplc_alpha4_physical_gate.tmp";

    static const char *PAYLOAD =
        "JWPLC_ALPHA4_SD_OK";

    if (!JWPLC_SD.isReady())
    {
        Serial.println("SD no estaba ready; intentando begin()...");
        (void)JWPLC_SD.begin();
    }

    if (!JWPLC_SD.isReady())
    {
        Serial.print("SD error: ");
        Serial.println(JWPLC_SD.lastErrorString());

        sdOk = false;
        printPassFail("ALPHA4_SD", false);
        return;
    }

    if (JWPLC_SD.exists(PATH))
    {
        (void)JWPLC_SD.remove(PATH);
    }

    JWPLCFile file = JWPLC_SD.open(PATH, FILE_WRITE);

    if (!file)
    {
        Serial.print("No se pudo abrir para escritura: ");
        Serial.println(JWPLC_SD.lastErrorString());

        sdOk = false;
        printPassFail("ALPHA4_SD", false);
        return;
    }

    const size_t payloadLength = strlen(PAYLOAD);

    const size_t written =
        file.write(
            reinterpret_cast<const uint8_t *>(PAYLOAD),
            payloadLength);

    file.close();

    if (written != payloadLength)
    {
        Serial.println("Cantidad escrita incorrecta.");

        (void)JWPLC_SD.remove(PATH);

        sdOk = false;
        printPassFail("ALPHA4_SD", false);
        return;
    }

    file = JWPLC_SD.open(PATH, FILE_READ);

    if (!file)
    {
        Serial.print("No se pudo abrir para lectura: ");
        Serial.println(JWPLC_SD.lastErrorString());

        (void)JWPLC_SD.remove(PATH);

        sdOk = false;
        printPassFail("ALPHA4_SD", false);
        return;
    }

    char buffer[32] = {};
    size_t used = 0;

    while (file.available() && used < sizeof(buffer) - 1)
    {
        const int value = file.read();

        if (value < 0)
        {
            break;
        }

        buffer[used++] = static_cast<char>(value);
    }

    file.close();

    buffer[used] = '\0';

    const bool contentOk =
        strcmp(buffer, PAYLOAD) == 0;

    const bool removeOk =
        JWPLC_SD.remove(PATH);

    sdOk = contentOk && removeOk;

    Serial.print("Readback: ");
    Serial.println(buffer);

    Serial.print("Remove: ");
    Serial.println(removeOk ? "OK" : "FAIL");

    printPassFail("ALPHA4_SD", sdOk);
}

static void runAutomaticChecks()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println(" ETAPA 1 - PRUEBAS AUTOMATICAS");
    Serial.println("========================================");

    displayReadyOk = JWPLC_Display.isReady();

    printPassFail(
        "ALPHA4_DISPLAY_READY",
        displayReadyOk);

    runRTCCheck();
    runFRAMCheck();
    runSDCheck();

    Serial.println();
    Serial.println("Pruebas automaticas terminadas.");
}

static void prepareButtonStage()
{
    if (buttonStagePrepared)
    {
        return;
    }

    buttonStagePrepared = true;

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
    {
        buttonSeen[i] = false;
    }

    JWPLC_Buttons.clearPendingInput();

    Serial.println();
    Serial.println("========================================");
    Serial.println(" ETAPA 2 - BOTONERA");
    Serial.println("========================================");
    Serial.println("Pulsa una vez cada boton:");
    Serial.println("UP DOWN LEFT RIGHT CANCEL OK");
    Serial.println();
}

static void runButtonStage()
{
    prepareButtonStage();

    if (!JWPLC_Buttons.taskRunning())
    {
        JWPLC_Buttons.update();
    }

    for (uint8_t i = 0; i < BUTTON_COUNT; ++i)
    {
        if (!buttonSeen[i] && JWPLC_Buttons.pressed(BUTTON_IDS[i]))
        {
            buttonSeen[i] = true;

            Serial.print("[BUTTON PASS] ");
            Serial.println(BUTTON_NAMES[i]);
        }
    }

    if (allButtonsSeen())
    {
        buttonsOk = true;

        Serial.println();
        printPassFail("ALPHA4_BUTTONS", true);

        gateStage = GATE_INPUTS;
    }
}

static void prepareInputStage()
{
    if (inputStagePrepared)
    {
        return;
    }

    inputStagePrepared = true;

    Serial.println();
    Serial.println("========================================");
    Serial.println(" ETAPA 3 - ENTRADAS DIGITALES");
    Serial.println("========================================");
    Serial.println("Activa y luego desactiva cada entrada.");
    Serial.println("No se asume polaridad HIGH/LOW.");
    Serial.println("Cada canal debe cambiar y regresar a su");
    Serial.println("estado inicial.");
    Serial.println();

    for (uint8_t i = 0; i < 8; ++i)
    {
        pinMode(INPUT_PINS[i], INPUT);

        inputInitial[i] =
            digitalRead(INPUT_PINS[i]) != LOW;

        inputChanged[i] = false;
        inputReturned[i] = false;

        Serial.print(INPUT_NAMES[i]);
        Serial.print(" inicial=");
        Serial.println(
            inputInitial[i] ? "HIGH" : "LOW");
    }

    Serial.println();
}

static void runInputStage()
{
    prepareInputStage();

    for (uint8_t i = 0; i < 8; ++i)
    {
        const bool current =
            digitalRead(INPUT_PINS[i]) != LOW;

        if (!inputChanged[i] &&
            current != inputInitial[i])
        {
            inputChanged[i] = true;

            Serial.print("[INPUT CAMBIO] ");
            Serial.print(INPUT_NAMES[i]);
            Serial.print(" -> ");
            Serial.println(current ? "HIGH" : "LOW");
        }

        if (inputChanged[i] &&
            !inputReturned[i] &&
            current == inputInitial[i])
        {
            inputReturned[i] = true;

            Serial.print("[INPUT PASS] ");
            Serial.println(INPUT_NAMES[i]);
        }
    }

    if (allInputsReturned())
    {
        inputsOk = true;

        Serial.println();
        printPassFail("ALPHA4_INPUTS", true);

        gateStage = GATE_OUTPUTS;
    }
}

static void prepareOutputStage()
{
    if (outputStagePrepared)
    {
        return;
    }

    outputStagePrepared = true;
    outputSequenceDone = false;

    outputIndex = 0;
    outputHighPhase = false;
    outputNextMs = millis();

    for (uint8_t i = 0; i < 8; ++i)
    {
        pinMode(OUTPUT_PINS[i], OUTPUT);
        digitalWrite(OUTPUT_PINS[i], LOW);
    }

    Serial.println();
    Serial.println("========================================");
    Serial.println(" ETAPA 4 - SALIDAS / RELES");
    Serial.println("========================================");
    Serial.println("Se activara Q0_0 ... Q0_7 una por una.");
    Serial.println("Verifica fisicamente que SOLO el rele");
    Serial.println("indicado conmuta en cada paso.");
    Serial.println();
}

static void restartOutputSequence()
{
    allOutputsOff();

    outputIndex = 0;
    outputHighPhase = false;
    outputSequenceDone = false;
    outputNextMs = millis() + 500;

    Serial.println();
    Serial.println("Repitiendo secuencia de salidas...");
}

static void finishOutputSequence()
{
    allOutputsOff();

    outputSequenceDone = true;

    Serial.println();
    Serial.println("Secuencia de salidas terminada.");
    Serial.println();
    Serial.println("Escribe en el Monitor Serie:");
    Serial.println("  Y = los 8 reles conmutaron correctamente");
    Serial.println("  R = repetir secuencia");
    Serial.println("  N = fallo fisico");
}

static void runOutputSequence()
{
    if (outputSequenceDone)
    {
        return;
    }

    const uint32_t now = millis();

    if ((int32_t)(now - outputNextMs) < 0)
    {
        return;
    }

    if (!outputHighPhase)
    {
        allOutputsOff();

        Serial.print("[OUTPUT ON] ");
        Serial.println(OUTPUT_NAMES[outputIndex]);

        digitalWrite(
            OUTPUT_PINS[outputIndex],
            HIGH);

        outputHighPhase = true;
        outputNextMs = now + 900;
    }
    else
    {
        digitalWrite(
            OUTPUT_PINS[outputIndex],
            LOW);

        Serial.print("[OUTPUT OFF] ");
        Serial.println(OUTPUT_NAMES[outputIndex]);

        outputHighPhase = false;
        ++outputIndex;

        if (outputIndex >= 8)
        {
            finishOutputSequence();
        }
        else
        {
            outputNextMs = now + 350;
        }
    }
}

static void prepareDisplayConfirmation()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println(" ETAPA 5 - CONFIRMACION VISUAL TFT");
    Serial.println("========================================");

    JWPLC_Display.setRunLed(true);
    JWPLC_Display.setBusLed(true);
    JWPLC_Display.setErrLed(true);
    JWPLC_Display.setEthLed(true);

    Serial.println("Verifica que el TFT esta operativo.");
    Serial.println("Los indicadores RUN/BUS/ERR/ETH se han");
    Serial.println("solicitado encendidos para facilitar");
    Serial.println("la comprobacion visual.");
    Serial.println();
    Serial.println("Escribe:");
    Serial.println("  Y = TFT correcto");
    Serial.println("  N = TFT incorrecto");
}

static void printFinalResult()
{
    Serial.println();
    Serial.println("========================================");
    Serial.println(" RESULTADO GATE FISICO LOCAL ALPHA4");
    Serial.println("========================================");

    printPassFail(
        "ALPHA4_DISPLAY_READY",
        displayReadyOk);

    printPassFail(
        "ALPHA4_RTC",
        rtcOk);

    printPassFail(
        "ALPHA4_FRAM",
        framOk);

    printPassFail(
        "ALPHA4_SD",
        sdOk);

    printPassFail(
        "ALPHA4_BUTTONS",
        buttonsOk);

    printPassFail(
        "ALPHA4_INPUTS",
        inputsOk);

    printPassFail(
        "ALPHA4_OUTPUTS",
        outputsOk);

    printPassFail(
        "ALPHA4_DISPLAY_VISUAL",
        displayVisualOk);

    const bool localPass =
        displayReadyOk &&
        rtcOk &&
        framOk &&
        sdOk &&
        buttonsOk &&
        inputsOk &&
        outputsOk &&
        displayVisualOk;

    Serial.println();

    printPassFail(
        "ALPHA4_LOCAL_PHYSICAL_GATE",
        localPass);

    Serial.println();
    Serial.println(
        "Ethernet y RS-485/Modbus quedan en gates separados.");
}

static char readCommand()
{
    while (Serial.available())
    {
        const char c = Serial.read();

        if (c == 'Y' || c == 'y')
        {
            return 'Y';
        }

        if (c == 'N' || c == 'n')
        {
            return 'N';
        }

        if (c == 'R' || c == 'r')
        {
            return 'R';
        }
    }

    return 0;
}

void setup()
{
    Serial.begin(115200);
    delay(500);

    Serial.println();
    Serial.println("========================================");
    Serial.println(" JWPLC ALPHA4 LOCAL PHYSICAL GATE");
    Serial.println(" P1-P8");
    Serial.println("========================================");
    Serial.println();
    Serial.println("Esperando inicializacion normal del");
    Serial.println("autoload JWPLC...");

    automaticStartMs = millis();
}

void loop()
{
    switch (gateStage)
    {
        case GATE_AUTOMATIC:
        {
            // El Display puede terminar de inicializarse desde
            // los hooks normales posteriores a setup().
            if (!automaticTestsExecuted)
            {
                if (JWPLC_Display.isReady() ||
                    (millis() - automaticStartMs) >= 8000)
                {
                    automaticTestsExecuted = true;

                    runAutomaticChecks();

                    gateStage = GATE_BUTTONS;
                }
            }

            break;
        }

        case GATE_BUTTONS:
        {
            runButtonStage();
            break;
        }

        case GATE_INPUTS:
        {
            runInputStage();
            break;
        }

        case GATE_OUTPUTS:
        {
            prepareOutputStage();
            runOutputSequence();

            if (outputSequenceDone)
            {
                const char command = readCommand();

                if (command == 'R')
                {
                    restartOutputSequence();
                }
                else if (command == 'Y')
                {
                    outputsOk = true;

                    printPassFail(
                        "ALPHA4_OUTPUTS",
                        true);

                    prepareDisplayConfirmation();

                    gateStage =
                        GATE_DISPLAY_CONFIRM;
                }
                else if (command == 'N')
                {
                    outputsOk = false;

                    printPassFail(
                        "ALPHA4_OUTPUTS",
                        false);

                    prepareDisplayConfirmation();

                    gateStage =
                        GATE_DISPLAY_CONFIRM;
                }
            }

            break;
        }

        case GATE_DISPLAY_CONFIRM:
        {
            const char command = readCommand();

            if (command == 'Y' ||
                command == 'N')
            {
                displayVisualOk =
                    command == 'Y';

                printPassFail(
                    "ALPHA4_DISPLAY_VISUAL",
                    displayVisualOk);

                gateStage = GATE_DONE;

                printFinalResult();
            }

            break;
        }

        case GATE_DONE:
        default:
        {
            break;
        }
    }

    delay(5);
}

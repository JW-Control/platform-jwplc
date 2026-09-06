#include <JWPLC_Display.h>

// =====================================================
// Alpha11 A11-3E - Gate físico de páginas
// =====================================================
// PAGE_SELECT:
//   LEFT / RIGHT -> cambia página
//   OK           -> entra a PAGE_CONTENT
//
// PAGE_CONTENT:
//   LEFT / RIGHT / UP / DOWN / OK -> disponibles para el usuario
//   ESC                           -> vuelve a PAGE_SELECT
//
// Indicador global NN/TT:
//   PAGE_SELECT  -> fondo negro / texto blanco
//   PAGE_CONTENT -> fondo blanco / texto negro
// =====================================================

enum HMIFieldId : uint8_t
{
    FIELD_P1_STATUS = 1,
    FIELD_P1_VALUE = 2,
    FIELD_P2_BOOL = 3,
    FIELD_P3_BAR = 4
};

static const JWPLC_UIField HMI_FIELDS[] =
{
    JWPLC_UITextField(
        FIELD_P1_STATUS,
        JWPLC_UIRect(18, 26),
        JWPLC_UIText("Pagina 01", nullptr, 10),
        JWPLC_UITextFieldStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_LEFT),
        0,
        JWPLC_UIColors(0xFFFF, 0x07FF, 0x0000, 0xFFFF)),

    JWPLC_UIValueField(
        FIELD_P1_VALUE,
        JWPLC_UIRect(18, 80),
        JWPLC_UIText("Valor", "u"),
        JWPLC_UIValueFormat(3, 1, true, false),
        JWPLC_UIValueStyle(
            2,
            1,
            false,
            JWPLC_UI_LAYOUT_INLINE,
            JWPLC_UI_ALIGN_RIGHT),
        0,
        JWPLC_UIColors(0xFFFF, 0x07E0, 0x0000, 0xFFFF)),

    JWPLC_UIBoolField(
        FIELD_P2_BOOL,
        JWPLC_UIRect(18, 42),
        JWPLC_UIText("Pagina 02", nullptr),
        JWPLC_UIBoolText("OFF", "ON"),
        JWPLC_UIBoolStyle(
            2,
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED,
            JWPLC_UI_ALIGN_CENTER),
        1,
        JWPLC_UIColors(0xFFFF, 0xFFE0, 0x0000, 0xFFFF)),

    JWPLC_UIBarField(
        FIELD_P3_BAR,
        JWPLC_UIRect(18, 42, 190, JWPLC_UI_AUTO),
        JWPLC_UIText("Pagina 03", "%"),
        JWPLC_UIRange(0.0f, 100.0f),
        JWPLC_UIBarStyle(
            1,
            true,
            JWPLC_UI_LAYOUT_STACKED),
        2,
        JWPLC_UIColors(0xFFFF, 0xFD20, 0x0000, 0xFFFF))
};

static uint8_t g_lastPage = 255;
static bool g_lastSelection = false;
static bool g_firstState = true;

static void printNavigationState()
{
    const uint8_t page = JWPLC_Display.userPage();
    const bool selecting = JWPLC_Display.isUserPageSelection();

    if (!g_firstState && page == g_lastPage && selecting == g_lastSelection)
    {
        return;
    }

    g_firstState = false;
    g_lastPage = page;
    g_lastSelection = selecting;

    Serial.printf(
        "[A11-PAGES] page=%u/%u mode=%s\n",
        (unsigned)(page + 1),
        (unsigned)JWPLC_Display.userPageCount(),
        selecting ? "SELECT" : "CONTENT");
}

static void printContentButtons()
{
    if (JWPLC_Display.isUserPageSelection())
    {
        return;
    }

    if (JWPLC_Buttons.pressed(BTN_LEFT))
        Serial.println("[A11-PAGES] USER LEFT");
    if (JWPLC_Buttons.pressed(BTN_RIGHT))
        Serial.println("[A11-PAGES] USER RIGHT");
    if (JWPLC_Buttons.pressed(BTN_UP))
        Serial.println("[A11-PAGES] USER UP");
    if (JWPLC_Buttons.pressed(BTN_DOWN))
        Serial.println("[A11-PAGES] USER DOWN");
    if (JWPLC_Buttons.pressed(BTN_OK))
        Serial.println("[A11-PAGES] USER OK");

    // ESC no se imprime: en PAGE_CONTENT pertenece al sistema HMI y debe
    // regresar al selector antes de llegar a la lógica del usuario.
}

void setup()
{
    Serial.begin(115200);
    delay(150);

    JWPLC_Display.setFields(
        HMI_FIELDS,
        sizeof(HMI_FIELDS) / sizeof(HMI_FIELDS[0]));

    JWPLC_Display.setUserPageCount(3);
    JWPLC_Display.setUserPage(0);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);

    JWPLC_Display.setText(FIELD_P1_STATUS, "READY");
    JWPLC_Display.setValue(FIELD_P1_VALUE, 25.6f);
    JWPLC_Display.setBool(FIELD_P2_BOOL, true);
    JWPLC_Display.setBar(FIELD_P3_BAR, 72.0f);

    JWPLC_Display.enterUserUI();

    Serial.println("[A11-PAGES] Gate listo");
    Serial.println("[A11-PAGES] SELECT: LEFT/RIGHT cambian pagina, OK entra");
    Serial.println("[A11-PAGES] CONTENT: flechas/OK usuario, ESC sale");
    printNavigationState();
}

void loop()
{
    printNavigationState();
    printContentButtons();
    delay(5);
}

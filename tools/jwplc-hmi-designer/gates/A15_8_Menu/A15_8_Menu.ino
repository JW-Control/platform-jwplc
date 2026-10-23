// Gate A15-8: MENU declarativo de JWPLC_Display (escrito a mano, sin Designer).
//
// Página 1: un VALUE con la temperatura simulada.
// Página 2: MENU "Ajustes" con 3 niveles:
//   Ajustes
//   ├─ Temperatura   (VIEW float, cambia sola)
//   ├─ Consigna      (EDIT_NUM float 0..100 paso 0.5)
//   ├─ Bomba         (EDIT_BOOL PARADA/MARCHA)
//   └─ Avanzado >
//       ├─ Ciclos    (VIEW int32)
//       ├─ Retardo   (EDIT_NUM int32 0..600 s)
//       └─ Alarmas >
//           ├─ Alta  (EDIT_NUM float)
//           └─ Habilitada (EDIT_BOOL)
//
// Prueba en la placa: LEFT/RIGHT elige la página 02, OK entra (indicador
// sombreado), UP/DOWN mueve, OK edita/confirma, ESC cancela/sube, ESC en la
// raíz vuelve al selector de páginas. Serial (115200) imprime cada cambio.
#include <JWPLC_Display.h>

float temperatura = 21.5f;
float consigna = 45.0f;
bool bomba = false;
int32_t ciclos = 0;
int32_t retardo = 30;
float alarmaAlta = 80.0f;
bool alarmaHabilitada = true;

enum : uint8_t
{
    FIELD_TEMP = 0
};

enum : uint8_t
{
    MENU_AJUSTES = 0
};

enum : uint8_t
{
    N_TEMP = 0,
    N_CONSIGNA,
    N_BOMBA,
    N_AVANZADO,
    N_CICLOS,
    N_RETARDO,
    N_ALARMAS,
    N_ALTA,
    N_HABILITADA
};

static const JWPLC_UIField fields[] = {
    JWPLC_UIValueField(
        FIELD_TEMP,
        JWPLC_UIRect(20, 60),
        JWPLC_UIText("Temp", "C"),
        JWPLC_UIValueFormat(3, 1, true, false),
        JWPLC_UIValueStyle(3, 1, true),
        0)};

static const JWPLC_UIMenuNode ajustesNodes[] = {
    JWPLC_UIMenuView(JWPLC_UI_MENU_ROOT, "Temperatura", &temperatura, 1, "C"),
    JWPLC_UIMenuEditNum(JWPLC_UI_MENU_ROOT, "Consigna", &consigna, 0.0f, 100.0f, 0.5f, 1, "C"),
    JWPLC_UIMenuEditBool(JWPLC_UI_MENU_ROOT, "Bomba", &bomba, "PARADA", "MARCHA"),
    JWPLC_UIMenuSubmenu(JWPLC_UI_MENU_ROOT, "Avanzado"),
    JWPLC_UIMenuView(N_AVANZADO, "Ciclos", &ciclos),
    JWPLC_UIMenuEditNum(N_AVANZADO, "Retardo", &retardo, 0, 600, 5, "s"),
    JWPLC_UIMenuSubmenu(N_AVANZADO, "Alarmas"),
    JWPLC_UIMenuEditNum(N_ALARMAS, "Alta", &alarmaAlta, 0.0f, 150.0f, 1.0f, 0, "C"),
    JWPLC_UIMenuEditBool(N_ALARMAS, "Habilitada", &alarmaHabilitada, "NO", "SI")};

static const JWPLC_UIMenu menus[] = {
    JWPLC_UIMenuField(
        MENU_AJUSTES,
        10, 20, 300, 140,
        "Ajustes",
        ajustesNodes,
        sizeof(ajustesNodes) / sizeof(ajustesNodes[0]),
        1)};

void setup()
{
    Serial.begin(115200);
    JWPLC_Display.setFields(fields, sizeof(fields) / sizeof(fields[0]));
    const bool ok = JWPLC_Display.setMenus(menus, sizeof(menus) / sizeof(menus[0]));
    Serial.printf("setMenus=%s\n", ok ? "OK" : "FAIL");
    JWPLC_Display.setUserPageCount(2);
    JWPLC_Display.setUserRefreshMode(USER_REFRESH_ON_DEMAND);
}

void loop()
{
    static uint32_t last = 0;
    if (millis() - last >= 500)
    {
        last = millis();
        temperatura += bomba ? -0.3f : 0.2f;
        ciclos++;
        JWPLC_Display.setValue(FIELD_TEMP, temperatura);
    }

    uint8_t node = 0;
    if (JWPLC_Display.menuChanged(MENU_AJUSTES, &node))
    {
        Serial.printf("menu: nodo %u -> consigna=%.1f bomba=%d retardo=%ld alta=%.0f hab=%d\n",
                      node, consigna, bomba, (long)retardo, alarmaAlta, alarmaHabilitada);
    }
}

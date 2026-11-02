// Interfaz del Runtime en la TFT (modo USER de JWPLC_Display).
//
// Pila de pantallas: el menú raíz abajo y, encima, submenús, el editor FBD
// y los diálogos. Cada pantalla dibuja solo lo que cambió.
// Fuentes (regla del propietario): títulos y símbolos en GLCD (x2), filas y
// mensajes en JWPLC_FONT_16. Barra de título gris, como la IDLE.
#pragma once

#include <JWPLC_Display.h>
#include <JWPLC_UI_Widgets.h>

#include "../rt/RtCore.h"

namespace rtui
{

using namespace jwrt;

// ---------------------------------------------------------------- geometría
static const int16_t SCREEN_W = 320;
static const int16_t SCREEN_H = 170;
static const int16_t BAR_H = 22;     // barra de título
static const int16_t FOOT_Y = 156;   // pie (ayuda y mensajes)
static const int16_t FOOT_H = 14;
static const int16_t BODY_Y = BAR_H + 1;
static const int16_t BODY_H = FOOT_Y - BODY_Y - 1;

// Colores
extern JWPLC_UITheme T;
static const uint16_t C_ON = 0x07E0;      // señal en 1
static const uint16_t C_OFF = 0x4A69;     // señal en 0
static const uint16_t C_SEL = 0xFFE0;     // selección del editor
static const uint16_t C_RUN = 0x07E0;
static const uint16_t C_STOP = 0xFFE0;
static const uint16_t C_TILE_SEL = 0x0200; // fondo de la baldosa elegida

// ---------------------------------------------------------------- pantallas
class Screen
{
public:
    virtual ~Screen() {}
    virtual void enter() = 0;   // dibuja todo
    virtual void update() = 0;  // teclas y dirty refresh
    virtual void resume() { enter(); } // al volver de una pantalla de encima
    virtual bool modal() const { return false; } // se dibuja encima de la anterior
};

void begin();
void push(Screen *s);
void pop();
void popToRoot();
void popTo(Screen *s); // quita pantallas hasta que s quede arriba
Screen *current();

// La UI pide volver a la pantalla RUN; la tarea llama goIdle() fuera del batch.
bool takeIdleRequest();
void requestIdle(); // como ESC en el menú raíz: vuelve a la pantalla RUN
extern volatile bool editing; // editor abierto: no vuelve solo a RUN
uint32_t lastKeyMs();

// ---------------------------------------------------------------- dibujo
void drawStatusBar(const char title[]);
void refreshStatusBar(bool force = false); // chip RUN/STOP y reloj
void drawFooter(const char help[]);
void footerMessage(const char text[], uint16_t color, uint32_t ms = 2000);
void serviceFooter();
void clearBody();
void text16(const char t[], int16_t x, int16_t y, uint16_t fg, uint16_t bg);
void textGlcd(const char t[], int16_t x, int16_t y, uint16_t fg, uint16_t bg, uint8_t size = 1);

bool key(uint8_t id); // pressed() con registro de actividad

// ---------------------------------------------------------------- órdenes
// Envía la orden a la tarea; mientras dura, el pie dice "Procesando...".
// Al terminar muestra el resultado y llama onDone(ok).
void runCommand(Command cmd, const CommandArgs &args, void (*onDone)(bool ok) = nullptr);
void runCommand(Command cmd, void (*onDone)(bool ok) = nullptr);
bool commandPending();
void serviceCommand();

// ---------------------------------------------------------------- modales
void confirm(const char title[], const char message[], void (*onYes)(), void (*onNo)() = nullptr);
void info(const char title[], const char message[]);
void editValue(const char title[], int32_t value, uint8_t digits, uint8_t decimals,
               const char unit[], int32_t minValue, int32_t maxValue, void (*onDone)(int32_t));
void editText(const char title[], const char initial[], uint8_t maxLen, void (*onDone)(const char[]));
// Pide el PIN si está activo y la sesión no es Admin.
void requireAdmin(void (*onOk)());

// ---------------------------------------------------------------- listas
struct ListSpec
{
    const char *title;
    const char *help;
    uint16_t (*count)();
    JWPLC_UIRowText row;
    void (*ok)(uint16_t index);
    void (*side)(uint16_t index, int8_t dir); // ◄ ► (opcional)
    void (*tick)();                           // cada ciclo (opcional)
    uint16_t refreshMs;                       // redibujo periódico de filas (0 = no)
};
void pushList(const ListSpec &spec, uint16_t selected = 0);
void refreshList(); // la lista de arriba vuelve a leer filas y cantidad
uint16_t listSelected();

// ---------------------------------------------------------------- menús
void pushRootMenu();
void openProgramMenu();
void openParams();
void openSdMenu();
void openConfigMenu();
void openExpansionMenu();
void openNetMenu();
void openDiagMenu();

// Editor FBD (monitor = solo ver).
void openEditor(bool monitor);

// ---------------------------------------------------------------- página HMI
// Páginas del HMI Designer (proyecto «Runtime»), registradas con
// JWPLC_Runtime.setHMI() desde el header generado (RtUIHMI.cpp).
void setHMI(const JWPLC_UIField fields[], uint8_t count, uint8_t pages);
bool hasHMI();
uint8_t hmiPageCount();
bool hmiActive();          // una página HMI está en pantalla
void openHMI(uint8_t page); // encima de lo que haya (normalmente el menú raíz)
void releaseHMI();         // quita los campos de JWPLC_Display

} // namespace rtui

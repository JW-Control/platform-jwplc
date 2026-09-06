#ifndef JWPLC_UI_RUNTIME_HOOKS_H
#define JWPLC_UI_RUNTIME_HOOKS_H

#include <Arduino.h>

class Adafruit_ST7789;

// Hooks internos entre el runtime base del Display y el motor HMI Alpha11.
//
// JWPLC_Display.cpp mantiene implementaciones weak/no-op para que un sketch que
// no use la HMI de campos no arrastre JWPLC_UI.cpp desde el archive
// precompilado. Cuando la API HMI se usa, JWPLC_UI_API.cpp aporta las
// implementaciones strong y enlaza el motor completo sin cambiar la API
// publica JWPLC_Display.*.
#ifdef __cplusplus
extern "C"
{
#endif

// Se ejecuta siempre en USER antes de decidir si hace falta dibujar. Permite
// procesar navegación física aunque el refresh haya sido forzado por un botón.
void jwplcUIRuntimeServiceInput(void);

bool jwplcUIRuntimeRefreshNeeded(void);
void jwplcUIRuntimeInvalidateAll(bool redrawStatic);
void jwplcUIRuntimePrepareEnter(void);
uint8_t jwplcUIRuntimeCurrentPage(void);
bool jwplcUIRuntimePageRedrawPending(void);
void jwplcUIRuntimeConsumePageRedrawPending(void);
void jwplcUIRuntimeConsumeRefreshRequest(void);
void jwplcUIRuntimeDrawStatic(Adafruit_ST7789 *tft);
void jwplcUIRuntimeDrawDirty(Adafruit_ST7789 *tft);

#ifdef __cplusplus
}
#endif

#endif // JWPLC_UI_RUNTIME_HOOKS_H

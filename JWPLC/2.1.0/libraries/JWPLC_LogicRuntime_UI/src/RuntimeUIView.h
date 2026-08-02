#ifndef JWPLC_LOGIC_RUNTIME_UI_VIEW_H
#define JWPLC_LOGIC_RUNTIME_UI_VIEW_H

#include <Arduino.h>

enum class RuntimeUIView : uint8_t
{
  None = 0,
  Home,
  Program,
  Diagram,
  Blocks,
  Storage,
  Diagnostics
};

#endif
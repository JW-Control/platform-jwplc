#include "RuntimeUIFBDMapV12.h"

#include <cstdio>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

RuntimeUIFBDMapV12::RuntimeUIFBDMapV12()
    : RuntimeUIFBDMapV11(),
      _parameterListTop(0)
{
}

void RuntimeUIFBDMapV12::resetV12State()
{
  _parameterListTop = 0;
}

void RuntimeUIFBDMapV12::attach(RuntimeUIV2ReadModel &model)
{
  resetV12State();
  RuntimeUIFBDMapV11::attach(model);
}

void RuntimeUIFBDMapV12::enter()
{
  resetV12State();
  RuntimeUIFBDMapV11::enter();
  suppressCompactAddPreviewV11();
}

void RuntimeUIFBDMapV12::forceRedraw()
{
  RuntimeUIFBDMapV11::forceRedraw();
  suppressCompactAddPreviewV11();
}

void RuntimeUIFBDMapV12::refresh(const JWPLC_IOState *io,
                                 const JWPLC_RTCState *rtc)
{
  if (!normalMapRootActiveV11())
  {
    RuntimeUIFBDMapV11::refresh(io, rtc);
    return;
  }

  // RIGHT se consume antes del mapa base y abre el nodo + completo en su
  // columna virtual. Para el mapa normal se salta V8/V9/V10, eliminando desde
  // el flujo activo cualquier llamada a drawAddPreview().
  suppressCompactAddPreviewV11();
  if (interceptAddRightPressForExtension())
  {
    suppressCompactAddPreviewV11();
    return;
  }

  RuntimeUIFBDMapV7::refresh(io, rtc);
  suppressCompactAddPreviewV11();
}

bool RuntimeUIFBDMapV12::canReturnToIdle() const
{
  // Las subpantallas deben recibir ESC antes que el router global. Solo la raíz
  // del mapa conserva el comportamiento histórico de retorno a IDLE.
  return normalMapRootActiveV11();
}

bool RuntimeUIFBDMapV12::handleMainInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    returnConfigureToTypeV11();
    return true;
  }

  const bool left = JWPLC_Buttons.pressed(BTN_LEFT);
  const bool right = !left && JWPLC_Buttons.pressed(BTN_RIGHT);
  if (left || right)
  {
    const MainFocus previous = _mainFocus;
    moveMainFocus(right);
    JWPLC_Display.notifyActivity();
    drawMainGroup(previous);
    drawMainGroup(_mainFocus);
    drawFixedContextForCurrent();
    drawConfigFooter(_mainFocus == MainFocus::Create
                         ? "OK CREAR   ESC ATRAS"
                         : "OK ABRIR   ESC ATRAS");
    return true;
  }

  if (!JWPLC_Buttons.pressed(BTN_OK))
  {
    return false;
  }

  JWPLC_Display.notifyActivity();
  if (_mainFocus == MainFocus::Create)
  {
    restoreWizardRepeatProfileV11();
    requestWizardCreateV11();
    return true;
  }

  if (_mainFocus == MainFocus::Sources)
  {
    _sourceInputIndex = 0;
    if (sourceInputCount() > 1)
    {
      _configView = ConfigView::SourceList;
      drawSourceListScreen();
    }
    else
    {
      beginSourceEdit();
    }
    gateInputUntilRelease(false);
    return true;
  }

  _parameterIndex = 0;
  _parameterListTop = 0;

  // Una lista de un solo elemento no aporta navegación. TON, DI y DO abren
  // directamente su editor actual. La lista queda reservada para bloques con
  // dos o más parámetros.
  if (parameterCount() <= 1)
  {
    beginParameterEdit();
  }
  else
  {
    _configView = ConfigView::ParameterList;
    drawParameterListScreen();
  }

  gateInputUntilRelease(false);
  return true;
}

void RuntimeUIFBDMapV12::ensureParameterListVisible()
{
  const uint8_t count = parameterCount();
  if (count <= PARAMETER_ROWS_VISIBLE)
  {
    _parameterListTop = 0;
    return;
  }

  if (_parameterIndex < _parameterListTop)
  {
    _parameterListTop = _parameterIndex;
  }
  else if (_parameterIndex >=
           static_cast<uint8_t>(_parameterListTop + PARAMETER_ROWS_VISIBLE))
  {
    _parameterListTop = static_cast<uint8_t>(
        _parameterIndex - PARAMETER_ROWS_VISIBLE + 1U);
  }

  const uint8_t maximumTop =
      static_cast<uint8_t>(count - PARAMETER_ROWS_VISIBLE);
  if (_parameterListTop > maximumTop)
  {
    _parameterListTop = maximumTop;
  }
}

void RuntimeUIFBDMapV12::drawParameterPosition()
{
  char position[8];
  const uint8_t count = parameterCount();
  std::snprintf(position,
                sizeof(position),
                "%02u/%02u",
                static_cast<unsigned>(count == 0 ? 0 : _parameterIndex + 1U),
                static_cast<unsigned>(count));

  updateTextField(JWPLC_Display.tft(),
                  PARAMETER_POSITION_X,
                  PARAMETER_POSITION_Y,
                  PARAMETER_POSITION_COLUMNS,
                  position,
                  COLOR_WARNING,
                  COLOR_PANEL);
}

void RuntimeUIFBDMapV12::drawParameterListRow(uint8_t parameterIndex,
                                              bool selected)
{
  if (parameterIndex < _parameterListTop ||
      parameterIndex >= static_cast<uint8_t>(
                            _parameterListTop + PARAMETER_ROWS_VISIBLE) ||
      parameterIndex >= parameterCount())
  {
    return;
  }

  char value[24];
  char label[44];
  formatParameterValue(parameterIndex, value, sizeof(value));
  std::snprintf(label,
                sizeof(label),
                "%s  <%s>",
                parameterName(parameterIndex),
                value);

  const uint8_t visibleRow =
      static_cast<uint8_t>(parameterIndex - _parameterListTop);
  drawMenuButton(JWPLC_Display.tft(),
                 LIST_X,
                 static_cast<int16_t>(LIST_Y + visibleRow * LIST_STEP),
                 LIST_W,
                 LIST_ROW_H,
                 label,
                 selected);
}

void RuntimeUIFBDMapV12::drawParameterListScreen()
{
  ensureParameterListVisible();

  char subtitle[30];
  std::snprintf(subtitle,
                sizeof(subtitle),
                "PARAMETROS: %s",
                wizardTypeLabelV11());
  drawConfigHeader(subtitle);
  drawParameterPosition();

  const uint8_t count = parameterCount();
  const uint8_t end = static_cast<uint8_t>(
      _parameterListTop + PARAMETER_ROWS_VISIBLE < count
          ? _parameterListTop + PARAMETER_ROWS_VISIBLE
          : count);
  for (uint8_t parameter = _parameterListTop; parameter < end; ++parameter)
  {
    drawParameterListRow(parameter, parameter == _parameterIndex);
  }

  drawConfigFooter("UP/DOWN   OK EDITAR   ESC ATRAS");
}

bool RuntimeUIFBDMapV12::handleParameterListInput()
{
  if (JWPLC_Buttons.pressed(BTN_ESC))
  {
    _configView = ConfigView::Main;
    JWPLC_Display.notifyActivity();
    drawMainScreen();
    gateInputUntilRelease(false);
    return true;
  }

  if (JWPLC_Buttons.pressed(BTN_OK))
  {
    JWPLC_Display.notifyActivity();
    beginParameterEdit();
    gateInputUntilRelease(false);
    return true;
  }

  const bool up = JWPLC_Buttons.pressed(BTN_UP);
  const bool down = !up && JWPLC_Buttons.pressed(BTN_DOWN);
  if (!up && !down)
  {
    return false;
  }

  const uint8_t count = parameterCount();
  if (count == 0)
  {
    return false;
  }

  const uint8_t previous = _parameterIndex;
  const uint8_t previousTop = _parameterListTop;
  _parameterIndex = up
                        ? (_parameterIndex == 0
                               ? static_cast<uint8_t>(count - 1U)
                               : static_cast<uint8_t>(_parameterIndex - 1U))
                        : static_cast<uint8_t>((_parameterIndex + 1U) % count);
  ensureParameterListVisible();
  JWPLC_Display.notifyActivity();

  if (previousTop != _parameterListTop)
  {
    // El viewport cambió: el redibujado completo está permitido por cambio de
    // página/layout y evita residuos de filas que dejaron de ser visibles.
    drawParameterListScreen();
    return true;
  }

  drawParameterListRow(previous, false);
  drawParameterListRow(_parameterIndex, true);
  drawParameterPosition();
  return true;
}

void RuntimeUIFBDMapV12::returnFromParameterEdit(bool accept)
{
  restoreWizardRepeatProfileV11();
  if (!accept)
  {
    restoreParameterBackupV11();
  }

  JWPLC_Display.notifyActivity();
  gateInputUntilRelease(false);

  if (parameterCount() <= 1)
  {
    _configView = ConfigView::Main;
    drawMainScreen();
    return;
  }

  _configView = ConfigView::ParameterList;
  ensureParameterListVisible();
  drawParameterListScreen();
}
